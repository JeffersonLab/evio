//
// Created by Carl Timmer on 2020-04-23.
//

#ifndef EVIO_6_0_BANKHEADER_H
#define EVIO_6_0_BANKHEADER_H

#include "BaseStructureHeader.h"

namespace evio {


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
