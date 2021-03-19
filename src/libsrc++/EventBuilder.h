//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVENTBUILDER_H
#define EVIO_6_0_EVENTBUILDER_H


#include <iostream>
#include <cstdio>
#include <stdexcept>
#include <memory>


#include "DataType.h"
#include "EvioEvent.h"
#include "EvioBank.h"
#include "EvioSegment.h"
#include "EvioTagSegment.h"
#include "BaseStructure.h"
#include "StructureType.h"
#include "EventWriter.h"
#include "CompositeData.h"


namespace evio {

    /**
     * This class is used for creating and manipulating events. One constructor is convenient for creating new events while
     * another is useful for manipulating existing events. You can create a new EventBuilder for each event being handled;
     * however, in many cases one can use the same EventBuilder for all events by calling the setEvent method.
     * The only reason a singleton pattern was not used was to allow for the possibility that events will be built or
     * manipulated on multiple threads.
     * @author heddle (original java version)
     * @author timmer
     */
    class EventBuilder {

    private:

        /** The event being built.  */
        std::shared_ptr<EvioEvent> event;

    public:

        EventBuilder(uint16_t tag, DataType const dataType, uint8_t num) ;
        EventBuilder(std::shared_ptr<EvioEvent> & event);

        void setAllHeaderLengths();
        void clearData(std::shared_ptr<BaseStructure> structure);
        void addChild(std::shared_ptr<BaseStructure> parent, std::shared_ptr<BaseStructure> child);
        void remove(std::shared_ptr<BaseStructure> child);

        void setIntData(std::shared_ptr<BaseStructure> structure, int32_t* data, size_t count);
        void setUIntData(std::shared_ptr<BaseStructure> structure, uint32_t* data, size_t count);
        void setShortData(std::shared_ptr<BaseStructure> structure, int16_t* data, size_t count);
        void setUShortData(std::shared_ptr<BaseStructure> structure, uint16_t* data, size_t count);
        void setLongData(std::shared_ptr<BaseStructure> structure, int64_t* data, size_t count);
        void setULongData(std::shared_ptr<BaseStructure> structure, uint64_t* data, size_t count);
        void setCharData(std::shared_ptr<BaseStructure> structure, char* data, size_t count);
        void setUCharData(std::shared_ptr<BaseStructure> structure, unsigned char* data, size_t count);
        void setFloatData(std::shared_ptr<BaseStructure> structure, float* data, size_t count);
        void setDoubleData(std::shared_ptr<BaseStructure> structure, double* data, size_t count);

        // TODO: Need to look at adding strings & composite data,, vectors???????
        void setStringData(std::shared_ptr<BaseStructure> structure, std::string* data, size_t count);
        void setCompositeData(std::shared_ptr<BaseStructure> structure,
                              std::shared_ptr<CompositeData> *data, size_t count);

        void appendIntData(std::shared_ptr<BaseStructure> structure, int32_t* data, size_t count);
        void appendUIntData(std::shared_ptr<BaseStructure> structure, uint32_t* data, size_t count);
        void appendShortData(std::shared_ptr<BaseStructure> structure, int16_t* data, size_t count);
        void appendUShortData(std::shared_ptr<BaseStructure> structure, uint16_t* data, size_t count);
        void appendLongData(std::shared_ptr<BaseStructure> structure, int64_t* data, size_t count);
        void appendULongData(std::shared_ptr<BaseStructure> structure, uint64_t* data, size_t count);
        void appendCharData(std::shared_ptr<BaseStructure> structure, char* data, size_t count);
        void appendUCharData(std::shared_ptr<BaseStructure> structure, unsigned char* data, size_t count);
        void appendFloatData(std::shared_ptr<BaseStructure> structure, float* data, size_t count);
        void appendDoubleData(std::shared_ptr<BaseStructure> structure, double* data, size_t count);
        void appendStringData(std::shared_ptr<BaseStructure> structure, std::string* data, size_t count);
        void appendCompositeData(std::shared_ptr<BaseStructure> structure,
                                 std::shared_ptr<CompositeData> *data, size_t count);

        std::shared_ptr<EvioEvent> getEvent();
        void setEvent(std::shared_ptr<EvioEvent> & ev);

        static int main(int argc, char **argv);

    private:

        static void fakeIntArray(uint32_t* array, uint32_t size);
        static void fakeShortArray(uint16_t* array, uint32_t size);
        static std::string* fakeStringArray();
        static void fakeDoubleArray(double *array, uint32_t size);

    };


}


#endif //EVIO_6_0_EVENTBUILDER_H
