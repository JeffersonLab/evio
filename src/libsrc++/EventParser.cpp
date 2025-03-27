//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EventParser.h"


namespace  evio {


    /**
	 * Method for parsing the event which will drill down and uncover all structures.
	 * @param evioEvent the event to parse.
	 */
    void EventParser::eventParse(std::shared_ptr<EvioEvent> evioEvent) {
        // The event itself is a structure (EvioEvent extends EvioBank) so just
        // parse it as such. The recursive drill down will take care of the rest.
        parseStruct(evioEvent);
    }


    /**
     * Parse a structure. If it is a structure of structures, such as a bank of banks or a segment of tag segments,
     * parse recursively.
     *
     * @param structure the structure being processed.
     * @throws EvioException if data not in evio format
     */
    void EventParser::parseStruct(std::shared_ptr<BaseStructure> structure) {

        // update the tree
        auto parent = structure->getParent();

        // If it is not a structure of structures, we are done. That will leave the raw bytes in
        // the "leaf" structures (e.g., a bank of ints) --which will be interpreted by the various get data methods.
        DataType dataType = structure->getHeader()->getDataType();
        if (!dataType.isStructure()) {
            return;
        }

        // Does the present structure contain structures? (as opposed to a leaf, which contains primitives). If
        // it is a leaf we are done. That will leave the raw bytes in the "leaf" structures (e.g., a bank of ints)
        // --which will be interpreted by the various "get data" methods.
        auto & bytes = structure->getRawBytes();
        ByteOrder byteOrder = structure->getByteOrder();

        if (bytes.empty()) {
            throw EvioException("Null data in structure");
        }

        size_t length = bytes.size();
        size_t offset = 0;

        if (dataType == DataType::BANK || dataType == DataType::ALSOBANK) {
            // extract all the banks from this bank.
            while (offset < length) {
                auto header = EventHeaderParser::createBankHeader(bytes.data() + offset, byteOrder);

                // offset still points to beginning of new header. Have to get data for new child bank.
                uint32_t newByteLen = 4 * (header->getLength() - 1); // -1 to account for extra header word
                std::vector<uint8_t> newBytes;
                newBytes.resize(newByteLen, 0);
                std::memcpy(newBytes.data(), bytes.data() + offset + 8, newByteLen);

                // we can now create a new bank
                auto childBank = EvioBank::getInstance(header);
                structure->add(childBank);

                childBank->setRawBytes(newBytes);
                childBank->setByteOrder(byteOrder);
                parseStruct(childBank);

                // position offset to start of next header
                offset += 4 * (header->getLength() + 1); // plus 1 for length word
            }
        }
        else if (dataType == DataType::SEGMENT || dataType == DataType::ALSOSEGMENT) {
            // extract all the segments from this bank.
            while (offset < length) {
                auto header = EventHeaderParser::createSegmentHeader(bytes.data() + offset, byteOrder);

                uint32_t newByteLen = 4 * header->getLength();
                std::vector<uint8_t> newBytes;
                newBytes.resize(newByteLen, 0);
                std::memcpy(newBytes.data(), bytes.data() + offset + 4, newByteLen);

                auto childSegment = EvioSegment::getInstance(header);
                structure->add(childSegment);

                childSegment->setRawBytes(newBytes);
                childSegment->setByteOrder(byteOrder);
                parseStruct(childSegment);

                offset += 4 * (header->getLength() + 1);
            }
        }
        else if (dataType == DataType::TAGSEGMENT) {
            // extract all the tag segments from this bank.
            while (offset < length) {
                auto header = EventHeaderParser::createTagSegmentHeader(bytes.data() + offset, byteOrder);

                uint32_t newByteLen = 4 * header->getLength();
                std::vector<uint8_t> newBytes;
                newBytes.resize(newByteLen, 0);
                std::memcpy(newBytes.data(), bytes.data() + offset + 4, newByteLen);

                auto childTagSegment = EvioTagSegment::getInstance(header);
                structure->add(childTagSegment);

                childTagSegment->setRawBytes(newBytes);
                childTagSegment->setByteOrder(byteOrder);
                parseStruct(childTagSegment);

                offset += 4 * (header->getLength() + 1);
            }
        }
    }

    // Now the non-static parser

    /**
   	 * This is the workhorse method for parsing the event. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners in a SAX-Like manner.
   	 *
   	 * @param evioEvent the event to parse.
   	 * @throws EvioException if arg is null or data not in evio format.
   	 */
    void EventParser::parseEvent(std::shared_ptr<EvioEvent> evioEvent) {

        auto lock = std::unique_lock<std::recursive_mutex>(mtx); // equivalent to mtx.lock();

        if (evioEvent == nullptr) {
            throw EvioException("Null event in parseEvent");
        }

        if (evioEvent->isParsed()) {
            std::cout << "Event already parsed" << std::endl;
            return;
        }

        //let listeners know we started
        notifyStart(evioEvent);

        // The event itself is a structure (EvioEvent extends EvioBank) so just
        // parse it as such. The recursive drill down will take care of the rest.
        parseStructure(evioEvent, evioEvent);

        evioEvent->setParsed(true);

        // let listeners know we stopped
        notifyStop(evioEvent);
    }


    /**
   	 * This is the workhorse method for parsing the event. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners in a SAX-Like manner. <br>
   	 * Note: applications may choose not to provide a listener. In that case, when the event is parsed, its structures
   	 * may be accessed through the event's tree model, i.e., via <code>event.getTreeModel()</code>.
   	 *
     * @param evioEvent the event to parse.
     * @param synced    if true, mutex protect this method.
   	 * @throws EvioException if arg is null or data not in evio format.
   	 */
    void EventParser::parseEvent(std::shared_ptr<EvioEvent> evioEvent, bool synced) {

        if (synced) {
            parseEvent(evioEvent);
            return;
        }

        if (evioEvent == nullptr) {
            throw EvioException("Null event in parseEvent");
        }

        if (evioEvent->isParsed()) {
            std::cout << "Event already parsed" << std::endl;
            return;
        }

        //let listeners know we started
        notifyStart(evioEvent);

        // The event itself is a structure (EvioEvent extends EvioBank) so just
        // parse it as such. The recursive drill down will take care of the rest.
        parseStructure(evioEvent, evioEvent);

        evioEvent->setParsed(true);

        // let listeners know we stopped
        notifyStop(evioEvent);
    }


    /**
	 * Parse a structure. If it is a structure of structures, such as a bank of banks or a segment of tag segments,
	 * parse recursively. Listeners are notified AFTER all their children have been handled, not before. Thus the
	 * LAST structure that will send notification is the outermost bank--the event bank itself.
	 *
	 * @param evioEvent the parent event being processed.
	 * @param structure the structure being processed.
	 * @throws EvioException if data not in evio format.
	 */
    void EventParser::parseStructure(std::shared_ptr<EvioEvent> evioEvent,
                                     std::shared_ptr<BaseStructure> structure) {

        // update the tree
        auto parent = structure->getParent();

        // if it is not a structure of structures, we are done. That will leave the raw bytes in
        // the "leaf" structures (e.g., a bank of ints) --which will be interpreted by the various get data methods.
        DataType dataType = structure->getHeader()->getDataType();
        if (!dataType.isStructure()) {
            // notify the listeners
            notifyEvioListeners(evioEvent, structure);
            return;
        }

        // Does the present structure contain structures? (as opposed to a leaf, which contains primitives). If
        // it is a leaf we are done. That will leave the raw bytes in the "leaf" structures (e.g., a bank of ints)
        // --which will be interpreted by the various "get data" methods.
        auto & bytes = structure->getRawBytes();
        ByteOrder byteOrder = structure->getByteOrder();

        if (bytes.empty()) {
            throw EvioException("Null data in structure");
        }

        size_t length = bytes.size();
        size_t offset = 0;

        if (dataType == DataType::BANK || dataType == DataType::ALSOBANK) {
            // extract all the banks from this bank.
            while (offset < length) {
                auto header = EventHeaderParser::createBankHeader(bytes.data() + offset, byteOrder);

                // offset still points to beginning of new header. Have to get data for new child bank.
                uint32_t newByteLen = 4 * (header->getLength() - 1); // -1 to account for extra header word
                std::vector<uint8_t> newBytes;
                newBytes.resize(newByteLen, 0);
                std::memcpy(newBytes.data(), bytes.data() + offset + 8, newByteLen);

                // we can now create a new bank
                auto childBank = EvioBank::getInstance(header);
                structure->add(childBank);

                childBank->setRawBytes(newBytes);
                childBank->setByteOrder(byteOrder);
                parseStructure(evioEvent, childBank);

                // position offset to start of next header
                offset += 4 * (header->getLength() + 1); // plus 1 for length word
            }
        }
        else if (dataType == DataType::SEGMENT || dataType == DataType::ALSOSEGMENT) {
            // extract all the segments from this bank.
            while (offset < length) {
                auto header = EventHeaderParser::createSegmentHeader(bytes.data() + offset, byteOrder);

                uint32_t newByteLen = 4 * header->getLength();
                std::vector<uint8_t> newBytes;
                newBytes.resize(newByteLen, 0);
                std::memcpy(newBytes.data(), bytes.data() + offset + 4, newByteLen);

                auto childSegment = EvioSegment::getInstance(header);
                structure->add(childSegment);

                childSegment->setRawBytes(newBytes);
                childSegment->setByteOrder(byteOrder);
                parseStructure(evioEvent, childSegment);

                offset += 4 * (header->getLength() + 1);
            }

        }
        else if (dataType == DataType::TAGSEGMENT) {
            // extract all the tag segments from this bank.
            while (offset < length) {
                auto header = EventHeaderParser::createTagSegmentHeader(bytes.data() + offset, byteOrder);

                uint32_t newByteLen = 4 * header->getLength();
                std::vector<uint8_t> newBytes;
                newBytes.resize(newByteLen, 0);
                std::memcpy(newBytes.data(), bytes.data() + offset + 4, newByteLen);

                auto childTagSegment = EvioTagSegment::getInstance(header);
                structure->add(childTagSegment);

                childTagSegment->setRawBytes(newBytes);
                childTagSegment->setByteOrder(byteOrder);
                parseStructure(evioEvent, childTagSegment);

                offset += 4 * (header->getLength() + 1);
            }
        }

        // notify the listeners
        notifyEvioListeners(evioEvent, structure);
    }


    /**
     * This is when a structure is encountered while parsing an event.
     * It notifies all listeners about the structure.
     *
     * @param evioEvent event being parsed
     * @param structure the structure encountered, which may be a Bank, Segment, or TagSegment.
     */
    void EventParser::notifyEvioListeners(std::shared_ptr<EvioEvent> evioEvent,
                                          std::shared_ptr<BaseStructure> structure) {

        // are notifications turned off?
        if (!notificationActive) {
            return;
        }

        if (evioListenerList.empty()) {
            return;
        }

        // if there is a global filter, lets use it.
        if (evioFilter != nullptr) {
            if (!evioFilter->accept(structure->getStructureType(), structure)) {
                return;
            }
        }

        for (int i = (int)evioListenerList.size(); i > 0; i--) {
            //std::cout << "notifyEvioListeners: call listener #" << (i-1) << std::endl;
            (evioListenerList[i - 1])->gotStructure(evioEvent, structure);
        }
    }


    /**
     * Notify listeners we are starting to parse a new event
     * @param evioEvent the event in question;
     */
    void EventParser::notifyStart(std::shared_ptr<EvioEvent> evioEvent) {

        // are notifications turned off?
        if (!notificationActive) {
            return;
        }

        if (evioListenerList.empty()) {
            return;
        }

        for (int i = (int)evioListenerList.size(); i > 0; i--) {
            //std::cout << "notifyStart: call startEventParse on listener #" << (i-1) << std::endl;
            (evioListenerList[i - 1])->startEventParse(evioEvent);
        }
    }


    /**
     * Notify listeners we are done to parsing a new event
     * @param evioEvent the event in question;
     */
    void EventParser::notifyStop(std::shared_ptr<EvioEvent> evioEvent) {

        // are notifications turned off?
        if (!notificationActive) {
            return;
        }

        if (evioListenerList.empty()) {
            return;
        }

        for (int i = (int)evioListenerList.size(); i > 0; i--) {
            //std::cout << "notifyStop: call endEventParse on listener #" << (i-1) << std::endl;
            (evioListenerList[i - 1])->endEventParse(evioEvent);
        }
    }


    /**
     * Remove an Evio listener. Evio listeners listen for structures encountered when an event is being parsed.
     * @param listener The Evio listener to remove.
     */
    void EventParser::removeEvioListener(std::shared_ptr<IEvioListener> listener) {
        if ((listener == nullptr) || (evioListenerList.empty())) {
            return;
        }

        auto it = evioListenerList.begin();
        auto end = evioListenerList.end();

        for (; it < end; it++) {
            if (listener == *it) {
                evioListenerList.erase(it);
                break;
            }
        }
    }


    /**
     * Add an Evio listener. Evio listeners listen for structures encountered when an event is being parsed.
     * @param listener The Evio listener to add.
     */
    void EventParser::addEvioListener(std::shared_ptr<IEvioListener> listener) {
        if (listener == nullptr) {
            return;
        }
        evioListenerList.push_back(listener);
    }


    /**
     * Get the flag determining whether notification of listeners is active. Normally it is. But in some cases it should
     * be temporarily suspended. For example, in a "goto event" process, the listeners will not be notified of the
     * intervening events as the file is scanned to get to the target event.
     *
     * @return <code>true</code> if notification of events to the listeners is active.
     */
    bool EventParser::isNotificationActive() const {return notificationActive;}


    /**
     * Set the flag determining whether notification of listeners is active. Normally it is. But in some cases it
     * should be temporarily suspended. For example, in a "goto event" process, the listeners will not be notified of
     * the intervening events as the file is scanned to get to the target event.
     *
     * @param active set <code>true</code> if notification of events to the listeners is active.
     */
    void EventParser::setNotificationActive(bool active) {notificationActive = active;}


    /**
     * Set the global filter used for filtering structures. If set to <code>null</code>, the default, then all
     * structures will be sent to the listeners.
     *
     * @param filter the filter to set.
     * @see IEvioFilter
     */
    void EventParser::setEvioFilter(std::shared_ptr<IEvioFilter> filter) {evioFilter = filter;}


    ///////////////////////////////////////////
    //
    //   Scanning parsed BaseStructure trees
    //
    //////////////////////////////////////////


    /**
     * Visit all the structures in the given structure (including the structure itself --
     * which is considered its own descendant).
     * This is similar to listening to the event as it is being parsed,
     * but is done to a complete (already) parsed event.
     *
     * @param structure  the structure to start scanning.
     * @param listener   an listener to notify as each structure is visited.
     */
    void EventParser::vistAllStructures(std::shared_ptr<BaseStructure> structure,
                                        std::shared_ptr<IEvioListener> listener) {
        visitAllDescendants(structure, structure, listener, nullptr);
    }


    /**
     * Visit all the structures in the given structure (including the structure itself --
     * which is considered its own descendant) in a depth first manner.
     *
     * @param structure  the structure to start scanning.
     * @param listener an listener to notify as each structure is visited.
     * @param filter an optional filter that must "accept" structures before
     *               they are passed to the listener. If <code>null</code>, all
     *               structures are passed. In this way, specific types of
     *               structures can be captured.
     */
    void EventParser::vistAllStructures(std::shared_ptr<BaseStructure> structure,
                                        std::shared_ptr<IEvioListener> listener,
                                        std::shared_ptr<IEvioFilter>   filter) {
        visitAllDescendants(structure, structure, listener, filter);
    }


    /**
     * Visit all the descendants of a given structure
     * (which is considered a descendant of itself.)
     *
     * @param topLevelStruct the structure starting the scan.
     * @param structure  the structure to start scanning.
     * @param listener an listener to notify as each structure is visited.
     * @param filter an optional filter that must "accept" structures before
     *               they are passed to the listener. If <code>nullptr</code>, all
     *               structures are passed. In this way, specific types of
     *               structures can be captured.
     */
    void EventParser::visitAllDescendants(std::shared_ptr<BaseStructure> topLevelStruct,
                                          std::shared_ptr<BaseStructure> structure,
                                          std::shared_ptr<IEvioListener> listener,
                                          std::shared_ptr<IEvioFilter>   filter) {
        if (listener != nullptr) {
            bool accept = true;
            if (filter != nullptr) {
                accept = filter->accept(structure->getStructureType(), structure);
            }

            if (accept) {
                listener->gotStructure(topLevelStruct, structure);
            }
        }

        if (!(structure->isLeaf())) {
            for (auto child : structure->children) {
                visitAllDescendants(topLevelStruct, child, listener, filter);
            }
        }
    }


    /**
     * Visit all the descendant structures, and collect those that pass a filter.
     *
     * @param structure  the structure to start scanning.
     * @param filter the filter that must be passed. If <code>nullptr</code>,
     *               this will return all the structures.
     * @param structs vector to be filled with all structures that are accepted by filter.
     */
    void EventParser::getMatchingStructures(std::shared_ptr<BaseStructure> structure,
                                            std::shared_ptr<IEvioFilter>   filter,
                                            std::vector<std::shared_ptr<BaseStructure>> & structs) {
        structs.clear();
        structs.reserve(25);

        // Create a listener that puts all structures into a vector
        class matchListener final : public IEvioListener {
            std::vector<std::shared_ptr<BaseStructure>> & vec;
        public:
            explicit matchListener(std::vector<std::shared_ptr<BaseStructure>> & v) : vec(v) {}

            void startEventParse(std::shared_ptr<BaseStructure> structure) override {};
            void endEventParse(std::shared_ptr<BaseStructure> structure) override {};

            void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                              std::shared_ptr<BaseStructure> structure) override {
                vec.push_back(structure);
            }
        };

        std::shared_ptr<matchListener> spListener = std::make_shared<matchListener>(structs);

        vistAllStructures(structure, spListener, filter);
    }



}
