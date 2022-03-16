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
 $Id: Decay.cpp,v 1.2 2005/12/13 21:07:03 omni Exp $
 **************************************************************************/

#include "config.h"

#include <list>
#include <new>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include <openframe/openframe.h>

#include <Decay.h>

namespace aprscreate {
  using namespace openframe::loglevel;

/**************************************************************************
 ** Decay Class                                                          **
 **************************************************************************/

  /******************************
   ** Constructor / Destructor **
   ******************************/

  Decay::Decay(const openframe::LogObject::thread_id_t thread_id)
        : openframe::LogObject(thread_id) {
  } // Decay::Decay

  Decay::~Decay() {
    clear();
  } // Decay::~Decay

  /***************
   ** Variables **
   ***************/

  /*********************
   ** Message Members **
   *********************/

  const bool Decay::add(const std::string &source, const std::string &name,
                        const std::string &message, const time_t rate, const time_t max,
                        std::string &id) {
    DecayMe *d;
    md5wrapper md5;
    std::stringstream s;

    id="";

    if (source.length() < 1)
      return false;

    if (message.length() < 1)
      return false;

    if (rate <= 0)
      return false;

    if (max <= 0)
      return false;

    // rate*2 must be less than max
    if (rate*2 > max)
      return false;

    s.str();
    s << time(NULL) << source << message << rate;

    d = new DecayMe;

    d->rate = rate;
    d->max = max;
    d->source = source;
    d->count = 0;
    d->broadcast_ts = time(NULL);
    d->message = message;
    d->name = name;
    d->create_ts = time(NULL);
    d->id = md5.getHashFromString(s.str());

    id = d->id;

    TLOG(LogInfo, << "decay{add}: "
                  << d->name
                  << " resending in "
                  << (d->rate/60)
                  << ":"
                  << std::setw(2) << std::setfill('0')
                  << (d->rate%60)
                  << " minutes from "
                  << d->source
                  << std::endl);

    _decayList.push_back(d);

    return true;
  } // Decay::add

  const unsigned int Decay::remove(const std::string &id) {
    DecayMe *d;
    decayListType newList;
    int i;
    time_t now=time(NULL);

    if (id.length() < 1)
      return 0;

    for(i=0; !_decayList.empty();) {
      d = _decayList.front();

      if (d->id != id)
        newList.push_back(d);
      else {
        TLOG(LogInfo, << "decay{remove}: "
                      << d->name
                      << " after "
                      << ((now-d->create_ts)/60)
                      << ":"
                      << std::setw(2) << std::setfill('0')
                      << ((now-d->create_ts)%60)
                      << " minutes from "
                      << d->source
                      << std::endl);
        delete d;
        i++;
      } // else

      _decayList.pop_front();
    } // while

    _decayList = newList;

    return i;
  } // Decay::Remove

  const int Decay::next(decayStringsType &decayStrings) {
    DecayMe *d;
    decayListType newList;
    int i;
    time_t now=time(NULL);

    for(i=0; !_decayList.empty();) {
      d = _decayList.front();

      if ((d->broadcast_ts+d->rate) < now) {
        decayStrings.push_back(d->message);
        TLOG(LogInfo, << "decay{retry}: #"
                      << d->count+1
                      << ") "
                      << d->name
                      << " after "
                      << (d->rate/60)
                      << ":"
                      << std::setw(2) << std::setfill('0')
                      << (d->rate%60)
                      << " minutes, next in "
                      << (d->rate*2/60)
                      << ":"
                      << std::setw(2) << std::setfill('0')
                      << ((d->rate*2)%60)
                      << " minutes from "
                      << d->source
                      << std::endl);

        d->broadcast_ts = now;
        d->rate = d->rate*2;
        d->count++;
        i++;
      } // if

      if (d->rate <= d->max)
        newList.push_back(d);
      else {
        TLOG(LogInfo, << "decay{done}: "
                      << d->name
                      << " after "
                      << ((now-d->create_ts)/60)
                      << ":"
                      << std::setw(2) << std::setfill('0')
                      << ((now-d->create_ts)%60)
                      << " minutes from "
                      << d->source
                      << std::endl);
         delete d;
       } // else

      _decayList.pop_front();
    } // while

    _decayList = newList;

    return i;
  } // Decay::next

  const Decay::decayListSizeType Decay::clear() {
    decayListSizeType ret = _decayList.size();

    while(!_decayList.empty()) {
      delete _decayList.front();
      _decayList.pop_front();
    } // while

    return ret;
  } // Decay::clear
} // namespace aprscreate
