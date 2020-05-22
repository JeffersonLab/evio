//
// Created by Carl Timmer on 2020-05-19.
//

#include "EventParser.h"


namespace  evio {


//    /**
//	 * Method for parsing the event which will drill down and uncover all structures.
//	 * @param evioEvent the event to parse.
//	 */
//    void EventParser::eventParse(std::shared_ptr<EvioEvent> & evioEvent) {
//        // The event itself is a structure (EvioEvent extends EvioBank) so just
//        // parse it as such. The recursive drill down will take care of the rest.
//        parseStruct(evioEvent, evioEvent);
//    }
//
//    /**
//     * Parse a structure. If it is a structure of structures, such as a bank of banks or a segment of tag segments,
//     * parse recursively.
//     *
//     * @param evioEvent the parent event being processed.
//     * @param structure the structure being processed.
//     * @throws EvioException if data not in evio format
//     */
//    void EventParser::parseStruct(std::shared_ptr<EvioEvent> & evioEvent, std::shared_ptr<BaseStructure> & structure) {
//
//        // update the tree
//        auto parent = structure->getParent();
//
//        if (parent != nullptr) { // start of event
//            evioEvent->insert(structure, parent);
//        }
//
//        // If it is not a structure of structures, we are done. That will leave the raw bytes in
//        // the "leaf" structures (e.g., a bank of ints) --which will be interpreted by the various get data methods.
//        DataType dataType = structure->getHeader()->getDataType();
//        if (!dataType.isStructure()) {
//            return;
//        }
//
//        // Does the present structure contain structures? (as opposed to a leaf, which contains primitives). If
//        // it is a leaf we are done. That will leave the raw bytes in the "leaf" structures (e.g., a bank of ints)
//        // --which will be interpreted by the various "get data" methods.
//        auto bytes = structure->getRawBytes();
//        ByteOrder byteOrder = structure->getByteOrder();
//
//        if (bytes == null) {
//            throw EvioException("Null data in parseStructure (Bank).");
//        }
//
//        int length = bytes.length;
//        int offset = 0;
//
//        switch (dataType) {
//            case BANK:
//            case ALSOBANK:
//
//                // extract all the banks from this bank.
//                while (offset < length) {
//                    BankHeader header = createBankHeader(bytes, offset, byteOrder);
//
//                    // offset still points to beginning of new header. Have to get data for new child bank.
//                    int newByteLen = 4 * (header.getLength() - 1); // -1 to account for extra header word
//                    byte newBytes[] = new byte[newByteLen];
//                    System.arraycopy(bytes, offset + 8, newBytes, 0, newByteLen);
//
//                    // we can now create a new bank
//                    EvioBank childBank = new EvioBank(header);
//                    childBank.setParent(structure);
//
//                    childBank.setRawBytes(newBytes);
//                    childBank.setByteOrder(byteOrder);
//                    parseStruct(evioEvent, childBank);
//
//                    // position offset to start of next header
//                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
//                }
//                break; // structure contains banks
//
//            case SEGMENT:
//            case ALSOSEGMENT:
//
//                // extract all the segments from this bank.
//                while (offset < length) {
//                    SegmentHeader header = createSegmentHeader(bytes, offset, byteOrder);
//
//                    // offset still points to beginning of new header. Have to get data for new child segment.
//                    int newByteLen = 4 * header.getLength();
//                    byte newBytes[] = new byte[newByteLen];
//                    System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);
//
//                    // we can now create a new segment
//                    EvioSegment childSegment = new EvioSegment(header);
//                    childSegment.setParent(structure);
//
//                    childSegment.setRawBytes(newBytes);
//                    childSegment.setByteOrder(byteOrder);
//                    parseStruct(evioEvent, childSegment);
//
//                    // position offset to start of next header
//                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
//                }
//
//                break; // structure contains segments
//
//            case TAGSEGMENT:
////			case ALSOTAGSEGMENT:
//
//                // extract all the tag segments from this bank.
//                while (offset < length) {
//                    TagSegmentHeader header = createTagSegmentHeader(bytes, offset, byteOrder);
//
//                    // offset still points to beginning of new header. Have to get data for new child tag segment.
//                    int newByteLen = 4 * header.getLength();
//                    byte newBytes[] = new byte[newByteLen];
//                    System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);
//
//                    // we can now create a new tag segment
//                    EvioTagSegment childTagSegment = new EvioTagSegment(header);
//                    childTagSegment.setParent(structure);
//
//                    childTagSegment.setRawBytes(newBytes);
//                    childTagSegment.setByteOrder(byteOrder);
//                    parseStruct(evioEvent, childTagSegment);
//
//                    // position offset to start of next header
//                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
//                }
//
//                break;
//
//            default:
//        }
//    }
//
//    // Now the non-static parser
//
//
//    /**
//   	 * This is the workhorse method for parsing the event. It will drill down and uncover all structures
//     * (banks, segments, and tagsegments) and notify any interested listeners in a SAX-Like manner. <br>
//   	 * Note: applications may choose not to provide a listener. In that case, when the event is parsed, its structures
//   	 * may be accessed through the event's tree model, i.e., via <code>event.getTreeModel()</code>.
//   	 *
//   	 * @param evioEvent the event to parse.
//   	 * @throws EvioException if arg is null or data not in evio format.
//   	 */
//    void EventParser::parseEvent(std::shared_ptr<EvioEvent> evioEvent) {
//
//            if (evioEvent == nullptr) {
//                throw EvioException("Null event in parseEvent.");
//            }
//
//            if (evioEvent->isParsed()) {
//                std::cout << "Event already parsed" << std::endl;
//                return;
//            }
//
//            //let listeners know we started
//            notifyStart(evioEvent);
//
//            // The event itself is a structure (EvioEvent extends EvioBank) so just
//            // parse it as such. The recursive drill down will take care of the rest.
//            parseStructure(evioEvent, evioEvent);
//
//            evioEvent->setParsed(true);
//
//            // let listeners know we stopped
//            notifyStop(evioEvent);
//    }
//
//    /**
//   	 * This is the workhorse method for parsing the event. It will drill down and uncover all structures
//     * (banks, segments, and tagsegments) and notify any interested listeners in a SAX-Like manner. <br>
//   	 * Note: applications may choose not to provide a listener. In that case, when the event is parsed, its structures
//   	 * may be accessed through the event's tree model, i.e., via <code>event.getTreeModel()</code>.
//   	 *
//     * @param evioEvent the event to parse.
//     * @param synced if true, synchronize this method.
//   	 * @throws EvioException if arg is null or data not in evio format.
//   	 */
//    void EventParser::parseEvent(std::shared_ptr<EvioEvent> evioEvent, bool synced) {
//
//            if (synced) {
//                parseEvent(evioEvent);
//                return;
//            }
//
//            if (evioEvent == null) {
//                throw new EvioException("Null event in parseEvent.");
//            }
//
//            if (evioEvent.parsed) {
//                System.out.println("Event already parsed");
//                return;
//            }
//
//            //let listeners know we started
//            notifyStart(evioEvent);
//
//            // The event itself is a structure (EvioEvent extends EvioBank) so just
//            // parse it as such. The recursive drill down will take care of the rest.
//            parseStructure(evioEvent, evioEvent);
//
//            evioEvent.parsed = true;
//
//            // let listeners know we stopped
//            notifyStop(evioEvent);
//    }
//
//    /**
//	 * Parse a structure. If it is a structure of structures, such as a bank of banks or a segment of tag segments,
//	 * parse recursively. Listeners are notified AFTER all their children have been handled, not before. Thus the
//	 * LAST structure that will send notification is the outermost bank--the event bank itself.
//	 *
//	 * @param evioEvent the parent event being processed.
//	 * @param structure the structure being processed.
//	 * @throws EvioException if data not in evio format.
//	 */
//    private
//
//    void parseStructure(EvioEvent evioEvent, BaseStructure structure)
//
//    throws EvioException{
//
//            // update the tree
//            BaseStructure parent = structure.getParent();
//
//            if (parent == null) { // start of event
//                evioEvent.treeModel = new DefaultTreeModel(evioEvent);
//            }
//            else {
//                evioEvent.insert(structure, parent);
//            }
//
//            // if it is not a structure of structures, we are done. That will leave the raw bytes in
//            // the "leaf" structures (e.g., a bank of ints) --which will be interpreted by the various get data methods.
//            DataType dataType = structure.getHeader().getDataType();
//            if (!dataType.isStructure()) {
//                // notify the listeners
//                notifyEvioListeners(evioEvent, structure);
//                return;
//            }
//
//            // Does the present structure contain structures? (as opposed to a leaf, which contains primitives). If
//            // it is a leaf we are done. That will leave the raw bytes in the "leaf" structures (e.g., a bank of ints)
//            // --which will be interpreted by the various "get data" methods.
//            byte bytes[] = structure.getRawBytes();
//            ByteOrder byteOrder = structure.getByteOrder();
//
//            if (bytes == null) {
//                throw new EvioException("Null data in parseStructure (Bank).");
//            }
//
//            int length = bytes.length;
//            int offset = 0;
//
//            switch (dataType) {
//                case BANK:
//                case ALSOBANK:
//
//                    // extract all the banks from this bank.
//                    while (offset < length) {
//                        BankHeader header = createBankHeader(bytes, offset, byteOrder);
//
//                        // offset still points to beginning of new header. Have to get data for new child bank.
//                        int newByteLen = 4 * (header.getLength() - 1); // -1 to account for extra header word
//                        byte newBytes[] = new byte[newByteLen];
//                        System.arraycopy(bytes, offset + 8, newBytes, 0, newByteLen);
//
//                        // we can now create a new bank
//                        EvioBank childBank = new EvioBank(header);
//                        childBank.setParent(structure);
//
//                        childBank.setRawBytes(newBytes);
//                        childBank.setByteOrder(byteOrder);
//                        parseStructure(evioEvent, childBank);
//
//                        // position offset to start of next header
//                        offset += 4 * (header.getLength() + 1); // plus 1 for length word
//                    }
//                break; // structure contains banks
//
//                case SEGMENT:
//                case ALSOSEGMENT:
//
//                    // extract all the segments from this bank.
//                    while (offset < length) {
//                        SegmentHeader header = createSegmentHeader(bytes, offset, byteOrder);
//
//                        // offset still points to beginning of new header. Have to get data for new child segment.
//                        int newByteLen = 4 * header.getLength();
//                        byte newBytes[] = new byte[newByteLen];
//                        System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);
//
//                        // we can now create a new segment
//                        EvioSegment childSegment = new EvioSegment(header);
//                        childSegment.setParent(structure);
//
//                        childSegment.setRawBytes(newBytes);
//                        childSegment.setByteOrder(byteOrder);
//                        parseStructure(evioEvent, childSegment);
//
//                        // position offset to start of next header
//                        offset += 4 * (header.getLength() + 1); // plus 1 for length word
//                    }
//
//                break; // structure contains segments
//
//                case TAGSEGMENT:
////			case ALSOTAGSEGMENT:
//
//                    // extract all the tag segments from this bank.
//                    while (offset < length) {
//                        TagSegmentHeader header = createTagSegmentHeader(bytes, offset, byteOrder);
//
//                        // offset still points to beginning of new header. Have to get data for new child tag segment.
//                        int newByteLen = 4 * header.getLength();
//                        byte newBytes[] = new byte[newByteLen];
//                        System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);
//
//                        // we can now create a new tag segment
//                        EvioTagSegment childTagSegment = new EvioTagSegment(header);
//                        childTagSegment.setParent(structure);
//
//                        childTagSegment.setRawBytes(newBytes);
//                        childTagSegment.setByteOrder(byteOrder);
//                        parseStructure(evioEvent, childTagSegment);
//
//                        // position offset to start of next header
//                        offset += 4 * (header.getLength() + 1); // plus 1 for length word
//                    }
//
//                break;
//
//                default:
//            }
//            // notify the listeners
//            notifyEvioListeners(evioEvent, structure);
//    }


    /**
     * Create a bank header from the first eight bytes of the data array.
     *
     * @param bytes the byte array, probably from a bank that encloses this new bank.
     * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     *
     * @throws EvioException if data not in evio format.
     * @return the new bank header.
     */
    std::shared_ptr<BankHeader> EventParser::createBankHeader(uint8_t * bytes, ByteOrder const & byteOrder) {

        std::shared_ptr<BankHeader> header = std::make_shared<BankHeader>();

        // Does the length make sense?
        uint32_t len = 0;
        Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &len);

        header->setLength(len);
        bytes += 4;

        // Read and parse second header word
        uint32_t word = 0;
        Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &word);

        header->setTag(word >> 16);
        int dt = (word >> 8) & 0xff;
        int type = dt & 0x3f;
        uint8_t padding = dt >> 6;
        header->setDataType(type);
        header->setPadding(padding);
        header->setNumber(word);

        return header;
    }


    /**
     * Create a segment header from the first four bytes of the data array.
     *
     * @param bytes the byte array, probably from a bank that encloses this new segment.
     * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     *
     * @throws EvioException if data not in evio format.
     * @return the new segment header.
     */
    std::shared_ptr<SegmentHeader> EventParser::createSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder) {

        std::shared_ptr<SegmentHeader> header = std::make_shared<SegmentHeader>();

        // Read and parse header word
        uint32_t word = 0;
        Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &word);

        uint32_t len = word & 0xffff;
        header->setLength(len);

        int dt = (word >> 16) & 0xff;
        int type = dt & 0x3f;
        int padding = dt >> 6;
        header->setDataType(type);
        header->setPadding(padding);
        header->setTag(word >> 24);

        return header;
    }


    /**
     * Create a tag segment header from the first four bytes of the data array.
     *
     * @param bytes the byte array, probably from a bank that encloses this new tag segment.
     * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     *
     * @throws EvioException if data not in evio format.
     * @return the new tagsegment header.
     */
    std::shared_ptr<TagSegmentHeader> EventParser::createTagSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder) {

        std::shared_ptr<TagSegmentHeader> header = std::make_shared<TagSegmentHeader>();

        // Read and parse header word
        uint32_t word = 0;
        Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &word);

        uint32_t len = word & 0xffff;
        header->setLength(len);
        header->setDataType((word >> 16) & 0xf);
        header->setTag(word >> 20);

        return header;
    }

//
//    /**
//     * This is when a structure is encountered while parsing an event.
//     * It notifies all listeners about the structure.
//     *
//     * @param evioEvent event being parsed
//     * @param structure the structure encountered, which may be a Bank, Segment, or TagSegment.
//     */
//    protected
//
//    void notifyEvioListeners(EvioEvent evioEvent, IEvioStructure structure) {
//
//        // are notifications turned off?
//        if (!notificationActive) {
//            return;
//        }
//
//        if (evioListenerList == null) {
//            return;
//        }
//
//        // if there is a global filter, lets use it.
//        if (evioFilter != null) {
//            if (!evioFilter.accept(structure.getStructureType(), structure)) {
//                return;
//            }
//        }
//
//        // Guaranteed to return a non-null array
//        Object[]
//        listeners = evioListenerList.getListenerList();
//
//        // This weird loop is the bullet proof way of notifying all listeners.
//        for (int i = listeners.length - 2; i >= 0; i -= 2) {
//            if (listeners[i] == IEvioListener.class) {
//                ((IEvioListener) listeners[i + 1]).gotStructure(evioEvent, structure);
//            }
//        }
//    }
//
//    /**
//     * Notify listeners we are starting to parse a new event
//     * @param evioEvent the event in question;
//     */
//    protected
//
//    void notifyStart(EvioEvent evioEvent) {
//
//        // are notifications turned off?
//        if (!notificationActive) {
//            return;
//        }
//
//        if (evioListenerList == null) {
//            return;
//        }
//
//        // Guaranteed to return a non-null array
//        Object[]
//        listeners = evioListenerList.getListenerList();
//
//        // This weird loop is the bullet proof way of notifying all listeners.
//        for (int i = listeners.length - 2; i >= 0; i -= 2) {
//            if (listeners[i] == IEvioListener.class) {
//                ((IEvioListener) listeners[i + 1]).startEventParse(evioEvent);
//            }
//        }
//    }
//
//    /**
//     * Notify listeners we are done to parsing a new event
//     * @param evioEvent the event in question;
//     */
//    protected
//
//    void notifyStop(EvioEvent evioEvent) {
//
//        // are notifications turned off?
//        if (!notificationActive) {
//            return;
//        }
//
//        if (evioListenerList == null) {
//            return;
//        }
//
//        // Guaranteed to return a non-null array
//        Object[]
//        listeners = evioListenerList.getListenerList();
//
//        // This weird loop is the bullet proof way of notifying all listeners.
//        for (int i = listeners.length - 2; i >= 0; i -= 2) {
//            if (listeners[i] == IEvioListener.class) {
//                ((IEvioListener) listeners[i + 1]).endEventParse(evioEvent);
//            }
//        }
//    }
//
//    /**
//     * Remove an Evio listener. Evio listeners listen for structures encountered when an event is being parsed.
//     *
//     * @param listener The Evio listener to remove.
//     */
//    public
//
//    void removeEvioListener(IEvioListener listener) {
//
//        if ((listener == null) || (evioListenerList == null)) {
//            return;
//        }
//
//        evioListenerList.remove(IEvioListener.
//        class, listener);
//    }
//
//    /**
//     * Add an Evio listener. Evio listeners listen for structures encountered when an event is being parsed.
//     *
//     * @param listener The Evio listener to add.
//     */
//    public
//
//    void addEvioListener(IEvioListener listener) {
//
//        if (listener == null) {
//            return;
//        }
//
//        if (evioListenerList == null) {
//            evioListenerList = new EventListenerList();
//        }
//
//        evioListenerList.add(IEvioListener.
//        class, listener);
//    }
//
//    /**
//     * Get the flag determining whether notification of listeners is active. Normally it is. But in some cases it should
//     * be temporarily suspended. For example, in a "goto event" process, the listeners will not be notified of the
//     * intervening events as the file is scanned to get to the target event.
//     *
//     * @return <code>true</code> if notification of events to the listeners is active.
//     */
//    public
//
//    boolean isNotificationActive() {
//        return notificationActive;
//    }
//
//    /**
//     * Set the flag determining whether notification of listeners is active. Normally it is. But in some cases it
//     * should be temporarily suspended. For example, in a "goto event" process, the listeners will not be notified of
//     * the intervening events as the file is scanned to get to the target event.
//     *
//     * @param notificationActive set <code>true</code> if notification of events to the listeners is active.
//     */
//    public
//
//    void setNotificationActive(boolean notificationActive) {
//        this.notificationActive = notificationActive;
//    }
//
//    /**
//     * Set the global filter used for filtering structures. If set to <code>null</code>, the default, then all
//     * structures will be sent to the listeners.
//     *
//     * @param evioFilter the filter to set.
//     * @see IEvioFilter
//     */
//    public
//
//    void setEvioFilter(IEvioFilter evioFilter) {
//        this.evioFilter = evioFilter;
//    }

}
