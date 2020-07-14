//
// Created by Carl Timmer on 7/13/20.
//

#ifndef EVIO_6_0_EVENTBUILDER_H
#define EVIO_6_0_EVENTBUILDER_H


#include <iostream>
#include <cstdio>
#include <stdexcept>
#include <memory>


#include "DataType.h"
#include "EvioEvent.h"
#include "BaseStructure.h"
#include "StructureType.h"


namespace evio {

    /**
     * This class is used for creating and manipulating events. One constructor is convenient for creating new events while
     * another is useful for manipulating existing events. You can create a new EventBuilder for each event being handled,
     * However, in many cases one can use the same EventBuilder for all events by calling the setEvent method.
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

        EventBuilder(uint16_t tag, DataType const & dataType, uint8_t num) ;
        EventBuilder(std::shared_ptr<EvioEvent> & event);

        void setAllHeaderLengths();
        void clearData(std::shared_ptr<BaseStructure> & structure);
        void addChild(std::shared_ptr<BaseStructure> & parent, std::shared_ptr<BaseStructure> & child);
        void remove(std::shared_ptr<BaseStructure> & child);

        static void setIntData(std::shared_ptr<BaseStructure> & structure, int32_t* data, size_t count);
        static void setUIntData(std::shared_ptr<BaseStructure> & structure, uint32_t* data, size_t count);
        static void setShortData(std::shared_ptr<BaseStructure> & structure, int16_t* data, size_t count);
        static void setUShortData(std::shared_ptr<BaseStructure> & structure, uint16_t* data, size_t count);
        static void setLongData(std::shared_ptr<BaseStructure> & structure, int64_t* data, size_t count);
        static void setULongData(std::shared_ptr<BaseStructure> & structure, uint64_t* data, size_t count);
        static void setCharData(std::shared_ptr<BaseStructure> & structure, char* data, size_t count);
        static void setUCharData(std::shared_ptr<BaseStructure> & structure, unsigned char* data, size_t count);
        static void setFloatData(std::shared_ptr<BaseStructure> & structure, float* data, size_t count);
        static void setDoubleData(std::shared_ptr<BaseStructure> & structure, double* data, size_t count);
        static void setStringData(std::shared_ptr<BaseStructure> & structure, std::string* data, size_t count);

        static void appendIntData(std::shared_ptr<BaseStructure> & structure, int32_t* data, size_t count);
        static void appendUIntData(std::shared_ptr<BaseStructure> & structure, uint32_t* data, size_t count);
        static void appendShortData(std::shared_ptr<BaseStructure> & structure, int16_t* data, size_t count);
        static void appendUShortData(std::shared_ptr<BaseStructure> & structure, uint16_t* data, size_t count);
        static void appendLongData(std::shared_ptr<BaseStructure> & structure, int64_t* data, size_t count);
        static void appendULongData(std::shared_ptr<BaseStructure> & structure, uint64_t* data, size_t count);
        static void appendCharData(std::shared_ptr<BaseStructure> & structure, char* data, size_t count);
        static void appendUCharData(std::shared_ptr<BaseStructure> & structure, unsigned char* data, size_t count);
        static void appendFloatData(std::shared_ptr<BaseStructure> & structure, float* data, size_t count);
        static void appendDoubleData(std::shared_ptr<BaseStructure> & structure, double* data, size_t count);
        static void appendStringData(std::shared_ptr<BaseStructure> & structure, std::string* data, size_t count);

        std::shared_ptr<EvioEvent> getEvent();
        void setEvent(std::shared_ptr<EvioEvent> & event);

        static void main(int argc, char **argv);

    private:

        static int[] fakeIntArray(int size);
        static short[] fakeShortArray(int size);
        static char[] fakeCharArray();
        static double[] fakeDoubleArray(int size);

    };


}


#endif //EVIO_6_0_EVENTBUILDER_H
