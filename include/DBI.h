/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, Internet APRS MySQL Injector                               **
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
 **************************************************************************/

#ifndef APRSCREATE_DBI_H
#define APRSCREATE_DBI_H

#include <openframe/DBI.h>
#include <aprs/APRS.h>

namespace aprscreate {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

#define NULL_OPTIONPP(x, y) ( x->getString(y).length() > 0 ? mysqlpp::SQLTypeAdapter(x->getString(y)) : mysqlpp::SQLTypeAdapter(mysqlpp::null) )


/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

 class DBI : public openframe::DBI {
    public:
      DBI(const openframe::LogObject::thread_id_t thread_id,
                 const std::string &db,
                 const std::string &host,
                 const std::string &user,
                 const std::string &pass);
      virtual ~DBI();

      void prepare_queries();

      bool isUserVerified(const std::string &callsign);
      bool getUserMsgChecksum(const std::string &id, const std::string &callsign, const std::string &key);
      DBI::simpleResultSizeType setUserMsgChecksum(const std::string &id,
                                                   const std::string &callsign,
                                                   const std::string &key);
      DBI::simpleResultSizeType setTryUserVerify(const std::string &id,
                                                 const std::string &callsign,
                                                 const std::string &key);


      bool isUserSession(const std::string &callsign, const time_t start_ts);
      DBI::resultSizeType getLastMessageId(const std::string &source, std::string &id);
      DBI::resultSizeType getMessageDecayId(const std::string &source, const std::string &target,
                                            const std::string &msgack, std::string &id);
      DBI::resultSizeType getObjectDecayId(const std::string &name, const time_t start_ts, std::string &id);
      DBI::resultSizeType getPendingMessages(DBI::resultType &res);
      DBI::simpleResultSizeType setMessageAck(const std::string &source, const std::string &target,
                                              const std::string &msgack);
      DBI::simpleResultSizeType setMessageSent(const int id, const std::string &decay_id, const time_t broadcast_ts);
      DBI::simpleResultSizeType setMessageError(const int id);
      DBI::resultSizeType getPendingObjects(const time_t now, DBI::resultType &res);
      DBI::simpleResultSizeType setObjectSent(const int id, const std::string &decay_id, const time_t broadcast_ts);
      DBI::simpleResultSizeType setObjectError(const int id);
      DBI::resultSizeType getPendingPositions(DBI::resultType &res);
      DBI::simpleResultSizeType setPositionSent(const int id, const time_t broadcast_ts);
      DBI::simpleResultSizeType setPositionError(const int id);
  }; // class DBI

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/

} // namespace aprscreate
#endif
