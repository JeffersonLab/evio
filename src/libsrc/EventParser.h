//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVENTPARSER_H
#define EVIO_6_0_EVENTPARSER_H


#include <cstring>
#include <memory>
#include <vector>
#include <cstring>
#include <iostream>
#include <mutex>


#include "ByteOrder.h"
#include "BaseStructure.h"
#include "BankHeader.h"
#include "SegmentHeader.h"
#include "TagSegmentHeader.h"
#include "IEvioFilter.h"
#include "IEvioListener.h"

#include "EvioEvent.h"
#include "EvioSegment.h"
#include "EvioTagSegment.h"


namespace evio {


    /**
     * Creates an object that controls the parsing of events.
     * This object, like the EvioReader object, has a method for parsing an event. An EvioReader
     * object will ultimately call this method--i.e., the concrete implementation of event
     * parsing is in this class. There is also a static method to do the parsing of an event,
     * but without notifications.<p>
     *
     * This class is also used to scan already parsed structures in a BaseStructure tree.
     * Originally, in it's java form, the "visit" methods were part of the BaseStructure class,
     * but since that results in C++ circular references, it is now in this class
     * and slightly modified to be static.<p>
     *
     * @author heddle (original Java file).
     * @author timmer
     * @date 5/19/2020
     */
    class EventParser {

    private:

        std::vector<std::shared_ptr<IEvioListener>> evioListenerList;
        std::shared_ptr<IEvioFilter> evioFilter;

        void parseStructure(std::shared_ptr<EvioEvent> evioEvent, std::shared_ptr<BaseStructure> structure);

    protected:

        bool notificationActive = true;
        /** Mutex for thread safety. */
        std::recursive_mutex mtx;

    public:

        static void eventParse(std::shared_ptr<EvioEvent> & evioEvent);

        void parseEvent(std::shared_ptr<EvioEvent> & evioEvent);
        void parseEvent(std::shared_ptr<EvioEvent> & evioEvent, bool synced);

    private:

        static void parseStruct(std::shared_ptr<BaseStructure> structure);

// Moved to EventHeaderParser to avoid circular references to BaseStructure:
//        static std::shared_ptr<BankHeader> createBankHeader(uint8_t * bytes, ByteOrder const & byteOrder);
//        static std::shared_ptr<SegmentHeader> createSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder);
//        static std::shared_ptr<TagSegmentHeader> createTagSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder);

    protected:

        void notifyEvioListeners(std::shared_ptr<EvioEvent> & event,
                                 std::shared_ptr<BaseStructure> & structure);
        void notifyStart(std::shared_ptr<EvioEvent> & event);
        void notifyStop(std::shared_ptr<EvioEvent> & event);

    public:

        void removeEvioListener(std::shared_ptr<IEvioListener> listener);
        void addEvioListener(std::shared_ptr<IEvioListener> listener);

        bool isNotificationActive() const;
        void setNotificationActive(bool notificationActive);
        void setEvioFilter(std::shared_ptr<IEvioFilter> evioFilter);

    public:

        // Scanning structures that have already been parsed

        static void vistAllStructures(std::shared_ptr<BaseStructure> const & structure,
                                      std::shared_ptr<IEvioListener> const & listener);
        static void vistAllStructures(std::shared_ptr<BaseStructure> const & structure,
                                      std::shared_ptr<IEvioListener> const & listener,
                                      std::shared_ptr<IEvioFilter> const & filter);
        static void getMatchingStructures(std::shared_ptr<BaseStructure> const & structure,
                                          std::shared_ptr<IEvioFilter> const & filter,
                                          std::vector<std::shared_ptr<BaseStructure>> & structs);
    private:

         static void visitAllDescendants(std::shared_ptr<BaseStructure> const & topLevelStruct,
                                         std::shared_ptr<BaseStructure> const & structure,
                                         std::shared_ptr<IEvioListener> const & listener,
                                         std::shared_ptr<IEvioFilter>   const & filter);


    };


}

#endif //EVIO_6_0_EVENTPARSER_H
