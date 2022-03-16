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
 **************************************************************************
 $Id: Decay.h,v 1.1 2005/11/21 18:16:02 omni Exp $
 **************************************************************************/

#ifndef APRSCREATE_DECAY_H
#define APRSCREATE_DECAY_H

#include <string>
#include <deque>

#include <openframe/openframe.h>

namespace aprscreate {
/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  class DecayMe {
    public:
      DecayMe() { };
      virtual ~DecayMe() { };

      std::string id;
      std::string source;
      std::string message;
      std::string name;
      time_t create_ts;
      time_t broadcast_ts;
      time_t rate;
      time_t max;
      unsigned int count;
  }; // class DecayMe

  class Decay : public openframe::LogObject {
    public:
      // ### Type Definitions ###
      typedef std::deque<DecayMe *> decayListType;
      typedef decayListType::size_type decayListSizeType;
      typedef std::deque<std::string> decayStringsType;

      Decay(const openframe::LogObject::thread_id_t thread_id=0);	// constructor
      virtual ~Decay();				// destructor

      // ### Variables ###

      // ### Members ###
      const bool add(const std::string &,
                     const std::string &,
                     const std::string &,
                     const time_t,
                     const time_t,
                     std::string &);
      const unsigned int remove(const std::string &);
      const int next(decayStringsType &);
      const decayListSizeType size() const { return _decayList.size(); }
      const decayListSizeType clear();

      /***************
       ** Variables **
       ***************/
    public:
    protected:
    private:
      decayListType _decayList;
  }; // class Decay

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace aprscreate
#endif
