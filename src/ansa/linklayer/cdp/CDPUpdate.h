//
// Copyright (C) 2009 - today, Brno University of Technology, Czech Republic
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 
/**
* @file CDPUpdate.h
* @authors Tomas Rajca, Vladimir Vesely (ivesely@fit.vutbr.cz)
* @copyright Brno University of Technology (www.fit.vutbr.cz) under GPLv3
*/

#ifndef CDPUPDATE_H_
#define CDPUPDATE_H_

#include "inet/common/INETDefs.h"
#include "ansa/linklayer/cdp/CDPUpdate_m.h"

namespace inet {


class INET_API CDPUpdate : public CDPUpdate_Base
{
  private:
    void copy(const CDPUpdate& other){};
    void clean();

  public:
    CDPUpdate() : CDPUpdate_Base() {}
    CDPUpdate(const CDPUpdate& other) : CDPUpdate_Base(other) {copy(other);}
    CDPUpdate& operator=(const CDPUpdate& other) {if (this==&other) return *this; CDPUpdate_Base::operator=(other); copy(other); return *this;}
    virtual CDPUpdate *dup() const {return new CDPUpdate(*this);}

    /**
     * Count the standard IP checksum for message.
     */
    uint16_t countChecksum();

    /**
     * Returns number of TLV in message.
     */
    unsigned int getOptionArraySize() const { return options.getTlvOptionArraySize(); }

    /**
     * Get length of the specified option.
     */
    short getOptionLength(const TlvOptionBase *opt) const;

    /**
     * Set length of the specified option.
     */
    void setOptionLength(TlvOptionBase *opt);

    /**
     * Returns option
     */
    virtual TlvOptionBase *getOptionForUpdate(unsigned int k) { return options.getTlvOptionForUpdate(k); }
    virtual const TlvOptionBase *getOption(unsigned int k) const { return options.getTlvOption(k); }

    /**
     * Returns the TlvOptionBase of the specified type,
     * or nullptr. If index is 0, then the first, if 1 then the
     * second option is returned.
     */
    virtual const TlvOptionBase *findOptionByType(short int optionType, int index = 0) const;

    /**
     * Adds an TlvOptionBase to the update.
     * default atPos means add to the end.
     */
    virtual void addOption(TlvOptionBase *opt, int atPos = -1);

};
} /* namespace inet */

#endif /* CDPUPDATE_H_ */
