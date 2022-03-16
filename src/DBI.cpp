/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, Internet APRS MySQL Injector                               **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
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
 $Id: Vars.cpp,v 1.1 2005/11/21 18:16:04 omni Exp $
 **************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>
#include <iostream>
#include <string>
#include <exception>
#include <sstream>

#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <math.h>

#include <openframe/openframe.h>
#include <aprs/APRS.h>

#include "DBI.h"

namespace aprscreate {
  using namespace openframe::loglevel;

  /**************************************************************************
   ** DBI Class                                                     **
   **************************************************************************/

  /******************************
   ** Constructor / Destructor **
   ******************************/

  DBI::DBI(const openframe::LogObject::thread_id_t thread_id,
                         const std::string &db,
                         const std::string &host,
                         const std::string &user,
                         const std::string &pass)
             : openframe::LogObject(thread_id),
               openframe::DBI(db, host, user, pass) {
  } // DBI::DBI

  DBI::~DBI() {
  } // DBI::~DBI

  void DBI::prepare_queries() {
    add_query("CALL_getLastMessageId", "CALL getLastMessageId(%0q:source)");
    add_query("CALL_getPendingPositions", "CALL getPendingPositions()");
    add_query("CALL_setPositionSent", "CALL setPositionSent(%0:id, %1:broadcast_ts)");
    add_query("CALL_setPositionError", "CALL setPositionError(%0:id)");
    add_query("CALL_getPendingObjects", "CALL getPendingObjects(%0:timestamp)");
    add_query("CALL_setObjectSent", "CALL setObjectSent(%0:id, %1q:decay_id, %2:broadcast_ts)");
    add_query("CALL_setObjectError", "CALL setObjectError(%0:id)");
    add_query("CALL_getPendingMessages", "CALL getPendingMessages()");
    add_query("CALL_setMessageAck", "CALL setMessageAck(%0q:source, %1q:target, %2q:msgack)");
    add_query("CALL_setMessageSent", "CALL setMessageSent(%0:id, %1q:decay_id, %2:broadcast_ts)");
    add_query("CALL_setMessageError", "CALL setMessageError(%0:id)");
    add_query("CALL_isUserSession", "CALL isUserSession(%0q:callsign, %1:timestamp)");
    add_query("CALL_getMessageDecayId", "CALL getMessageDecayId(%0q:source, %1q:target, %2q:msgack)");
    add_query("CALL_getObjectDecayId", "CALL getObjectDecayId(%0q:name, %1q:start_ts)");

    // verify queries
    add_query("CALL_getUserMsgChecksum", "CALL getUserMsgChecksum(%0:id, %1q:callsign, %2q:key)");
    add_query("CALL_setUserMsgChecksum", "CALL setUserMsgChecksum(%0:id, %1q:callsign, %2q:key)");
    add_query("CALL_setTryUserVerify", "CALL setTryUserVerify(%0:id, %1q:callsign, %2q:key)");
    add_query("CALL_isUserVerified", "CALL isUserVerified(%0q:callsign)");

  } // DBI::prepare_queries

  bool DBI::isUserVerified(const std::string &callsign) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_isUserVerified");

    resultType res;
    try {
      res = query->store(callsign);
      numRows = res.num_rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{isUserVerified}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{isUserVerified}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows ? true : false;
  } // DBI::isUserVerified

  bool DBI::getUserMsgChecksum(const std::string &id, const std::string &callsign, const std::string &key) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_getUserMsgChecksum");

    resultType res;
    try {
      res = query->store(id, callsign, key);
      numRows = res.num_rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getUserMsgChecksum}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getUserMsgChecksum}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows ? true : false;
  } // DBI::getUserMsgChecksum

  openframe::DBI::simpleResultSizeType DBI::setUserMsgChecksum(const std::string &id,
                                                                      const std::string &source,
                                                                      const std::string &key) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setUserMsgChecksum");

    DBI::simpleResultType res;
    try {
      res = query->execute(id, source, key);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setUserMsgChecksum}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setUserMsgChecksum}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setUserMsgChecksum

  openframe::DBI::simpleResultSizeType DBI::setTryUserVerify(const std::string &id,
                                                                    const std::string &source,
                                                                    const std::string &key) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setTryUserVerify");

    DBI::simpleResultType res;
    try {
      res = query->execute(id, source, key);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setTryUserVerify}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setTryUserVerify}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setTryUserVerify

  bool DBI::isUserSession(const std::string &callsign, const time_t start_ts) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_isUserSession");

    resultType res;
    try {
      res = query->store(callsign, start_ts);
      numRows = res.num_rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{isUserSession}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{isUserSession}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows ? true : false;
  } // DBI::isUserSession

  openframe::DBI::resultSizeType DBI::getLastMessageId(const std::string &source, std::string &id) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_getLastMessageId");

    resultType res;
    try {
      res = query->store(source);
      numRows = res.num_rows();

      if (numRows) res[0]["id"].to_string(id);

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getLastMessageId}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getLastMessageId}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::getLastMessageId

  openframe::DBI::resultSizeType DBI::getMessageDecayId(const std::string &source, const std::string &target,
                                                               const std::string &msgack, std::string &id) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_getMessageDecayId");

    resultType res;
    try {
      res = query->store(source, target, msgack);
      numRows = res.num_rows();

      if (numRows) res[0]["decay_id"].to_string(id);

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getMessageDecayId}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getMessageDecayId}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::getMessageDecayId

  openframe::DBI::resultSizeType DBI::getObjectDecayId(const std::string &name, const time_t start_ts,
                                                              std::string &id) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_getObjectDecayId");

    resultType res;
    try {
      res = query->store(name, start_ts);
      numRows = res.num_rows();

      if (numRows) res[0]["decay_id"].to_string(id);
      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getObjectDecayId}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getObjectDecayId}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::getObjectDecayId

  openframe::DBI::resultSizeType DBI::getPendingMessages(openframe::DBI::resultType &res) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_getPendingMessages");

    try {
      res = query->store();

      numRows = res.num_rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getPendingMessages}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getPendingMessages}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::getPendingMessages

  openframe::DBI::simpleResultSizeType DBI::setMessageAck(const std::string &source,
                                                                 const std::string &target,
                                                                 const std::string &msgack) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setMessageAck");

    DBI::simpleResultType res;
    try {
      res = query->execute(source, target, msgack);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setMessageAck}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setMessageAck}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setMessageAck

  openframe::DBI::simpleResultSizeType DBI::setMessageSent(const int id,
                                                                  const std::string &decay_id,
                                                                  const time_t broadcast_ts) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setMessageSent");

    DBI::simpleResultType res;
    try {
      res = query->execute(id, decay_id, broadcast_ts);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setMessageSent}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setMessageSent}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setMessageSent

  openframe::DBI::simpleResultSizeType DBI::setMessageError(const int id) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setMessageError");

    DBI::simpleResultType res;
    try {
      res = query->execute(id);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setMessageError}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setMessageError}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setMessageError

  openframe::DBI::resultSizeType DBI::getPendingObjects(const time_t now, openframe::DBI::resultType &res) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_getPendingObjects");

    try {
      res = query->store(now);

      numRows = res.num_rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getPendingObjects}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getPendingObjects}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::getPendingObjects

  openframe::DBI::simpleResultSizeType DBI::setObjectSent(const int id,
                                                                 const std::string &decay_id,
                                                                 const time_t broadcast_ts) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setObjectSent");

    DBI::simpleResultType res;
    try {
      res = query->execute(id, decay_id, broadcast_ts);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setObjectSent}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setObjectSent}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setObjectSent

  openframe::DBI::simpleResultSizeType DBI::setObjectError(const int id) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setObjectError");

    DBI::simpleResultType res;
    try {
      res = query->execute(id);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setObjectError}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setObjectError}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setObjectError

  openframe::DBI::resultSizeType DBI::getPendingPositions(openframe::DBI::resultType &res) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_getPendingPositions");

    try {
      res = query->store();

      numRows = res.num_rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getPendingPositions}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getPendingPositions}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::getPendingPositions

  openframe::DBI::simpleResultSizeType DBI::setPositionSent(const int id,
                                                                   const time_t broadcast_ts) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setPositionSent");

    DBI::simpleResultType res;
    try {
      res = query->execute(id, broadcast_ts);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setPositionSent}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setPositionSent}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setPositionSent

  openframe::DBI::simpleResultSizeType DBI::setPositionError(const int id) {
    int numRows = 0;

    mysqlpp::Query *query = q("CALL_setPositionError");

    DBI::simpleResultType res;
    try {
      res = query->execute(id);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setPositionError}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setPositionError}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI::setPositionError
} // namespace aprscreate
