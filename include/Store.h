/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, mySQL APRS Injector                                        **
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
 $Id: DCC.h,v 1.8 2003/09/04 00:22:00 omni Exp $
 **************************************************************************/

#ifndef APRSCREATE_STORE_H
#define APRSCREATE_STORE_H

#include <openframe/openframe.h>
#include <openstats/StatsClient_Interface.h>

#include "DBI.h"

namespace aprscreate {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/
  class MemcachedController;
  class Store : public openframe::LogObject,
                 public openstats::StatsClient_Interface {
    public:
      static const time_t kDefaultReportInterval;

      enum verifyStatusEnum {
        verifyStatusFail		= 0,
        verifyStatusSuccess		= 1,
        verifyStatusAlreadyVerified	= 2,
        verifyStatusIgnoredResend	= 3,
        verifyStatusInvalidArgs		= 4
      }; // verifyEnum

      Store(const openframe::LogObject::thread_id_t thread_id,
            const std::string &host,
            const std::string &user,
            const std::string &pass,
            const std::string &db,
            std::string memcached_host,
            const time_t expire_interval,
            const time_t report_interval=kDefaultReportInterval);
      virtual ~Store();
      Store &init();
      void onDescribeStats();
      void onDestroyStats();

      void try_stats();

      verifyStatusEnum tryVerify(const std::string &id, const std::string &callsign, const std::string &key);

      bool getAckFromMemcached(const std::string &target, std::string &ret);
      bool setAckInMemcached(const std::string &target, const std::string &buf, const time_t expire);

      bool isUserSession(const std::string &callsign, const time_t start_ts);
      openframe::DBI::resultSizeType getLastMessageId(const std::string &source, std::string &id);
      openframe::DBI::resultSizeType getMessageDecayId(const std::string &source,
                                                       const std::string &target,
                                                       const std::string &msgack, std::string &id);
      openframe::DBI::resultSizeType getObjectDecayId(const std::string &name, const time_t start_ts,
                                                      std::string &id);
      openframe::DBI::resultSizeType getPendingMessages(openframe::DBI::resultType &res);
      openframe::DBI::simpleResultSizeType setMessageAck(const std::string &source, const std::string &target,
                                                         const std::string &msgack);
      openframe::DBI::simpleResultSizeType setMessageSent(const int id, const std::string &decay_id, const time_t broadcast_ts);
      openframe::DBI::simpleResultSizeType setMessageError(const int id);
      openframe::DBI::resultSizeType getPendingObjects(const time_t now, openframe::DBI::resultType &res);
      openframe::DBI::simpleResultSizeType setObjectSent(const int id, const std::string &decay_id, const time_t broadcast_ts);
      openframe::DBI::simpleResultSizeType setObjectError(const int id);
      openframe::DBI::resultSizeType getPendingPositions(openframe::DBI::resultType &res);
      openframe::DBI::simpleResultSizeType setPositionSent(const int id, const time_t broadcast_ts);
      openframe::DBI::simpleResultSizeType setPositionError(const int id);

    // ### Variables ###

    protected:
      void try_stompstats();
      bool isMemcachedOk() const { return _last_cache_fail_at < time(NULL) - 60; }

    private:
      DBI *_dbi;			// new Injection handler
      MemcachedController *_memcached;	// memcached controller instance
      openframe::Stopwatch *_profile;

      // contructor vars
      std::string _host;
      std::string _user;
      std::string _pass;
      std::string _db;
      std::string _memcached_host;
      time_t _expire_interval;
      time_t _last_cache_fail_at;

    struct memcache_stats_t {
      unsigned int hits;
      unsigned int misses;
      unsigned int tries;
      unsigned int stored;
    }; // memcache_stats_t

    struct sql_stats_t {
      unsigned int hits;
      unsigned int misses;
      unsigned int tries;
      unsigned int inserted;
      unsigned int failed;
    };

    struct obj_stats_t {
      memcache_stats_t cache_ack;
      memcache_stats_t cache_session;
      sql_stats_t sql_ack;
      sql_stats_t sql_session;
      time_t last_report_at;
      time_t report_interval;
      time_t created_at;
    } _stats;
    obj_stats_t _stompstats;

    void init_stats(obj_stats_t &stats, const bool startup=false);
}; // Store

std::ostream &operator<<(std::ostream &ss, const Store::verifyStatusEnum status);

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace aprscreate
#endif
