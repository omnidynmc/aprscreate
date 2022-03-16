/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** HAL9000, Internet Relay Chat Bot                                     **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
 **                    Daniel Robert Karrels                             **
 **                    Dynamic Networking Solutions                      **
 **                                                                      **
 ** This program is free software; you can redistribute it and/or modify **
 ** it under the terms of the GNU General Public License as published by **
 ** the Free Software Foundation; either version 1, or (at your option)  **
 ** any later version.                                                   **
 **                                                                      **
 ** This program is distributed in the hope that it will be useful,      **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of       **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        **
 ** GNU General Public License for more details.                         **
 **                                                                      **
 ** You should have received a copy of the GNU General Public License    **
 ** along with this program; if not, write to the Free Software          **
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            **
 **************************************************************************
 $Id: APNS.cpp,v 1.12 2003/09/05 22:23:41 omni Exp $
 **************************************************************************/

#include <string>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <new>
#include <iostream>

#include <errno.h>
#include <time.h>
#include <math.h>

#include <aprs/APRS.h>

#include <openframe/openframe.h>
#include <openstats/StatsClient_Interface.h>

#include "DBI.h"
#include "MemcachedController.h"
#include "Store.h"

namespace aprscreate {
  using namespace openframe::loglevel;

/**************************************************************************
 ** Store Class                                                         **
 **************************************************************************/
  const time_t Store::kDefaultReportInterval			= 3600;

  Store::Store(const openframe::LogObject::thread_id_t thread_id,
               const std::string &host,
               const std::string &user,
               const std::string &pass,
               const std::string &db,
               const std::string memcached_host, const time_t expire_interval, const time_t report_interval)
        : openframe::LogObject(thread_id),
          _host(host),
          _user(user),
          _pass(pass),
          _db(db),
          _memcached_host(memcached_host),
          _expire_interval(expire_interval) {


    init_stats(_stats, true);
    init_stats(_stompstats, true);

    _stats.report_interval = report_interval;
    _stompstats.report_interval = 5;

    _last_cache_fail_at = 0;

    _dbi = NULL;
    _memcached = NULL;
    _profile = NULL;
  } // Store::Store

  Store::~Store() {
    if (_memcached) delete _memcached;
    if (_dbi) delete _dbi;
    if (_profile) delete _profile;
  } // Store::~Store

  Store &Store::init() {
    try {
      _dbi = new DBI(thread_id(), _host, _user, _pass, _db);
      _dbi->set_elogger( elogger(), elog_name() );
      _dbi->init();
    } // try
    catch(std::bad_alloc xa) {
      assert(false);
    } // catch

    _memcached = new MemcachedController(_memcached_host);
    _memcached->expire(_expire_interval);

    _profile = new openframe::Stopwatch();
    _profile->add("memcached.ack", 300);

    return *this;
  } // Store::init

  void Store::init_stats(obj_stats_t &stats, const bool startup) {
    memset(&stats.cache_ack, '\0', sizeof(memcache_stats_t) );
    memset(&stats.cache_session, '\0', sizeof(memcache_stats_t) );

    memset(&stats.sql_ack, '\0', sizeof(sql_stats_t) );
    memset(&stats.sql_session, '\0', sizeof(sql_stats_t) );

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // init_stats

  void Store::onDescribeStats() {
    describe_root_stat("store.num.cache.ack.hits", "store/cache/ack/num hits - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.ack.misses", "store/cache/ack/num misses - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.ack.tries", "store/cache/ack/num tries - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.ack.stored", "store/cache/ack/num stored - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.ack.hitrate", "store/cache/ack/num hitrate - ack", openstats::graphTypeGauge, openstats::dataTypeFloat);

    describe_root_stat("store.num.sql.ack.hits", "store/sql/ack/num hits - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.ack.misses", "store/sql/ack/num misses - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.ack.tries", "store/sql/ack/num tries - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.ack.inserted", "store/sql/ack/num inserted - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.ack.failed", "store/sql/ack/num failed - ack", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.ack.hitrate", "store/sql/ack/num hitrate - ack", openstats::graphTypeGauge, openstats::dataTypeFloat);
  } // Store::onDescribeStats

  void Store::onDestroyStats() {
    destroy_stat("store.num.*");
  } // Store::onDestroyStats

  void Store::try_stats() {
    try_stompstats();

    if (_stats.last_report_at > time(NULL) - _stats.report_interval) return;

    TLOG(LogNotice, << "Memcached{ack} hits "
                    << _stats.cache_ack.hits
                    << ", misses "
                    << _stats.cache_ack.misses
                    << ", tries "
                    << _stats.cache_ack.tries
                    << ", rate %"
                    << std::fixed << std::setprecision(2)
                    << OPENSTATS_PERCENT(_stats.cache_ack.hits, _stats.cache_ack.tries)
                    << " average "
                    << std::fixed << std::setprecision(4)
                    << _profile->average("memcached.ack")
                    << "s"
                    << std::endl);


    TLOG(LogNotice, << "Sql{ack} hits "
                    << _stats.sql_ack.hits
                    << ", misses "
                    << _stats.sql_ack.misses
                    << ", tries "
                    << _stats.sql_ack.tries
                    << ", rate %"
                    << std::fixed << std::setprecision(2)
                    << OPENSTATS_PERCENT(_stats.sql_ack.hits, _stats.sql_ack.tries)
                    << std::endl);

    init_stats(_stats);
  } // Store::try_stats

  void Store::try_stompstats() {
    if (_stompstats.last_report_at > time(NULL) - _stompstats.report_interval) return;

    // this prevents stompstats from having to lookup strings in
    // its hash tables over and over again in realtime at ~35 pps

    datapoint("store.num.sql.ack.tries", _stompstats.sql_ack.tries);
    datapoint("store.num.sql.ack.hits", _stompstats.sql_ack.hits);
    datapoint_float("store.num.sql.ack.hitrate", OPENSTATS_PERCENT(_stompstats.sql_ack.hits, _stompstats.sql_ack.tries) );
    datapoint("store.num.sql.ack.misses", _stompstats.sql_ack.misses);
    datapoint("store.num.sql.ack.inserted", _stompstats.sql_ack.inserted);
    datapoint("store.num.sql.ack.failed", _stompstats.sql_ack.failed);

    datapoint("store.num.cache.ack.tries", _stompstats.cache_ack.tries);
    datapoint("store.num.cache.ack.misses", _stompstats.cache_ack.misses);
    datapoint("store.num.cache.ack.hits", _stompstats.cache_ack.hits);
    datapoint_float("store.num.cache.ack.hitrate", OPENSTATS_PERCENT(_stompstats.cache_ack.hits, _stompstats.cache_ack.tries) );
    datapoint("store.num.cache.ack.stored", _stompstats.cache_ack.stored);

    init_stats(_stompstats);
  } // Store::try_stompstats()

  //
  // Verification
  //
  Store::verifyStatusEnum Store::tryVerify(const std::string &id, const std::string &source, const std::string &key) {
    bool ok = source.length() > 0 && source.length() < 10
              && id.length()
              && key.length() == 8;
    if (!ok) return verifyStatusInvalidArgs;

    // try and detect resends
    ok = _dbi->getUserMsgChecksum(id, source, key);
    if (ok) return verifyStatusIgnoredResend;

    _dbi->setUserMsgChecksum(id, source, key);

    ok = _dbi->isUserVerified(source);
    if (ok) return verifyStatusAlreadyVerified;

    DBI::resultSizeType num_affected = _dbi->setTryUserVerify(id, source, key);
    if (num_affected) return verifyStatusSuccess;

    return verifyStatusFail;
  } // Store::tryVerify

  //
  // Memcache Acks
  //
  bool Store::getAckFromMemcached(const std::string &target, std::string &ret) {
    MemcachedController::memcachedReturnEnum mcr;
    openframe::Stopwatch sw;

    if (!isMemcachedOk()) return false;

    std::string key = openframe::StringTool::toUpper(target);

    _stats.cache_ack.tries++;
    _stompstats.cache_ack.tries++;

    sw.Start();

    std::string buf;
    try {
      mcr = _memcached->get("ack", key, buf);
    } // try
    catch(MemcachedController_Exception e) {
      TLOG(LogError, << e.message() << std::endl);
      _last_cache_fail_at = time(NULL);
    } // catch

    _profile->average("memcached.ack", sw.Time());

    if (mcr != MemcachedController::MEMCACHED_CONTROLLER_SUCCESS) {
      _stats.cache_ack.misses++;
      _stompstats.cache_ack.misses++;
      return false;
    } // if

    _stats.cache_ack.hits++;
    _stompstats.cache_ack.hits++;

    ret = buf;

    TLOG(LogDebug, << "memcached{ack} found key "
                   << target
                   << std::endl);
    TLOG(LogDebug, << "memcached{ack} data: "
                   << buf
                   << std::endl);


    return true;
  } // Store::getAckFromMemcached

  bool Store::setAckInMemcached(const std::string &target, const std::string &buf, const time_t expire) {
    bool isOK = true;

    if (!isMemcachedOk()) return false;

    std::string key = openframe::StringTool::toUpper(target);

    try {
      _memcached->put("ack", key, buf, expire);
    } // try
    catch(MemcachedController_Exception e) {
      TLOG(LogError, << e.message() << std::endl);
      _last_cache_fail_at = time(NULL);
      return false;
    } // catch

    _stats.cache_ack.stored++;
    _stompstats.cache_ack.stored++;
    return isOK;
  } // Store::setAckInMemcached

  bool Store::isUserSession(const std::string &callsign, const time_t start_ts) {
    return _dbi->isUserSession(callsign, start_ts);
  } // Store::isUserSession

  openframe::DBI::resultSizeType Store::getLastMessageId(const std::string &source, std::string &id) {
    return _dbi->getLastMessageId(source, id);
  } // Store::getLastMessageId

  openframe::DBI::resultSizeType Store::getMessageDecayId(const std::string &source, const std::string &target,
                                                          const std::string &msgack, std::string &id) {
    return _dbi->getMessageDecayId(source, target, msgack, id);
  } // Store::getMessageDecayId

  openframe::DBI::resultSizeType Store::getObjectDecayId(const std::string &name, const time_t start_ts,
                                                         std::string &id) {
    return _dbi->getObjectDecayId(name, start_ts, id);
  } // Store::getObjectDecayId

  openframe::DBI::resultSizeType Store::getPendingMessages(openframe::DBI::resultType &res) {
    return _dbi->getPendingMessages(res);
  } // Store::getPendingMessages

  openframe::DBI::simpleResultSizeType Store::setMessageAck(const std::string &source,
                                                            const std::string &target,
                                                            const std::string &msgack) {
    return _dbi->setMessageAck(source, target, msgack);
  } // Store::setMessageAck

  openframe::DBI::simpleResultSizeType Store::setMessageSent(const int id,
                                                             const std::string &decay_id,
                                                             const time_t broadcast_ts) {
    return _dbi->setMessageSent(id, decay_id, broadcast_ts);
  } // Store::setMessageSent

  openframe::DBI::simpleResultSizeType Store::setMessageError(const int id) {
    return _dbi->setMessageError(id);
  } // Store::setMessageError

  openframe::DBI::resultSizeType Store::getPendingObjects(const time_t now, openframe::DBI::resultType &res) {
    return _dbi->getPendingObjects(now, res);
  } // Store::getPendingObjects

  openframe::DBI::simpleResultSizeType Store::setObjectSent(const int id,
                                                            const std::string &decay_id,
                                                            const time_t broadcast_ts) {
    return _dbi->setObjectSent(id, decay_id, broadcast_ts);
  } // Store::setObjectSent

  openframe::DBI::simpleResultSizeType Store::setObjectError(const int id) {
    return _dbi->setObjectError(id);
  } // Store::setObjectError

  openframe::DBI::resultSizeType Store::getPendingPositions(openframe::DBI::resultType &res) {
    return _dbi->getPendingPositions(res);
  } // Store::getPendingPositions

  openframe::DBI::simpleResultSizeType Store::setPositionSent(const int id, const time_t broadcast_ts) {
    return _dbi->setPositionSent(id, broadcast_ts);
  } // Store::setPositionSent

  openframe::DBI::simpleResultSizeType Store::setPositionError(const int id) {
    return _dbi->setPositionError(id);
  } // Store::setPositionError

  std::ostream &operator<<(std::ostream &ss, const Store::verifyStatusEnum status) {
    switch(status) {
      case Store::verifyStatusInvalidArgs:
        ss << "Invalid arguments.";
        break;
      case Store::verifyStatusIgnoredResend:
        ss << "Resend detected and ignored, already replied.";
        break;
      case Store::verifyStatusAlreadyVerified:
        ss << "Already verified.";
        break;
      case Store::verifyStatusSuccess:
        ss << "Verification successful!";
        break;
      case Store::verifyStatusFail:
        ss << "Verification failed, check key and try again.";
        break;
      default:
        ss << "Unknown error.";
        break;
    } // switch
    return ss;
  } // operator<<
} // namespace aprscreate
