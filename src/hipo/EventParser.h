//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#ifndef EVIO_6_0_EVENTPARSER_H
#define EVIO_6_0_EVENTPARSER_H


#include <cstring>
#include <memory>

#include "ByteOrder.h"
#include "BankHeader.h"
#include "SegmentHeader.h"
#include "TagSegmentHeader.h"



namespace evio {


    /**
     * Creates an object that controls the parsing of events.
     * This object, like the EvioReader object, has a method for parsing an event. An EvioReader
     * object will ultimately call this method--i.e., the concrete implementation of event
     * parsing is in this class.<p>
     * There is also a static method to do the parsing of an event, but without notifications.
     *
     * @author heddle (original Java file).
     * @author timmer
     * @date 5/19/2020
     */
    class EventParser {


//    private:
//
//     //   EventListenerList evioListenerList;
//     //   IEvioFilter evioFilter;
//        void parseStructure(std::shared_ptr<EvioEvent> evioEvent, std::shared_ptr<BaseStructure> structure);
//
//    protected:
//
//        bool notificationActive = true;
//
//    public:
//
//        static void eventParse(std::shared_ptr<EvioEvent> & evioEvent);
//        // synchronized!
//        void parseEvent(std::shared_ptr<EvioEvent> evioEvent);
//        void parseEvent(std::shared_ptr<EvioEvent> evioEvent, bool synced);
//
//    private:
//
//        static void parseStruct(std::shared_ptr<EvioEvent> & evioEvent, std::shared_ptr<BaseStructure> & structure);


    public: /* package */

        static std::shared_ptr<BankHeader> createBankHeader(uint8_t * bytes, ByteOrder const & byteOrder);
        static std::shared_ptr<SegmentHeader> createSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder);
        static std::shared_ptr<TagSegmentHeader> createTagSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder);

//    protected:
//
//        void notifyEvioListeners(EvioEvent evioEvent, IEvioStructure structure);
//        void notifyStart(EvioEvent evioEvent);
//        void notifyStop(EvioEvent evioEvent);
//
//    public:
//
//        void removeEvioListener(IEvioListener listener);
//        void addEvioListener(IEvioListener listener);
//
//        bool isNotificationActive();
//        void setNotificationActive(bool notificationActive);
//        void setEvioFilter(IEvioFilter evioFilter);
    };


}

#endif //EVIO_6_0_EVENTPARSER_H
