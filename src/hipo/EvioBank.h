//
// Created by timmer on 1/28/20.
//

#ifndef EVIO_6_0_EVIOBANK_H
#define EVIO_6_0_EVIOBANK_H

#include "ByteBuffer.h"

namespace evio {

    class EvioBank {
    public:
        EvioBank();

        int length() { return 5; };

        int getTotalBytes() { return 2; }

        void write(std::shared_ptr<ByteBuffer> &recordEvents) {}
    };

}
#endif //EVIO_6_0_EVIOBANK_H
