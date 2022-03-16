#include "config.h"

#include <string>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <openframe/openframe.h>
#include <stomp/StompHeaders.h>
#include <stomp/StompFrame.h>
#include <stomp/Stomp.h>
#include <aprs/aprs.h>

#include <Decay.h>
#include <Worker.h>
#include <Store.h>
#include <MemcachedController.h>

namespace aprscreate {
  using namespace openframe::loglevel;

  const int Worker::kDefaultStompPrefetch		= 1024;
  const time_t Worker::kDefaultStatsInterval		= 3600;
  const time_t Worker::kDefaultMemcachedExpire		= 3600;
  const char *Worker::kDefaultStompDestFeedsAprsIs	= "/queue/feeds.aprs.is";
  const char *Worker::kDefaultStompDestPushAprs		= "/queue/push.aprs.is";
  const char *Worker::kDefaultStompDestNotifyMessages	= "/topic/notify.aprs.messages";
  const char *Worker::kDefaultAprsDest			= "APOA00";
  const time_t Worker::kDefaultDecayRetry		= 15;
  const time_t Worker::kDefaultDecayTimeout		= 900;
  const time_t Worker::kDefaultSessionExpire		= 300;
  const char *Worker::kDefaultDigiList			= "TCPIP*,qAC";


  Worker::Worker(const openframe::LogObject::thread_id_t thread_id,
                 const std::string &stomp_hosts,
                 const std::string &stomp_login,
                 const std::string &stomp_passcode,
                 const std::string &memcached_host,
                 const std::string &db_host,
                 const std::string &db_user,
                 const std::string &db_pass,
                 const std::string &db_database)
         : openframe::LogObject(thread_id),
           _stomp_hosts(stomp_hosts),
           _stomp_login(stomp_login),
           _stomp_passcode(stomp_passcode),
           _memcached_host(memcached_host),
           _db_host(db_host),
           _db_user(db_user),
           _db_pass(db_pass),
           _db_database(db_database),
           _aprs_dest(kDefaultAprsDest),
           _decay_retry(kDefaultDecayRetry),
           _decay_timeout(kDefaultDecayTimeout),
           _session_expire(kDefaultSessionExpire) {

    _store = NULL;
    _stomp = NULL;
    _decay = NULL;
    _connected = false;
    _console = false;
    _no_send = false;

    _stomp_dest_feeds_aprs_is = kDefaultStompDestFeedsAprsIs;
    _stomp_dest_push_aprs = kDefaultStompDestPushAprs;
    _stomp_dest_notify_msgs = kDefaultStompDestNotifyMessages;

    _create_timer.last_try_at = time(NULL);
    _create_timer.try_interval = 2;

    _callsign = "";
    _digis = kDefaultDigiList;

    init_stats(_stats, true);
    init_stompstats(_stompstats, true);
    _stats.report_interval = 60;
    _stompstats.report_interval = 5;
  } // Worker::Worker

  Worker::~Worker() {
    onDestroyStats();

    if (_store) delete _store;
    if (_stomp) delete _stomp;
  } // Worker:~Worker

  void Worker::init() {
    try {
      stomp::StompHeaders *headers = new stomp::StompHeaders("openstomp.prefetch",
                                                             openframe::stringify<int>(kDefaultStompPrefetch)
                                                            );
      headers->add_header("heart-beat", "0,5000");
      _stomp = new stomp::Stomp(_stomp_hosts,
                                _stomp_login,
                                _stomp_passcode,
                                headers);

      _store = new Store(thread_id(),
                         _db_host,
                         _db_user,
                         _db_pass,
                         _db_database,
                         _memcached_host,
                         kDefaultMemcachedExpire,
                         kDefaultStatsInterval);
      _store->replace_stats( stats(), "");
      _store->set_elogger( elogger(), elog_name() );
      _store->init();

      _decay = new Decay( thread_id() );
      _decay->set_elogger( elogger(), elog_name() );
    } // try
    catch(std::bad_alloc &xa) {
      assert(false);
    } // catch
  } // Worker::init

  void Worker::init_stats(obj_stats_t &stats, const bool startup) {
    stats.connects = 0;
    stats.disconnects = 0;
    stats.packets = 0;
    stats.frames_in = 0;
    stats.frames_out = 0;

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // Worker::init_stats

  void Worker::init_stompstats(obj_stompstats_t &stats, const bool startup) {
    memset(&stats.aprs_stats, '\0', sizeof(aprs_stats_t) );

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // Worker::init_stompstats

  void Worker::onDescribeStats() {
    describe_stat("num.frames.out", "worker"+ openframe::stringify<int>( thread_id() ) +"/num frames out", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.frames.in", "worker"+ openframe::stringify<int>( thread_id() )+"/num frames in", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.bytes.out", "worker"+ openframe::stringify<int>( thread_id() )+"/num bytes out", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.bytes.in", "worker"+ openframe::stringify<int>( thread_id() )+"/num bytes in", openstats::graphTypeCounter, openstats::dataTypeInt);
  } // Worker::onDescribeStats

  void Worker::onDestroyStats() {
    destroy_stat("*");
  } // Worker::onDestroyStats

  void Worker::try_stats() {
    try_stompstats();

    if (_stats.last_report_at > time(NULL) - _stats.report_interval) return;

    int diff = time(NULL) - _stats.last_report_at;
    double pps = double(_stats.packets) / diff;
    double fps_in = double(_stats.frames_in) / diff;
    double fps_out = double(_stats.frames_out) / diff;

    TLOG(LogNotice, << "Stats packets " << _stats.packets
                    << ", pps " << pps << "/s"
                    << ", frames in " << _stats.frames_in
                    << ", fps in " << fps_in << "/s"
                    << ", frames out " << _stats.frames_out
                    << ", fps out " << fps_out << "/s"
                    << ", next in " << _stats.report_interval
                    << ", connect attempts " << _stats.connects
                    << "; " << _stomp->connected_to()
                    << std::endl);

    init_stats(_stats);
    _stats.last_report_at = time(NULL);
  } // Worker::try_stats

  void Worker::try_stompstats() {
    if (_stompstats.last_report_at > time(NULL) - _stompstats.report_interval) return;

    init_stompstats(_stompstats);
  } // Worker::try_stompstats

  bool Worker::run() {
    try_stats();
    _store->try_stats();

    /**********************
     ** Check Connection **
     **********************/
    if (!_connected) {
      ++_stats.connects;
      bool ok = _stomp->subscribe(_stomp_dest_notify_msgs, "1");
      if (!ok) {
        TLOG(LogInfo, << "not connected, retry in 2 seconds; " << _stomp->last_error() << std::endl);
        return false;
      } // if
      _connected = true;
      TLOG(LogNotice, << "Connected to " << _stomp->connected_to() << std::endl);
    } // if

    stomp::StompFrame *frame;
    bool ok = false;

    if (_create_timer.last_try_at < time(NULL) - _create_timer.try_interval) {
      handle_decays();
      create_messages();
      create_objects();
      create_positions();
      _create_timer.last_try_at = time(NULL);
    } // if

    try {
      ok = _stomp->next_frame(frame);
    } // try
    catch(stomp::Stomp_Exception &ex) {
      TLOG(LogWarn, << "ERROR: " << ex.message() << std::endl);
      _connected = false;
      ++_stats.disconnects;
      return false;
    } // catch

    if (!ok) return false;

    /*******************
     ** Process Frame **
     *******************/
    ++_stats.frames_in;
    bool is_usable = frame->is_command(stomp::StompFrame::commandMessage)
                     && frame->is_header("message-id");
    if (!is_usable) {
      frame->release();
      return true;
    } // if
    ++_stats.frames_in;

   TLOG(LogDebug, << "received message; "
                  << frame->body()
                  << std::endl);

    process_message( frame->body() );

    std::string message_id = frame->get_header("message-id");
    _stomp->ack(message_id, "1");

    frame->release();
    return true;
  } // Worker::run

  bool Worker::push_aprs(const std::string &body) {
    if (_no_send) {
      TLOG(LogWarn, << "send{no} "
                    << body
                    << std::endl);
      return true;
    } // if

    std::stringstream s;
    s << time(NULL) << " " << body << std::endl;
    _stomp->send(_stomp_dest_feeds_aprs_is, s.str());
    return _stomp->send(_stomp_dest_push_aprs, body+"\n");
  } // Worker::push_aprs

  bool Worker::process_message(const std::string &body) {
    openframe::Vars *v = new openframe::Vars(body);

    // if this isn't a message we can ack then we don't
    // care
    bool ok = v->is("sr,to,ms,pa");
    if (!ok) {
      delete v;
      return false;
    } // if

    process_message_t pm;
    pm.source = openframe::StringTool::toUpper( v->get("sr") );
    pm.target = openframe::StringTool::toUpper( v->get("to") );
    pm.body = v->get("ms");
    pm.path = v->get("pa");
    pm.is_to_me = is_callsign()
                  && (pm.target == _callsign ? true : false);
    pm.id = v->get("id");
    pm.ack = v->get("ack");
    pm.reply_id = v->get("rpl");
    pm.is_ackonly = v->is("ao");
    pm.v = v;

    event_message_to_me(pm);
    event_message_ack(pm);
    event_message_verify(pm);

    delete v;
    return true;
  } // Worker::process_message

  bool Worker::event_message_verify(process_message_t &pm) {
    // is to me?
    if (!pm.is_to_me) return false;

    TLOG(LogInfo, << "verify{rcv} Received message '"
                  << pm.body
                  << "' from "
                  << pm.source
                  << std::endl);

    // Only process RF verifications, ignore anything from TCPIP only.
    openframe::StringTool::regexMatchListType rl;
    bool ok = openframe::StringTool::ereg("TCP(IP|XX)", pm.path, rl);
    if (ok) {
      TLOG(LogInfo, << "verify{fail} Cannot verify "
                    << pm.source
                    << " path was from internet; "
                    << pm.path
                    << std::endl);
      return false;
    } // if

    // Does this packet have a valid verification message?
    ok = openframe::StringTool::ereg("^(K([0-9]+)([A-Z0-9]{8}))[ ]*$",
                                     openframe::StringTool::toUpper(pm.body),
                                     rl);
    if (!ok) {
      TLOG(LogInfo, << "verify{fail} Cannot verify "
                    << pm.source
                    << " invalid key; "
                    << pm.body
                    << std::endl);
      return false;
    } // if

    std::string verifyId = rl[2].matchString;
    std::string verifyKey = rl[3].matchString;

    Store::verifyStatusEnum ret = _store->tryVerify(verifyId, pm.source, verifyKey);

    std::stringstream s;
    s << ret;
    if (ret != Store::verifyStatusIgnoredResend) {
      aprs::Message *msg = new aprs::Message(_callsign, _aprs_dest, pm.source, s.str() );
      msg->add_digis(_digis);
      push_aprs( msg->compile() );
      delete msg;
    } // if

    TLOG(LogNotice, << "Verify: "
                    << pm.source
                    << "@id("
                    << verifyId
                    << ") using '"
                    << verifyKey
                    << "' "
                    << s.str()
                    << std::endl);

    return true;
  } // Worker::event_message_verify

  bool Worker::event_message_to_me(process_message_t &pm) {
    bool ok = is_callsign()
              && pm.id.length();
    if (!ok) return false;

    // no ack, wasn't to me
    if (!pm.is_to_me) return false;

    // was ack only no reply
    if (pm.is_ackonly) return false;

    TLOG(LogInfo, << "Sending ack("
                  << pm.id
                  << ") to '"
                  << pm.source
                  << "' from '"
                  << _callsign
                  << "'"
                  << std::endl);

    std::stringstream s;
    s << "ack" << pm.id;

    aprs::Message *msg = new aprs::Message(_callsign, _aprs_dest, pm.source, s.str() );
    msg->add_digis(_digis);
    push_aprs( msg->compile() );
    delete msg;

    return true;
  } // Worker::event_message_to_me

  bool Worker::event_message_ack(process_message_t &pm) {
    bool ok = !pm.is_to_me
              && pm.reply_id.length()
              && pm.ack.length();
    if (ok) return false;

    std::string buf;
    bool found = _store->getAckFromMemcached(pm.v->get("sr"), buf);

    if (!found) {
      openframe::Stopwatch sw;
      sw.Start();
      bool isSession = _store->isUserSession(pm.target, time(NULL) - _session_expire);
      TLOG((sw.Time() > 1 ? LogWarn : LogDebug),
                    << "ack{session} "
                     << pm.target
                     << " is"
                     << (isSession ? "" : " not")
                     << " in a session "
                     << int(sw.Time()*1000)
                     << "ms"
                     << std::endl);
      if (isSession) {
        found = true;
        _store->setAckInMemcached(pm.v->get("sr"), buf, _session_expire);
      } // if
    } // if

    // if no user session don't ack
    if (!found) return false;

    // First find out if we have a user session for
    // the target user.  If we don't then don't bother to
    // go any further.
    std::stringstream s;
    s << "ack" << pm.reply_id;

    aprs::Message *msg = new aprs::Message(_callsign, _aprs_dest, pm.source, s.str() );
    msg->add_digis(_digis);
    push_aprs( msg->compile() );
    delete msg;

    // If this is an ack from a web user session message then we
    // need to store it and stop retrying that users message.
    if ( pm.ack.length() ) {
      // remove any decay for this message
      std::string decayId;
      if (_store->getMessageDecayId(pm.target, pm.source, pm.ack, decayId))
        _decay->remove(decayId);

      if (_store->setMessageAck(pm.source, pm.target, pm.ack) ) {
        TLOG(LogInfo, << "*** Received ack("
                      << pm.ack
                      << ") for '"
                      << pm.target
                      << "' from '"
                      << pm.source
                      << "'"
                      << std::endl);
      } // if
    } // if

    return true;
  } // Worker::event_message_ack

  /**********************
   ** Create Positions **
   **********************/
  struct aprs_position_t {
    bool local;
    std::string decay_id;
    std::string status;
    std::string source;
    std::string overlay;
    std::string title;
    char symbol_table, symbol_code;
    double latitude, longitude, altitude, speed;
    int course;
    unsigned int id;
    time_t broadcast_ts;
  }; // aprs_position_t

  void Worker::handle_decays() {
    Decay::decayStringsType decayStrings;

    _decay->next(decayStrings);
    while(!decayStrings.empty()) {
      push_aprs( decayStrings.front() );
      // FIXME: must send back through aprsinject to get into openaprs db
      decayStrings.pop_front();
    } // while
  } // Worker::handle_decays

  unsigned int Worker::create_positions() {
    openframe::DBI::resultType res;
    openframe::DBI::resultSizeType num_rows = _store->getPendingPositions(res);

    if (!num_rows) return 0;

    unsigned int num_created = 0;
    for(openframe::DBI::resultSizeType i=0; i < res.num_rows(); i++) {
      aprs_position_t p;

      // initialize variables
      p.speed = p.altitude = 0.0;
      p.course = 0;

      res[i]["source"].to_string(p.source);
      p.latitude = atof(res[i]["latitude"].c_str());
      p.longitude = atof(res[i]["longitude"].c_str());
      p.symbol_table = *res[i]["symbol_table"].c_str();
      p.symbol_code = *res[i]["symbol_code"].c_str();
      p.local = (*res[i]["local"].c_str() == 'Y') ? true : false;

      if ( !res[i]["speed"].is_null() )
        p.speed = atof(res[i]["speed"].c_str());

      if ( !res[i]["course"].is_null() )
        p.course = int(atof(res[i]["course"].c_str()));

      if ( !res[i]["altitude"].is_null() )
        p.altitude = atof( res[i]["altitude"].c_str() );

      if ( !res[i]["status"].is_null() )
        res[i]["status"].to_string(p.status);
      else
        p.status = "";

      int id = atoi( res[i]["id"].c_str() );
      aprs::Position *pos;
      try {
        std::string source, status;
        res[i]["source"].to_string(source);
        res[i]["status"].to_string(status);
        pos = new aprs::Position(p.source,
                                 _aprs_dest,
                                 p.latitude,
                                 p.longitude,
                                 p.symbol_table,
                                 p.symbol_code,
                                 p.course,
                                 p.speed,
                                 p.altitude,
                                 0,
                                 status);
        pos->add_digis(_digis);
      } // try
      catch(aprs::APRS_Exception &ex) {
        TLOG(LogWarn, << "could not create position; "
                      << ex.message()
                      << std::endl);

        // Don't keep trying to create the same object over and over
        // if an error occurred in creation.
        _store->setPositionError(id);
        continue;
      } // catch

      bool is_local = *res[i]["local"].c_str() == 'Y';

      if (!is_local) push_aprs( pos->compile() );
      _store->setPositionSent(id, time(NULL) );
      num_created++;

      delete pos;
    } // while

    return num_created;
  } // Worker::create_positions

  /*********************
   ** Create Messages **
   *********************/
  struct aprs_message_t {
    bool local;
    int id;
    std::string source;
    std::string target;
    std::string message;
    std::string title;
    std::string decay_id;
  }; // struct aprs_message_t

  unsigned int Worker::create_messages() {
    openframe::DBI::resultType res;
    openframe::DBI::resultSizeType num_rows = _store->getPendingMessages(res);
    if (!num_rows) return 0;

    unsigned int num_created = 0;
    for(openframe::DBI::resultSizeType i=0; i < res.num_rows(); i++) {
      aprs_message_t m;

      // initialize variables
      res[i]["source"].to_string(m.source);
      res[i]["target"].to_string(m.target);
      res[i]["message"].to_string(m.message);
      m.id = atoi(res[i]["id"].c_str());
      m.local = (*res[i]["local"].c_str() == 'Y' ? true : false);

      std::stringstream s;
      s << "Create message \'"
        << m.message
        << "\' to "
        << m.target;
      m.title = s.str();

      // only tack on msgid if the client might support reply-acks
      std::string msgid;
      s.str("");
      if (_store->getLastMessageId(m.target, msgid)
          && msgid.length() == 2)
        s << m.message << msgid;
      else
        s << m.message;

      aprs::Message *msg;
      try {
        msg = new aprs::Message(m.source,
                                _aprs_dest,
                                m.target,
                                s.str());
        msg->add_digis(_digis);
      } // try
      catch(aprs::APRS_Exception &ex) {
        TLOG(LogWarn, << "could not create message; "
                      << ex.message()
                      << std::endl);
        _store->setMessageError(m.id);
        continue;
      } // catch

      std::string pac = msg->compile();
      delete msg;

      TLOG(LogNotice, << "Creating message from "
                      << m.source
                      << " to "
                      << m.target
                      << " with message "
                      << s.str()
                      << std::endl);

      if (!m.local) {
        _decay->add(m.source,
                    m.title,
                    pac,
                    _decay_retry,
                    _decay_timeout,
                    m.decay_id);

        push_aprs(pac);
        _store->setMessageSent(m.id, m.decay_id, time(NULL) );
        setMessageSessionInMemcached(m.source);

        num_created++;
      } // if

    } // while

    return num_created;
  } // Worker::create_messages

  bool Worker::setMessageSessionInMemcached(const std::string &source) {
    std::string key = openframe::StringTool::toUpper(source);
    bool isOK = true;

    return _store->setAckInMemcached(key, "1", 300);
  } // Worker::_setMessageSessionInMemcached

  /********************
   ** Create Objects **
   ********************/
  struct aprs_object_t {
    bool toKill;
    bool local;
    std::string decay_id;
    std::string name;
    std::string status;
    std::string source;
    std::string overlay;
    std::string title;
    char symbol_table, symbol_code;
    double latitude, longitude, altitude, speed;
    int course;
    unsigned int id;
    unsigned int beacon;
    time_t broadcast_ts;
    time_t expire_ts;
  };

  unsigned int Worker::create_objects() {
    openframe::DBI::resultType res;
    openframe::DBI::resultSizeType num_rows = _store->getPendingObjects(time(NULL), res);
    if (!num_rows) return 0;

    unsigned int num_created = 0;
    for(openframe::DBI::resultSizeType i=0; i < res.num_rows(); i++) {
      aprs_object_t o;
      o.id = atoi(res[i]["id"].c_str());
      o.broadcast_ts = atoi( res[i]["broadcast_ts"].c_str() );
      o.expire_ts = atoi( res[i]["expire_ts"].c_str() );
      o.beacon = atoi( res[i]["beacon"].c_str() );

      if (o.broadcast_ts > 0
          && o.broadcast_ts > (time(NULL) - o.beacon))
        continue;

      // initialize variables
      o.speed = o.altitude = 0.0;
      o.course = 0;

      res[i]["name"].to_string(o.name);
      res[i]["source"].to_string(o.source);
      o.latitude = atof( res[i]["latitude"].c_str() );
      o.longitude = atof( res[i]["longitude"].c_str() );
      o.symbol_table = *res[i]["symbol_table"].c_str();
      o.symbol_code = *res[i]["symbol_code"].c_str();
      res[i]["decay_id"].to_string(o.decay_id);
      o.local = (res[i]["local"] == "Y") ? true : false;

      if ( !res[i]["speed"].is_null() )
        o.speed = atof(res[i]["speed"].c_str());

      if ( !res[i]["course"].is_null() )
        o.course = int(atof(res[i]["course"].c_str()));

      if ( !res[i]["altitude"].is_null() )
        o.altitude = atof(res[i]["altitude"].c_str());

      if ( !res[i]["status"].is_null() )
        res[i]["status"].to_string(o.status);
      else
        o.status = "";

      o.toKill = (*res[i]["kill"].c_str() == 'Y' ? true : false);

      // remove any decays for this object
      std::string decayId;
      if (_store->getObjectDecayId(o.name, time(NULL) - 14400, decayId))
        _decay->remove(decayId);

      std::stringstream s;
      s << (o.toKill != true ? "Create" : "Delete")
        << " object \'"
        << o.name
        << "\'";

      o.title = s.str();

      aprs::Object *obj;
      try {
        obj = new aprs::Object(o.source, _aprs_dest, o.name, o.latitude, o.longitude,
                               o.symbol_table, o.symbol_code, o.speed,
                               o.course, o.altitude, 0, o.status, o.toKill, 0);
        obj->add_digis(_digis);
      } // try
      catch(aprs::APRS_Exception &ex) {
        TLOG(LogWarn, << "could not create object; "
                      << ex.message()
                      << std::endl);

        // Don't keep trying to create the same object over and over
        // if an error occurred in creation.
        _store->setObjectError(o.id);
        continue;
      } // catch

      std::string pac = obj->compile();
      if (!o.local) {
        if (o.broadcast_ts == 0)
          _decay->add(o.source, o.title, pac, 30, 300, o.decay_id);

        push_aprs(pac);
      } // if

      _store->setObjectSent(o.id, o.decay_id, time(NULL) );
      num_created++;
    } // while

    return num_created;
  } // Worker::create_objects
} // namespace aprscreate
