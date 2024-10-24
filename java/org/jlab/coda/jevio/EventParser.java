package org.jlab.coda.jevio;

import java.nio.ByteOrder;

import javax.swing.event.EventListenerList;
import javax.swing.tree.DefaultTreeModel;

/**
 * Creates an object that controls the parsing of events.
 * This object, like the EvioReader object, has a method for parsing an event. An EvioReader
 * object will ultimately call this method--i.e., the concrete implementation of event
 * parsing is in this class.<p>
 * There is also a static method to do the parsing of an event, but without notifications.
 * 
 * @author heddle
 * @author timmer
 */
public class EventParser {


    /**
	 * Method for parsing the event which will drill down and uncover all structures.
	 * @param evioEvent the event to parse.
	 * @throws EvioException if arg is null.
	 */
	static public void eventParse(EvioEvent evioEvent) throws EvioException {

		if (evioEvent == null) {
			throw new EvioException("Null event in parseEvent.");
		}

		// The event itself is a structure (EvioEvent extends EvioBank) so just
		// parse it as such. The recursive drill down will take care of the rest.
		parseStruct(evioEvent, evioEvent);
	}

	/**
	 * Parse a structure. If it is a structure of structures, such as a bank of banks or a segment of tag segments,
	 * parse recursively.
	 *
	 * @param evioEvent the parent event being processed.
	 * @param structure the structure being processed.
	 * @throws EvioException if data not in evio format
	 */
	static private void parseStruct(EvioEvent evioEvent, BaseStructure structure) throws EvioException {

		// update the tree
		BaseStructure parent = structure.getParent();

		if (parent == null) { // start of event
			evioEvent.treeModel = new DefaultTreeModel(evioEvent);
		}
		else {
			evioEvent.insert(structure, parent);
		}

		// If it is not a structure of structures, we are done. That will leave the raw bytes in
		// the "leaf" structures (e.g., a bank of ints) --which will be interpreted by the various get data methods.
		DataType dataType = structure.getHeader().getDataType();
		if (!dataType.isStructure()) {
			return;
		}

		// Does the present structure contain structures? (as opposed to a leaf, which contains primitives). If
		// it is a leaf we are done. That will leave the raw bytes in the "leaf" structures (e.g., a bank of ints)
        // --which will be interpreted by the various "get data" methods.
        byte bytes[] = structure.getRawBytes();
        ByteOrder byteOrder = structure.getByteOrder();

        if (bytes == null) {
            throw new EvioException("Null data in parseStructure (Bank).");
        }

        int length = bytes.length;
        int offset = 0;

        switch (dataType) {
            case BANK:
            case ALSOBANK:

                // extract all the banks from this bank.
                while (offset < length) {
                    BankHeader header = createBankHeader(bytes, offset, byteOrder);

                    // offset still points to beginning of new header. Have to get data for new child bank.
                    int newByteLen = 4 * (header.getLength() - 1); // -1 to account for extra header word
                    byte newBytes[] = new byte[newByteLen];
                    System.arraycopy(bytes, offset + 8, newBytes, 0, newByteLen);

                    // we can now create a new bank
                    EvioBank childBank = new EvioBank(header);
                    childBank.setParent(structure);

                    childBank.setRawBytes(newBytes);
                    childBank.setByteOrder(byteOrder);
                    parseStruct(evioEvent, childBank);

                    // position offset to start of next header
                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
                }
                break; // structure contains banks

            case SEGMENT:
            case ALSOSEGMENT:

                // extract all the segments from this bank.
                while (offset < length) {
                    SegmentHeader header = createSegmentHeader(bytes, offset, byteOrder);

                    // offset still points to beginning of new header. Have to get data for new child segment.
                    int newByteLen = 4 * header.getLength();
                    byte newBytes[] = new byte[newByteLen];
                    System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);

                    // we can now create a new segment
                    EvioSegment childSegment = new EvioSegment(header);
                    childSegment.setParent(structure);

                    childSegment.setRawBytes(newBytes);
                    childSegment.setByteOrder(byteOrder);
                    parseStruct(evioEvent, childSegment);

                    // position offset to start of next header
                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
                }

                break; // structure contains segments

            case TAGSEGMENT:
//			case ALSOTAGSEGMENT:

                // extract all the tag segments from this bank.
                while (offset < length) {
                    TagSegmentHeader header = createTagSegmentHeader(bytes, offset, byteOrder);

                    // offset still points to beginning of new header. Have to get data for new child tag segment.
                    int newByteLen = 4 * header.getLength();
                    byte newBytes[] = new byte[newByteLen];
                    System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);

                    // we can now create a new tag segment
                    EvioTagSegment childTagSegment = new EvioTagSegment(header);
                    childTagSegment.setParent(structure);

                    childTagSegment.setRawBytes(newBytes);
                    childTagSegment.setByteOrder(byteOrder);
                    parseStruct(evioEvent, childTagSegment);

                    // position offset to start of next header
                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
                }

                break;

            default:
        }
	}

    // Now the non-static parser

	/**
	 * Listener list for structures (banks, segments, tagsegments) encountered while processing an event.
	 */
	private EventListenerList evioListenerList;

	/**
	 * This flag determines whether notification of listeners is active. Normally it is. But in some cases it should be
	 * temporarily suspended. For example, in a "goto event" process, the listeners will not be notified of the
	 * intervening events as the file is scanned to get to the target event.
	 */
	protected boolean notificationActive = true;

	/**
	 * A single global structure filter. This may prove to be a limitation: perhaps we want one per listener.
	 */
	private IEvioFilter evioFilter;


    /**
   	 * This is the workhorse method for parsing the event. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners in a SAX-Like manner. <br>
   	 * Note: applications may choose not to provide a listener. In that case, when the event is parsed, its structures
   	 * may be accessed through the event's tree model, i.e., via <code>event.getTreeModel()</code>.
   	 *
   	 * @param evioEvent the event to parse.
   	 * @throws EvioException if arg is null or data not in evio format.
   	 */
   	public synchronized void parseEvent(EvioEvent evioEvent) throws EvioException {

        if (evioEvent == null) {
            throw new EvioException("Null event in parseEvent.");
        }

        if (evioEvent.parsed) {
            System.out.println("Event already parsed");
            return;
        }

        //let listeners know we started
        notifyStart(evioEvent);

        // The event itself is a structure (EvioEvent extends EvioBank) so just
        // parse it as such. The recursive drill down will take care of the rest.
        parseStructure(evioEvent, evioEvent);

        evioEvent.parsed = true;

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
     * @param synced if true, synchronize this method.
   	 * @throws EvioException if arg is null or data not in evio format.
   	 */
   	public void parseEvent(EvioEvent evioEvent, boolean synced) throws EvioException {

   	    if (synced) {
   	        parseEvent(evioEvent);
   	        return;
        }

        if (evioEvent == null) {
            throw new EvioException("Null event in parseEvent.");
        }

        if (evioEvent.parsed) {
            System.out.println("Event already parsed");
            return;
        }

        //let listeners know we started
        notifyStart(evioEvent);

        // The event itself is a structure (EvioEvent extends EvioBank) so just
        // parse it as such. The recursive drill down will take care of the rest.
        parseStructure(evioEvent, evioEvent);

        evioEvent.parsed = true;

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
	private void parseStructure(EvioEvent evioEvent, BaseStructure structure) throws EvioException {

		// update the tree
		BaseStructure parent = structure.getParent();

		if (parent == null) { // start of event
			evioEvent.treeModel = new DefaultTreeModel(evioEvent);
		}
		else {
			evioEvent.insert(structure, parent);
		}

		// if it is not a structure of structures, we are done. That will leave the raw bytes in
		// the "leaf" structures (e.g., a bank of ints) --which will be interpreted by the various get data methods.
		DataType dataType = structure.getHeader().getDataType();
		if (!dataType.isStructure()) {
			// notify the listeners
			notifyEvioListeners(evioEvent, structure);
			return;
		}

		// Does the present structure contain structures? (as opposed to a leaf, which contains primitives). If
		// it is a leaf we are done. That will leave the raw bytes in the "leaf" structures (e.g., a bank of ints)
        // --which will be interpreted by the various "get data" methods.
        byte bytes[] = structure.getRawBytes();
        ByteOrder byteOrder = structure.getByteOrder();

        if (bytes == null) {
            throw new EvioException("Null data in parseStructure (Bank).");
        }

        int length = bytes.length;
        int offset = 0;

        switch (dataType) {
            case BANK:
            case ALSOBANK:

                // extract all the banks from this bank.
                while (offset < length) {
                    BankHeader header = createBankHeader(bytes, offset, byteOrder);

                    // offset still points to beginning of new header. Have to get data for new child bank.
                    int newByteLen = 4 * (header.getLength() - 1); // -1 to account for extra header word
                    byte newBytes[] = new byte[newByteLen];
                    System.arraycopy(bytes, offset + 8, newBytes, 0, newByteLen);

                    // we can now create a new bank
                    EvioBank childBank = new EvioBank(header);
                    childBank.setParent(structure);

                    childBank.setRawBytes(newBytes);
                    childBank.setByteOrder(byteOrder);
                    parseStructure(evioEvent, childBank);

                    // position offset to start of next header
                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
                }
                break; // structure contains banks

            case SEGMENT:
            case ALSOSEGMENT:

                // extract all the segments from this bank.
                while (offset < length) {
                    SegmentHeader header = createSegmentHeader(bytes, offset, byteOrder);

                    // offset still points to beginning of new header. Have to get data for new child segment.
                    int newByteLen = 4 * header.getLength();
                    byte newBytes[] = new byte[newByteLen];
                    System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);

                    // we can now create a new segment
                    EvioSegment childSegment = new EvioSegment(header);
                    childSegment.setParent(structure);

                    childSegment.setRawBytes(newBytes);
                    childSegment.setByteOrder(byteOrder);
                    parseStructure(evioEvent, childSegment);

                    // position offset to start of next header
                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
                }

                break; // structure contains segments

            case TAGSEGMENT:
//			case ALSOTAGSEGMENT:

                // extract all the tag segments from this bank.
                while (offset < length) {
                    TagSegmentHeader header = createTagSegmentHeader(bytes, offset, byteOrder);

                    // offset still points to beginning of new header. Have to get data for new child tag segment.
                    int newByteLen = 4 * header.getLength();
                    byte newBytes[] = new byte[newByteLen];
                    System.arraycopy(bytes, offset + 4, newBytes, 0, newByteLen);

                    // we can now create a new tag segment
                    EvioTagSegment childTagSegment = new EvioTagSegment(header);
                    childTagSegment.setParent(structure);

                    childTagSegment.setRawBytes(newBytes);
                    childTagSegment.setByteOrder(byteOrder);
                    parseStructure(evioEvent, childTagSegment);

                    // position offset to start of next header
                    offset += 4 * (header.getLength() + 1); // plus 1 for length word
                }

                break;

            default:
        }
        // notify the listeners
		notifyEvioListeners(evioEvent, structure);
	}

    /**
     * Create a bank header from the first eight bytes of the data array.
     *
     * @param bytes the byte array, probably from a bank that encloses this new bank.
     * @param offset the offset to start reading from the byte array.
     * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     *
     * @throws EvioException if data not in evio format.
     * @return the new bank header.
     */
    static BankHeader createBankHeader(byte bytes[], int offset, ByteOrder byteOrder)
            throws EvioException {

        BankHeader header = new BankHeader();

        // Can we read at least 1 bank header?
        if (offset + 8 > bytes.length) {
            throw new EvioException("bad evio format");
        }

        // Does the length make sense?
        int len = ByteDataTransformer.toInt(bytes, byteOrder, offset);
        if ((len < 1) || bytes.length < 8 + offset) {
            throw new EvioException("bad length in bank header (0x" + Integer.toHexString(len) + ")");
        }

        header.setLength(len);
        offset += 4;

        // Read and parse second header word
        int word = ByteDataTransformer.toInt(bytes, byteOrder, offset);
        header.setTag(word >>> 16);
        int dt = (word >> 8) & 0xff;
        int type = dt & 0x3f;
        int padding = dt >>> 6;
        // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
        // This regularly happens with composite data.
        // However, it that MAY also be the legacy tagsegment type
        // with no padding information. Ignore this as having tag & num
        // in legacy code is probably rare.
        //if (dt == 0x40) {
        //    type = DataType.TAGSEGMENT.getValue();
        //    padding = 0;
        //}
        header.setDataType(type);
        header.setPadding(padding);
        header.setNumber(word & 0xff);

        return header;
    }


    /**
     * Create a segment header from the first four bytes of the data array.
     *
     * @param bytes the byte array, probably from a bank that encloses this new segment.
     * @param offset the offset to start reading from the byte array.
     * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     *
     * @throws EvioException if data not in evio format.
     * @return the new segment header.
     */
    static SegmentHeader createSegmentHeader(byte bytes[], int offset, ByteOrder byteOrder)
            throws EvioException {

        SegmentHeader header = new SegmentHeader();

        // Can we read at least 1 seg header?
        if (offset + 4 > bytes.length) {
            throw new EvioException("bad evio format");
        }

        // Read and parse header word
        int word = ByteDataTransformer.toInt(bytes, byteOrder, offset);

        int len = word & 0xffff;
        if ((len < 0) || (4*len + 4 + offset > bytes.length)) {
            throw new EvioException("bad length in seg header (0x" + Integer.toHexString(len) + ")");
        }
        header.setLength(len);

        int dt = (word >>> 16) & 0xff;
        int type = dt & 0x3f;
        int padding = dt >>> 6;
        // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
        // This regularly happens with composite data.
        // However, it that MAY also be the legacy tagsegment type
        // with no padding information. Ignore this as having tag & num
        // in legacy code is probably rare.
        //if (dt == 0x40) {
        //    type = DataType.TAGSEGMENT.getValue();
        //    padding = 0;
        //}
        header.setDataType(type);
        header.setPadding(padding);
        header.setTag(word >>> 24);

        return header;
    }


	/**
	 * Create a tag segment header from the first four bytes of the data array.
	 * 
	 * @param bytes the byte array, probably from a bank that encloses this new tag segment.
	 * @param offset the offset to start reading from the byte array.
     * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     *
     * @throws EvioException if data not in evio format.
     * @return the new tagsegment header.
	 */
	static TagSegmentHeader createTagSegmentHeader(byte bytes[], int offset, ByteOrder byteOrder)
            throws EvioException {

        TagSegmentHeader header = new TagSegmentHeader();

        // Can we read at least 1 tagseg header?
        if (offset + 4 > bytes.length) {
            throw new EvioException("bad evio format");
        }

         // Read and parse header word
        int word = ByteDataTransformer.toInt(bytes, byteOrder, offset);

        int len = word & 0xffff;
        if (4*len + 4 + offset > bytes.length) {
            throw new EvioException("bad length in tagseg header (0x" + Integer.toHexString(len) + ")");
        }
        header.setLength(len);

        header.setDataType((word >>> 16) & 0xf);
        header.setTag(word >>> 20);

        return header;
	}


	/**
	 * This is when a structure is encountered while parsing an event.
     * It notifies all listeners about the structure.
	 * 
     * @param evioEvent event being parsed
     * @param structure the structure encountered, which may be a Bank, Segment, or TagSegment.
	 */
	protected void notifyEvioListeners(EvioEvent evioEvent, IEvioStructure structure) {

		// are notifications turned off?
		if (!notificationActive) {
			return;
		}

		if (evioListenerList == null) {
			return;
		}

		// if there is a global filter, lets use it.
		if (evioFilter != null) {
			if (!evioFilter.accept(structure.getStructureType(), structure)) {
				return;
			}
		}

		// Guaranteed to return a non-null array
		Object[] listeners = evioListenerList.getListenerList();

		// This weird loop is the bullet proof way of notifying all listeners.
		for (int i = listeners.length - 2; i >= 0; i -= 2) {
			if (listeners[i] == IEvioListener.class) {
				((IEvioListener) listeners[i + 1]).gotStructure(evioEvent, structure);
			}
		}
	}

    /**
     * Notify listeners we are starting to parse a new event
     * @param evioEvent the event in question;
     */
    protected void notifyStart(EvioEvent evioEvent) {

        // are notifications turned off?
        if (!notificationActive) {
            return;
        }

        if (evioListenerList == null) {
            return;
        }

        // Guaranteed to return a non-null array
        Object[] listeners = evioListenerList.getListenerList();

        // This weird loop is the bullet proof way of notifying all listeners.
        for (int i = listeners.length - 2; i >= 0; i -= 2) {
            if (listeners[i] == IEvioListener.class) {
                ((IEvioListener) listeners[i + 1]).startEventParse(evioEvent);
            }
        }
    }
   
    /**
     * Notify listeners we are done to parsing a new event
     * @param evioEvent the event in question;
     */
    protected void notifyStop(EvioEvent evioEvent) {

        // are notifications turned off?
        if (!notificationActive) {
            return;
        }

        if (evioListenerList == null) {
            return;
        }

        // Guaranteed to return a non-null array
        Object[] listeners = evioListenerList.getListenerList();

        // This weird loop is the bullet proof way of notifying all listeners.
        for (int i = listeners.length - 2; i >= 0; i -= 2) {
            if (listeners[i] == IEvioListener.class) {
                ((IEvioListener) listeners[i + 1]).endEventParse(evioEvent);
            }
        }
    }

	/**
	 * Remove an Evio listener. Evio listeners listen for structures encountered when an event is being parsed.
	 * 
	 * @param listener The Evio listener to remove.
	 */
	public void removeEvioListener(IEvioListener listener) {

		if ((listener == null) || (evioListenerList == null)) {
			return;
		}

		evioListenerList.remove(IEvioListener.class, listener);
	}

	/**
	 * Add an Evio listener. Evio listeners listen for structures encountered when an event is being parsed.
	 * 
	 * @param listener The Evio listener to add.
	 */
	public void addEvioListener(IEvioListener listener) {

		if (listener == null) {
			return;
		}

		if (evioListenerList == null) {
			evioListenerList = new EventListenerList();
		}

		evioListenerList.add(IEvioListener.class, listener);
	}

	/**
	 * Get the flag determining whether notification of listeners is active. Normally it is. But in some cases it should
	 * be temporarily suspended. For example, in a "goto event" process, the listeners will not be notified of the
	 * intervening events as the file is scanned to get to the target event.
	 * 
	 * @return <code>true</code> if notification of events to the listeners is active.
	 */
	public boolean isNotificationActive() {
		return notificationActive;
	}

	/**
	 * Set the flag determining whether notification of listeners is active. Normally it is. But in some cases it
	 * should be temporarily suspended. For example, in a "goto event" process, the listeners will not be notified of
	 * the intervening events as the file is scanned to get to the target event.
	 * 
	 * @param notificationActive set <code>true</code> if notification of events to the listeners is active.
	 */
	public void setNotificationActive(boolean notificationActive) {
		this.notificationActive = notificationActive;
	}

	/**
	 * Set the global filter used for filtering structures. If set to <code>null</code>, the default, then all
	 * structures will be sent to the listeners.
	 * 
	 * @param evioFilter the filter to set.
	 * @see IEvioFilter
	 */
	public void setEvioFilter(IEvioFilter evioFilter) {
		this.evioFilter = evioFilter;
	}

}
