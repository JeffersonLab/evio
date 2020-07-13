//
// Created by Carl Timmer on 7/13/20.
//

#ifndef EVIO_6_0_EVENTBUILDER_H
#define EVIO_6_0_EVENTBUILDER_H


#include <cstdio>
#include <stdexcept>
#include <memory>


#include "DataType.h"
#include "EvioEvent.h"
#include "BaseStructure.h"


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
    public class EventBuilder {

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

        void setIntData(std::shared_ptr<BaseStructure> & structure, uint32_t* data, size_t count);
        void setShortData(std::shared_ptr<BaseStructure> & structure, uint16_t* data, size_t count);
        void setLongData(std::shared_ptr<BaseStructure> & structure, uint64_t data, size_t count);
        void setByteData(std::shared_ptr<BaseStructure> & structure, uint8_t* data, size_t count);
        void setFloatData(std::shared_ptr<BaseStructure> & structure, float* data, size_t count);
        void setDoubleData(std::shared_ptr<BaseStructure> & structure, double* data, size_t count);
        void setStringData(std::shared_ptr<BaseStructure> & structure, std::string* data, size_t count);

        void appendIntData(std::shared_ptr<BaseStructure> & structure, uint32_t* data, size_t count);
        void appendShortData(std::shared_ptr<BaseStructure> & structure, uint16_t* data, size_t count);
        void appendLongData(std::shared_ptr<BaseStructure> & structure, uint64_t* data, size_t count);
        void appendByteData(std::shared_ptr<BaseStructure> & structure, uint8_t* data, size_t count);
        void appendFloatData(std::shared_ptr<BaseStructure> & structure, float* data, size_t count)
        void appendDoubleData(std::shared_ptr<BaseStructure> & structure, double* data, size_t count);
        void appendStringData(std::shared_ptr<BaseStructure> & structure, std::string* data, size_t count);

        std::shared_ptr<EvioEvent> getEvent();
        void setEvent(std::shared_ptr<EvioEvent> & event);

        static void main(String args[]);

    private:

        static int[] fakeIntArray(int size);
        static short[] fakeShortArray(int size);
        static char[] fakeCharArray();
        static double[] fakeDoubleArray(int size);

    };


}


#endif //EVIO_6_0_EVENTBUILDER_H
