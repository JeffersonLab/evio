//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#ifndef EVIO_6_0_BANKHEADER_H
#define EVIO_6_0_BANKHEADER_H

#include "Util.h"
#include "BaseStructureHeader.h"

namespace evio {

    /**
     * This the header for an evio bank structure (<code>EvioBank</code>).
     * It does not contain the raw data, just the header.
     * Note: since an "event" is really just the outermost bank, this is also the header for an
     * <code>EvioEvent</code>. Copied from the java class of identical name.
     *
     * @author heddle (original Java version)
     * @author timmer
     * @date 4/23/2020
     */
    class BankHeader : BaseStructureHeader {

    protected:

        virtual void toArray(uint8_t *bArray, uint32_t offset,
                     ByteOrder & order, uint32_t destMaxSize);
        virtual void toVector(std::vector<uint8_t> & bVec, uint32_t offset, ByteOrder & order);

    public:

        BankHeader() = default;
        BankHeader(uint32_t tag, DataType & dataType, uint32_t num = 0);

        virtual uint32_t getHeaderLength();
        virtual string toString();
        uint32_t write(ByteBuffer & byteBuffer);
    };

}


#endif //EVIO_6_0_BANKHEADER_H
