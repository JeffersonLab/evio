//
// Created by timmer on 1/28/20.
//

#ifndef EVIO_6_0_EVIOBANK_H
#define EVIO_6_0_EVIOBANK_H

#include "ByteBuffer.h"

namespace evio {

    class EvioBank {

//        class MyClass
//        {
//        private:
//            template <typename T> void myfunc1internal(T foo)
//            {
//                // do the actual work here
//            }
//
//        public:
//            void myfunc1(int foo)     { myfunc1internal(foo); }
//            void myfunc1(double foo)  { myfunc1internal(foo); }
//        };



    public:
        EvioBank();

        int length() { return 0; };

        int getTotalBytes() { return 0; }

        void write(std::shared_ptr<ByteBuffer> &recordEvents) {}
    };


}
#endif //EVIO_6_0_EVIOBANK_H
