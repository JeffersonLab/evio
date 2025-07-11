/**
 * ################################
 * COMPOSITE DATA:
 * ################################
 * This is a new type of data (value = 0xf) which originated with Hall B.
 * It is a composite type and allows for possible expansion in the future
 * if there is a demand. Basically it allows the user to specify a custom
 * format by means of a string - stored in a tagsegment. The data in that
 * format follows in a bank. The routine to swap this data must be provided
 * by the definer of the composite type - in this case Hall B. The swapping
 * function is plugged into this evio library's swapping routine.
 *
 * Here's what the data looks like.
 *
 * MSB(31)                          LSB(0)
 * <---  32 bits ------------------------>
 * _______________________________________
 * |  tag    | type |    length          | --> tagsegment header
 * |_________|______|____________________|
 * |        Data Format String           |
 * |                                     |
 * |_____________________________________|
 * |              length                 | \
 * |_____________________________________|  \  bank header
 * |       tag      |  type   |   num    |  /
 * |________________|_________|__________| /
 * |               Data                  |
 * |                                     |
 * |_____________________________________|
 *
 * The beginning tagsegment is a normal evio tagsegment containing a string
 * (type = 0x3). Currently its type and tag are not used - at least not for
 * data formatting.
 * The bank is a normal evio bank header with data following.
 * The format string is used to read/write this data so that takes care of any
 * padding that may exist. As with the tagsegment, the tags and type are ignored.
 */
package org.jlab.coda.jevio;


import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import java.io.StringWriter;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * This is the class defining the composite data type.
 * It is a mixture of header and raw data.
 *
 * @author timmer
 * 
 */
public final class CompositeData implements Cloneable {

    /** String containing data format. */
    private String format;

    /** List of ints obtained from transforming format string. */
    private List<Integer> formatInts;

    /** List of extracted data items from raw bytes. */
    private List<Object> items;

    /** List of the types of the extracted data items. */
    private List<DataType> types;

    /** List of the "N" (32 bit) values extracted from the raw data. */
    private List<Integer> NList;

    /** List of the "n" (16 bit) values extracted from the raw data. */
    private List<Short> nList;

    /** List of the "m" (8 bit) values extracted from the raw data. */
    private List<Byte> mList;

    /** Tagsegment header of tagsegment containing format string. */
    private TagSegmentHeader tsHeader;

    /** Bank header of bank containing data. */
    private BankHeader bHeader;

    /** The entire raw data of the composite item - both tagsegment and data bank. */
    private byte rawBytes[];

	/** Buffer containing only the data of the composite item (no headers). */
    private ByteBuffer dataBuffer;

    /** Length of only data in bytes (not including padding). */
    private int dataBytes;

    /** Length of only data padding in bytes. */
    private int dataPadding;

    /** Offset (in 32bit words) in rawBytes to place of data. */
    private int dataOffset;

    /** Byte order of raw bytes. */
    private ByteOrder byteOrder;

    /** Index used in getting data items from the {@link #items} list. */
    private int getIndex;

    /**
     * Class used to track using the data format.
     */
    static final class LV {
        /** index of ifmt[] element containing left parenthesis "(". */
        int left;
        /** how many times format in the parenthesis must be repeated. */
        int nrepeat;
        /** right parenthesis ")" counter, or how many times format
         * in the parenthesis already repeated. */
        int irepeat;
    };


    /** Zero arg constructor used for cloning. */
    private CompositeData() {}



    /**
     * Constructor used for creating this object from scratch.
     *
     * @param format format String defining data
     * @param data data in given format
     * @param byteOrder byte order of resulting data.
     *
     * @throws EvioException data or format arg = null;
     *                       if improper format string
     */
    public CompositeData(String format, CompositeData.Data data, ByteOrder byteOrder)
                                throws EvioException {
        this(format, data.formatTag, data, data.dataTag, data.dataNum, byteOrder);
    }


    /**
     * Constructor used for creating this object from scratch.
     *
     * @param format format String defining data
     * @param formatTag tag used in tagsegment containing format
     * @param data data in given format
     * @param dataTag tag used in bank containing data
     * @param dataNum num used in bank containing data
     * @param byteOrder byte order of resulting data.
     *
     * @throws EvioException data or format arg = null;
     *                       if improper format string
     */
    public CompositeData(String format, int formatTag,
                         CompositeData.Data data, int dataTag, int dataNum,
                         ByteOrder byteOrder)
                                throws EvioException {

        boolean debug = false;

        this.format = format;
        this.byteOrder = byteOrder;

        if (debug) System.out.println("Analyzing composite data:");

        // Check args
        if (format == null || data == null) {
            throw new EvioException("format and/or data arg is null");
        }

        // Analyze format string
        formatInts = compositeFormatToInt(format);
        if (formatInts.size() < 1) {
           throw new EvioException("bad format string data");
        }

        items = data.dataItems;
        types = data.dataTypes;
        NList = data.Nlist;
        nList = data.nlist;
        mList = data.mlist;

        // Create the tag segment
        EvioTagSegment tagSegment = new EvioTagSegment(formatTag, DataType.CHARSTAR8);
        try {
            // Add format string
            tagSegment.appendStringData(format);
        }
        catch (EvioException e) {/* never happen */ }
        tsHeader = (TagSegmentHeader) tagSegment.getHeader();

        // Now create data bank
        EvioBank bank = new EvioBank(dataTag, DataType.COMPOSITE, dataNum);
        bHeader = (BankHeader) bank.getHeader();
        bHeader.setPadding(data.getPadding());

        // How many bytes do we skip over at the end?
        dataPadding = data.getPadding();

        // How big is the data in bytes (including padding) ?
        dataBytes = data.getDataSize();

        // Set data length in bank header (includes 2nd bank header word)
        bHeader.setLength(1 + dataBytes/4);

        // Length of everything except data (32 bit words)
        dataOffset =  bHeader.getHeaderLength() +
                     tsHeader.getHeaderLength() +
                     tsHeader.getLength();

        // Length of everything in bytes
        int totalByteLen = dataBytes + 4*dataOffset;

        // Create a big enough array to hold everything
        rawBytes = new byte[totalByteLen];

        // Create ByteBuffer object around array
        ByteBuffer allDataBuffer = ByteBuffer.wrap(rawBytes, 0, totalByteLen);
        allDataBuffer.order(byteOrder);

        // Write tagsegment to buffer
        tagSegment.write(allDataBuffer);

        // Write bank header to buffer
        bHeader.write(allDataBuffer);

        // Write data into the dataBuffer
        dataToRawBytes(allDataBuffer, data, formatInts);

        // Set data buffer for completeness
        dataBuffer = ByteBuffer.wrap(rawBytes, 4*dataOffset, dataBytes).slice();
        dataBuffer.order(byteOrder);

        // How big is the data in bytes (without padding) ?
        dataBytes -= data.getPadding();
    }


	/**
	 * Constructor used when reading existing data.
     *
     * @param rawBytes  raw data defining this composite type item
     * @param byteOrder byte order of rawBytes
     * @throws EvioException if null arg(s) or bad data format
	 */
	public CompositeData(byte[] rawBytes, ByteOrder byteOrder)
            throws EvioException {

        boolean debug = false;

        if (rawBytes == null) {
            throw new EvioException("null argument(s)");
        }

        if (debug) System.out.println("CompositeData constr: incoming byte order = " + byteOrder);

        if (byteOrder == null) {
            byteOrder = ByteOrder.BIG_ENDIAN;
        }

        if (debug) System.out.println("Analyzing composite data:");

        this.rawBytes  = rawBytes;
        this.byteOrder = byteOrder;

        // First read the tag segment header
        tsHeader = EventParser.createTagSegmentHeader(rawBytes, 0, byteOrder);

        if (debug) {
            System.out.println("    tagseg: type = " + Integer.toHexString(tsHeader.getDataTypeValue()) +
                                ", tag = " + tsHeader.getTag() + ", len = " + tsHeader.getLength());
        }

        // Hop over tagseg header
        dataOffset = tsHeader.getHeaderLength();

        // Read the format string it contains
        String[] strs = BaseStructure.unpackRawBytesToStrings(rawBytes, 4*dataOffset,
                                                              4*(tsHeader.getLength()));

        if (strs.length < 1) {
           throw new EvioException("bad format string data");
        }
        format = strs[0];

        if (debug) {
            System.out.println("    format: " + format);
        }

        formatInts = compositeFormatToInt(format);
        if (formatInts.size() < 1) {
           throw new EvioException("bad format string data");
        }

        // Hop over tagseg data
        dataOffset += tsHeader.getLength() - (tsHeader.getHeaderLength() - 1);

        // Read the data bank header
        bHeader = EventParser.createBankHeader(rawBytes, 4*dataOffset, byteOrder);

        // Hop over bank header
        dataOffset += bHeader.getHeaderLength();

        // How many bytes do we skip over at the end?
        dataPadding = bHeader.getPadding();

        // How much data do we have?
        dataBytes = 4*(bHeader.getLength() - (bHeader.getHeaderLength() - 1)) - dataPadding;
        if (dataBytes < 2) {
           throw new EvioException("no composite data");
        }

        int words = (tsHeader.getLength() + bHeader.getLength() + 2);

        if (debug) {
            System.out.println("    bank: type = " + Integer.toHexString(bHeader.getDataTypeValue()) +
                                ", tag = " + bHeader.getTag() + ", num = " + bHeader.getNumber());
            System.out.println("    bank: len (words) = " + bHeader.getLength() +
                               ", padding = " + dataPadding +
                               ", data len - padding = " + dataBytes +
            ", total printed words = " + words);
        }

        // Put data into ByteBuffer object
        dataBuffer = ByteBuffer.wrap(rawBytes, 4*dataOffset, dataBytes).slice();
        dataBuffer.order(byteOrder);

        // Turn dataBuffer into list of items and their types
        process();
 	}


    /**
     * Reset the data in this object.
     * This is designed to use the format, formatTag, dataTag, and dataNum
     * from the intitial constructor call.
     *
     * @param data data in given format
     *
     * @throws EvioException data arg = null;
     */
    public void resetData(CompositeData.Data data)
            throws EvioException {

        // Check args
        if (data == null) {
            throw new EvioException("data arg is null");
        }

        items = data.dataItems;
        types = data.dataTypes;
        NList = data.Nlist;
        nList = data.nlist;
        mList = data.mlist;

        EvioTagSegment tagSegment = new EvioTagSegment(tsHeader.getTag(), DataType.CHARSTAR8);
        try {
            // Add format string
            tagSegment.appendStringData(format);
        }
        catch (EvioException e) {/* never happen */ }

        // How many bytes do we skip over at the end?
        dataPadding = data.getPadding();

        // How big is the data in bytes (including padding) ?
        dataBytes = data.getDataSize();

        // Set data length in bank header (includes 2nd bank header word)
        bHeader.setLength(1 + dataBytes/4);

        // Length of everything except data (32 bit words)
        dataOffset = bHeader.getHeaderLength() +
                tsHeader.getHeaderLength() +
                tsHeader.getLength();

        // Length of everything in bytes
        int totalByteLen = dataBytes + 4*dataOffset;

        // Create a big enough array to hold everything
        if (rawBytes.length < totalByteLen) {
            rawBytes = new byte[totalByteLen];
        }

        // Create ByteBuffer object around array
        ByteBuffer allDataBuffer = ByteBuffer.wrap(rawBytes, 0, totalByteLen);
        allDataBuffer.order(byteOrder);

        // Write tagsegment to buffer
        tagSegment.write(allDataBuffer);

        // Write bank header to buffer
        bHeader.write(allDataBuffer);

        // Write data into the dataBuffer
        dataToRawBytes(allDataBuffer, data, formatInts);

        // Set data buffer for completeness
        dataBuffer = ByteBuffer.wrap(rawBytes, 4*dataOffset, dataBytes).slice();
        dataBuffer.order(byteOrder);

        // How big is the data in bytes (without padding) ?
        dataBytes -= data.getPadding();
    }


    /**
     * This method parses an array of raw bytes into an array of CompositeData objects.
     *
     * @param rawBytes  array of raw bytes to parse
     * @param byteOrder byte order of raw bytes
     * @return array of CompositeData objects obtained from parsing rawBytes. If none, return null.
     * @throws EvioException if null args or bad format of raw data
     */
    static public CompositeData[] parse(byte[] rawBytes, ByteOrder byteOrder) throws EvioException {

        boolean debug = false;

        if (rawBytes == null) {
            throw new EvioException("null argument(s)");
        }

        if (debug) System.out.println("CompositeData parse: incoming byte order = " + byteOrder);

        if (byteOrder == null) {
            byteOrder = ByteOrder.BIG_ENDIAN;
        }

        if (debug) System.out.println("Analyzing composite data:");

        // List containing all parsed CompositeData objects
        ArrayList<CompositeData> list = new ArrayList<CompositeData>(100);
        // How many bytes have we read for the current CompositeData object?
        int byteCount;
        // Offset into the given rawBytes array for the current CompositeData object
        int rawBytesOffset = 0;
        // How many unused bytes are left in the given rawBytes array?
        int rawBytesLeft = rawBytes.length;

        if (debug) System.out.println("    CD raw byte count = " + rawBytesLeft);

        // Parse while we still have bytes to read ...
        while (rawBytesLeft > 0) {

            byteCount = 0;

            // Create and fill new CompositeData object
            CompositeData cd = new CompositeData();
            cd.byteOrder = byteOrder;

            // First read the tag segment header
            cd.tsHeader = EventParser.createTagSegmentHeader(rawBytes, rawBytesOffset, byteOrder);
            byteCount += 4*(cd.tsHeader.getLength() + 1);

            if (debug) {
                System.out.println("    tagseg: type = " + cd.tsHeader.getDataType() +
                                    ", tag = " + cd.tsHeader.getTag() +
                                    ", len = " + cd.tsHeader.getLength());
            }

            // Hop over tagseg header
            cd.dataOffset = cd.tsHeader.getHeaderLength();

            // Read the format string it contains
            String[] strs = BaseStructure.unpackRawBytesToStrings(rawBytes, rawBytesOffset + 4*cd.dataOffset,
                                                                  4*(cd.tsHeader.getLength()));

            if (strs.length < 1) {
               throw new EvioException("bad format string data");
            }
            cd.format = strs[0];

            if (debug) {
                System.out.println("    format: " + cd.format);
            }

            // Chew on format string & spit out array of ints
            cd.formatInts = compositeFormatToInt(cd.format);
            if (cd.formatInts.size() < 1) {
               throw new EvioException("bad format string data");
            }

            // Hop over tagseg (format string) data
            cd.dataOffset = cd.tsHeader.getLength() + 1;

            // Read the data bank header
            cd.bHeader = EventParser.createBankHeader(rawBytes, rawBytesOffset + 4*cd.dataOffset, byteOrder);
            byteCount += 4*(cd.bHeader.getLength() + 1);

            // Hop over bank header
            cd.dataOffset += cd.bHeader.getHeaderLength();

            // How many bytes do we skip over at the end?
            cd.dataPadding = cd.bHeader.getPadding();

            // How much real data do we have (without padding)?
            cd.dataBytes = 4*(cd.bHeader.getLength() - (cd.bHeader.getHeaderLength() - 1)) - cd.dataPadding;
            if (cd.dataBytes < 2) {
               throw new EvioException("no composite data");
            }

            if (debug) {
                System.out.println("    bank: type = " + cd.bHeader.getDataType() +
                                    ", tag = " + cd.bHeader.getTag() +
                                    ", num = " + cd.bHeader.getNumber() +
                                    ", pad = " + cd.dataPadding);
                System.out.println("    bank: len (words) = " + cd.bHeader.getLength() +
                                   ", data len - padding (bytes) = " + cd.dataBytes);
            }

            // Make copy of only the rawbytes for this CompositeData object (including padding)
            cd.rawBytes = new byte[byteCount - cd.dataPadding];
            System.arraycopy(rawBytes, rawBytesOffset, cd.rawBytes,
                    0, byteCount-cd.dataPadding);

            // Put only actual data into ByteBuffer object
            cd.dataBuffer = ByteBuffer.wrap(cd.rawBytes, 4*cd.dataOffset, cd.dataBytes).slice();
            cd.dataBuffer.order(byteOrder);

            // Turn dataBuffer into list of items and their types
            cd.process();

            // Add to this CompositeData object to list
            list.add(cd);

            // Track how many raw bytes we have left to parse
            rawBytesLeft   -= byteCount;

            // Offset into rawBytes of next CompositeData object
            rawBytesOffset += byteCount;
            if (debug) System.out.println("    CD raw byte count = " + rawBytesLeft +
                                          ", raw byte offset = " + rawBytesOffset);
        }

        int size = list.size();
        if (size > 0) {
            // Turn list into array
            CompositeData[] cdArray = new CompositeData[size];
            return list.toArray(cdArray);
        }

        return null;
    }

    /**
     * This method generates raw bytes of evio format from an array of CompositeData objects.
     * The returned array consists of gluing together all the individual objects' rawByte arrays.
     *
     * @param data  array of CompositeData objects to turn into bytes.
     * @param order byte order of generated array.
     * @return array of raw, evio format bytes; null if arg is null or empty.
     * @throws EvioException if data takes up too much memory to store in raw byte array (JVM limit).
     */
    static public byte[] generateRawBytes(CompositeData[] data, ByteOrder order) throws EvioException {

        if (data == null || data.length < 1) {
            return null;
        }

        // Get a total length (# bytes)
        int totalLen = 0, len;
        for (CompositeData cd : data) {
            len = cd.getRawBytes().length;
            if (Integer.MAX_VALUE - totalLen < len) {
                throw new EvioException("added data overflowed containing structure");
            }
            totalLen += len;
        }

        // Allocate an array
        byte[] rawBytes = new byte[totalLen];

        // Copy everything in
        int offset = 0;
        for (CompositeData cd : data) {
            len = cd.getRawBytes().length;
            if (cd.byteOrder != order) {
                // This CompositeData object has a rawBytes array of the wrong byte order, so swap it
//System.out.println("CompositeData.generateRawBytes: call swapAll(), data in " + cd.byteOrder);
                swapAll(cd.getRawBytes(), 0, rawBytes, offset, len/4, cd.byteOrder);
            }
            else {
//System.out.println("CompositeData.generateRawBytes: call arraycopy()");
                System.arraycopy(cd.getRawBytes(), 0, rawBytes, offset, len);
            }
            offset += len;
        }

        return rawBytes;
    }


    /**
     * Method to clone a CompositeData object.
     * Deep cloned so no connection exists between
     * clone and object cloned.
     *
     * @return cloned CompositeData object.
     */
    public Object clone() {

        CompositeData cd = new CompositeData();

        cd.getIndex   = getIndex;
        cd.byteOrder  = byteOrder;
        cd.dataBytes  = dataBytes;
        cd.dataOffset = dataOffset;
        cd.rawBytes   = rawBytes.clone();

        // copy tagSegment header
        cd.tsHeader = new TagSegmentHeader(tsHeader.getTag(),
                                           tsHeader.getDataType());

        // copy bank header
        cd.bHeader = new BankHeader(bHeader.getTag(),
                                    bHeader.getDataType(),
                                    bHeader.getNumber());

        // copy ByteBuffer
        cd.dataBuffer = ByteBuffer.wrap(rawBytes, 4*dataOffset, 4*bHeader.getLength()).slice();
        cd.dataBuffer.order(byteOrder);

        // copy format ints
        cd.formatInts = new ArrayList<Integer>(formatInts.size());
        cd.formatInts.addAll(formatInts);

        // copy type items
        cd.types = new ArrayList<DataType>(types.size());
        cd.types.addAll(types);

        //-----------------
        // copy data items
        //-----------------

        // create new list of correct length
        int listLength = items.size();
        cd.items = new ArrayList<Object>(listLength);

        // copy each item, cloning those that are mutable
        for (int i=0; i < listLength; i++) {
            switch (types.get(i)) {
                // this is the only mutable type and so
                // it needs to be cloned specifically
                case CHARSTAR8:
                    String[] strs = (String[]) items.get(i);
                    cd.items.add(strs.clone());
                    break;
                default:
                    cd.items.add(items.get(i));
            }
        }

        return cd;
    }



    /**
     * This class is used to provide all data when constructing a CompositeData object.
     * Doing things this way keeps all internal data members self-consistent.
     */
    public static final class Data  {

        /** Convenient way to calculate padding. */
        static private int[] pads = {0,3,2,1};


        /** Keep a running total of how many bytes the data take without padding.
         *  This includes both the dataItems and N values and thus assumes all N
         *  values will be written. */
        private int dataBytes;

        /** The number of bytes needed to complete a 4-byte boundary. */
        private int paddingBytes;

        /** List of data objects. */
        private ArrayList<Object> dataItems = new ArrayList<Object>(100);

        /** List of types of data objects. */
        private ArrayList<DataType> dataTypes = new ArrayList<DataType>(100);

        /** List of "N" (32 bit) values - multiplier values read from data instead
         *  of being part of the format string. */
        private ArrayList<Integer> Nlist = new ArrayList<Integer>(100);

        /** List of "n" (16 bit) values - multiplier values read from data instead
         *  of being part of the format string. */
        private ArrayList<Short> nlist = new ArrayList<Short>(100);

        /** List of "m" (8 bit) values - multiplier values read from data instead
         *  of being part of the format string. */
        private ArrayList<Byte> mlist = new ArrayList<Byte>(100);

        /** Though currently not used, this is the tag in the segment containing format string. */
        private int formatTag;

        /** Though currently not used, this is the tag in the bank containing data. */
        private int dataTag;

        /** Though currently not used, this is the num in the bank containing data. */
        private int dataNum;



        /** Constructor. */
        public Data() {}


        /** Clear all existing data in this object to prepare for reuse. */
        public void clear() {
            dataItems.clear();
            dataTypes.clear();
            Nlist.clear();
            nlist.clear();
            mlist.clear();
            dataBytes = paddingBytes = formatTag = dataTag = dataNum = 0;
        }

        /**
         * Copy data from another object.
         * @param dataToCopy object to copy.
         */
        public void copy(Data dataToCopy) {
            dataBytes = dataToCopy.dataBytes;
            paddingBytes = dataToCopy.paddingBytes;
            formatTag = dataToCopy.formatTag;
            dataTag = dataToCopy.dataTag;
            dataNum = dataToCopy.dataNum;

            dataItems.clear();
            if (dataToCopy.dataItems.size() > 0) {
                dataItems.addAll(dataToCopy.dataItems);
            }

            dataTypes.clear();
            if (dataToCopy.dataTypes.size() > 0) {
                dataTypes.addAll(dataToCopy.dataTypes);
            }

            Nlist.clear();
            if (dataToCopy.Nlist.size() > 0) {
                Nlist.addAll(dataToCopy.Nlist);
            }

            nlist.clear();
            if (dataToCopy.nlist.size() > 0) {
                nlist.addAll(dataToCopy.nlist);
            }

            mlist.clear();
            if (dataToCopy.mlist.size() > 0) {
                mlist.addAll(dataToCopy.mlist);
            }
        }

        /**
         * This method sets the tag in the segment containing the format string.
         * @param tag tag in segment containing the format string.
         */
        public void setFormatTag(int tag) {
            this.formatTag = tag;
        }

        /**
         * This method sets the tag in the bank containing the data.
         * @param tag tag in bank containing the data.
         */
        public void setDataTag(int tag) {
            this.dataTag = tag;
        }

        /**
         * This method sets the num in the bank containing the data.
         * @param num num in bank containing the data.
         */
        public void setDataNum(int num) {
            this.dataNum = num;
        }

        /**
         * This method gets the tag in the segment containing the format string.
         * @return tag in segment containing the format string.
         */
        public int getFormatTag() {
            return formatTag;
        }

        /**
         * This method gets the tag in the bank containing the data.
         * @return tag in bank containing the data.
         */
        public int getDataTag() {
            return dataTag;
        }

        /**
         * This method gets the num in the bank containing the data.
         * @return num in bank containing the data.
         */
        public int getDataNum() {
            return dataNum;
        }

        /**
         * This method keeps track of the raw data size in bytes.
         * Also keeps track of padding.
         * @param bytes number of bytes to add.
         */
        private void addBytesToData(int bytes) {
            dataBytes += bytes;
            // Pad to 4 byte boundaries.
            paddingBytes = pads[dataBytes%4];
        }

        /**
         * This method gets the raw data size in bytes.
         * @return raw data size in bytes.
         */
        synchronized public int getDataSize() {
            return (dataBytes + paddingBytes);
        }

        /**
         * This method gets the padding (in bytes).
         * @return padding (in bytes).
         */
        synchronized public int getPadding() {
            return paddingBytes;
        }

        /**
         * This method adds an "N" or multiplier value to the data.
         * It needs to be added in sequence with other data.
         * @param N  N or multiplier value
         */
        synchronized public void addN(int N) {
            Nlist.add(N);
            dataItems.add(N);
            dataTypes.add(DataType.NVALUE);
            addBytesToData(4);
        }

        /**
         * This method adds an "n" or multiplier value to the data.
         * It needs to be added in sequence with other data.
         * @param n  n or multiplier value
         */
        synchronized public void addn(short n) {
            nlist.add(n);
            dataItems.add(n);
            dataTypes.add(DataType.nVALUE);
            addBytesToData(2);
        }

        /**
         * This method adds an "m" or multiplier value to the data.
         * It needs to be added in sequence with other data.
         * @param m  m or multiplier value
         */
        synchronized public void addm(byte m) {
            mlist.add(m);
            dataItems.add(m);
            dataTypes.add(DataType.mVALUE);
            addBytesToData(1);
        }

        /**
         * Add a signed 32 bit integer to the data.
         * @param i integer to add.
         */
        synchronized public void addInt(int i) {
            dataItems.add(i);
            dataTypes.add(DataType.INT32);
            addBytesToData(4);
        }

        /**
         * Add an array of signed 32 bit integers to the data.
         * @param i array of integers to add.
         */
        synchronized public void addInt(int[] i) {
            for (int ii : i) {
                dataItems.add(ii);
                dataTypes.add(DataType.INT32);
            }
            addBytesToData(4*i.length);
        }

        /**
         * Add an unsigned 32 bit integer to the data.
         * In Java, there is no unsigned types. It is the
         * responsibility of the user to handle this type properly.
         * @param i unsigned integer to add.
         */
        synchronized public void addUint(int i) {
            dataItems.add(i);
            dataTypes.add(DataType.UINT32);
            addBytesToData(4);
        }

        /**
         * Add an array of unsigned 32 bit integers to the data.
         * In Java, there is no unsigned types. It is the
         * responsibility of the user to handle this type properly.
         * @param i array of unsigned integers to add.
         */
        synchronized public void addUint(int[] i) {
            for (int ii : i) {
                dataItems.add(ii);
                dataTypes.add(DataType.UINT32);
            }
            addBytesToData(4*i.length);
        }



        /**
         * Add a 16 bit short to the data.
         * @param s short to add.
         */
        synchronized public void addShort(short s) {
            dataItems.add(s);
            dataTypes.add(DataType.SHORT16);
            addBytesToData(2);
        }

        /**
         * Add an array of 16 bit shorts to the data.
         * @param s array of shorts to add.
         */
        synchronized public void addShort(short[] s) {
            for (short ss : s) {
                dataItems.add(ss);
                dataTypes.add(DataType.SHORT16);
            }
            addBytesToData(2*s.length);
        }

        /**
         * Add an unsigned 16 bit short to the data.
         * @param s unsigned short to add.
         */
        synchronized public void addUshort(short s) {
            dataItems.add(s);
            dataTypes.add(DataType.USHORT16);
            addBytesToData(2);
        }

        /**
         * Add an array of unsigned 16 bit shorts to the data.
         * @param s array of unsigned shorts to add.
         */
        synchronized public void addUshort(short[] s) {
            for (short ss : s) {
                dataItems.add(ss);
                dataTypes.add(DataType.USHORT16);
            }
            addBytesToData(2*s.length);
        }



        /**
         * Add a 64 bit long to the data.
         * @param l long to add.
         */
        synchronized public void addLong(long l) {
            dataItems.add(l);
            dataTypes.add(DataType.LONG64);
            addBytesToData(8);
        }

        /**
         * Add an array of 64 bit longs to the data.
         * @param l array of longs to add.
         */
        synchronized public void addLong(long[] l) {
            for (long ll : l) {
                dataItems.add(ll);
                dataTypes.add(DataType.LONG64);
            }
            addBytesToData(8*l.length);
        }

        /**
         * Add an unsigned 64 bit long to the data.
         * @param l unsigned long to add.
         */
        synchronized public void addUlong(long l) {
            dataItems.add(l);
            dataTypes.add(DataType.ULONG64);
            addBytesToData(8);
        }

        /**
         * Add an array of unsigned 64 bit longs to the data.
         * @param l array of unsigned longs to add.
         */
        synchronized public void addUlong(long[] l) {
            for (long ll : l) {
                dataItems.add(ll);
                dataTypes.add(DataType.ULONG64);
            }
            addBytesToData(8*l.length);
        }



        /**
         * Add an 8 bit byte (char) to the data.
         * @param b byte to add.
         */
        synchronized public void addChar(byte b) {
            dataItems.add(b);
            dataTypes.add(DataType.CHAR8);
            addBytesToData(1);
        }

        /**
         * Add an array of 8 bit bytes (chars) to the data.
         * @param b array of bytes to add.
         */
        synchronized public void addChar(byte[] b) {
            for (byte bb : b) {
                dataItems.add(bb);
                dataTypes.add(DataType.CHAR8);
            }
            addBytesToData(b.length);
        }

        /**
         * Add an unsigned 8 bit byte (uchar) to the data.
         * @param b unsigned byte to add.
         */
        synchronized public void addUchar(byte b) {
            dataItems.add(b);
            dataTypes.add(DataType.UCHAR8);
            addBytesToData(1);
        }

        /**
         * Add an array of unsigned 8 bit bytes (uchars) to the data.
         * @param b array of unsigned bytes to add.
         */
        synchronized public void addUchar(byte[] b) {
            for (byte bb : b) {
                dataItems.add(bb);
                dataTypes.add(DataType.UCHAR8);
            }
            addBytesToData(b.length);
        }



        /**
         * Add a 32 bit float to the data.
         * @param f float to add.
         */
        synchronized public void addFloat(float f) {
            dataItems.add(f);
            dataTypes.add(DataType.FLOAT32);
            addBytesToData(4);
        }

        /**
         * Add an array of 32 bit floats to the data.
         * @param f array of floats to add.
         */
        synchronized public void addFloat(float[] f) {
            for (float ff : f) {
                dataItems.add(ff);
                dataTypes.add(DataType.FLOAT32);
            }
            addBytesToData(4*f.length);
        }



        /**
         * Add a 64 bit double to the data.
         * @param d double to add.
         */
        synchronized public void addDouble(double d) {
            dataItems.add(d);
            dataTypes.add(DataType.DOUBLE64);
            addBytesToData(8);
        }

        /**
          * Add an array of 64 bit doubles to the data.
          * @param d array of doubles to add.
          */
        synchronized public void addDouble(double[] d) {
            for (double dd : d) {
                dataItems.add(dd);
                dataTypes.add(DataType.DOUBLE64);
            }
            addBytesToData(8*d.length);
        }



        /**
         * Add a string to the data.
         * @param s string to add.
         */
        synchronized public void addString(String s) {
            String[] ss = new String[] {s};
            dataItems.add(ss);
            dataTypes.add(DataType.CHARSTAR8);
            addBytesToData(BaseStructure.stringsToRawSize(ss));
        }

        /**
         * Add an array of strings to the data.
         * @param s array of strings to add.
         */
        synchronized public void addString(String[] s) {
            dataItems.add(s);
            dataTypes.add(DataType.CHARSTAR8);
            addBytesToData(BaseStructure.stringsToRawSize(s));
        }
    }


    /**
     * This method helps the CompositeData object creator by
     * finding the proper format string parameter for putting
     * this array of Strings into its data.
     * The format is in the form "Ma" where M is an actual integer.
     * Warning, in this case, M may not be greater than 15.
     * If you want a longer string or array of strings, use the
     * format "Na" with a literal N. The N value can be added
     * through {@link org.jlab.coda.jevio.CompositeData.Data#addN(int)}
     *
     * @param strings array of strings to eventually put into a
     *                CompositeData object.
     * @return string representing its format to be used in the
     *                CompositeData object's format string, or
     *                null if arg is null or has 0 length.
     */
    public static String stringsToFormat(String[] strings) {
        byte[] rawBuf = BaseStructure.stringsToRawBytes(strings);
        if (rawBuf != null) {
            return rawBuf.length + "a";
        }

        return null;
    }

    /**
     * Get the data padding (0, 1, 2, or 3 bytes).
     * @return data padding.
     */
    public int getPadding() {return dataPadding;}

    /**
     * This method gets the format string.
     * @return format string.
     */
    public String getFormat() { return format; }


    /**
     * This method gets the raw data byte order.
     * @return raw data byte order.
     */
    public ByteOrder getByteOrder() {
        return byteOrder;
    }


    /**
     * This method gets the raw byte representation of this object's data.
     * @return raw byte representation of this object's data.
     */
    public byte[] getRawBytes() {
        return rawBytes;
    }

    /**
     * This method gets a list of all the data items inside the composite.
     * @return list of all the data items inside the composite.
     */
    public List<Object> getItems() {
        return items;
    }

    /**
     * This method gets a list of all the types of the data items inside the composite.
     * @return list of all the types of the data items inside the composite.
     */
    public List<DataType> getTypes() {
        return types;
    }

    /**
     * This method gets a list of all the N values of the data items inside the composite.
     * @return list of all the N values of the data items inside the composite.
     */
    public List<Integer> getNValues() {
        return NList;
    }

    /**
     * This method gets a list of all the n values of the data items inside the composite.
     * @return list of all the n values of the data items inside the composite.
     */
    public List<Short> getnValues() {
        return nList;
    }

    /**
     * This method gets a list of all the m values of the data items inside the composite.
     * @return list of all the m values of the data items inside the composite.
     */
    public List<Byte> getmValues() {
        return mList;
    }

    /**
     * This methods returns the index of the data item to be returned
     * on the next call to one of the get&lt;Type&gt;() methods
     * (e.g. {@link #getInt()}.
     * @return returns the index of the data item to be returned
     */
    public int index() {
        return getIndex;
    }

    /**
     * This methods sets the index of the data item to be returned
     * on the next call to one of the get&lt;Type&gt;() methods
     * (e.g. {@link #getInt()}.
     * @param index the index of the next data item to be returned
     */
    public void index(int index) {
        this.getIndex = index;
    }

    /**
     * This method gets the next data item as an Integer object.
     * @return Integer object, if that is the correct type of the next data item,
     *         or null if no more data items or data item is not a 32 bit signed or
     *         unsigned integer
     */
    public Integer getInt() {
        // Past end of data, return null
        if (getIndex > types.size()) {
            return null;
        }

        // Check for type mismatch to avoid ClassCastException
        DataType type = types.get(getIndex);
        if (type != DataType.INT32 && type != DataType.UINT32) {
            return null;
        }

        return (Integer) (items.get(getIndex++));
    }

    /**
     * This method gets the next N value item as an Integer object.
     * @return Integer object, if the correct type of the next data item is NVALUE,
     *         or null if no more data items or data item is not an NVALUE type.
     */
    public Integer getNValue() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.NVALUE) return null;
        return (Integer) (items.get(getIndex++));
    }

    /**
     * This method gets the next n value item as an Integer object.
     * @return Integer object, if the correct type of the next data item is nVALUE,
     *         or null if no more data items or data item is not an nVALUE type.
     */
    public Integer getnValue() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.nVALUE) return null;
        return ((Short)(items.get(getIndex++))).intValue() & 0xffff;
    }

    /**
     * This method gets the next m value item as an Integer object.
     * @return Integer object, if the correct type of the next data item is mVALUE,
     *         or null if no more data items or data item is not an mVALUE type.
     */
    public Integer getmValue() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.mVALUE) return null;
        return ((Byte)(items.get(getIndex++))).intValue() & 0xff;
    }

    /**
     * This method gets the next data item (which is of Hollerit type) as an Integer object.
     * @return Integer object, if Hollerit is the type of the next data item,
     *         or null if no more data items or data item is not a 32 bit signed or
     *         unsigned integer
     */
    public Integer getHollerit() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.HOLLERIT) return null;
        return (Integer) (items.get(getIndex++));
    }

    /**
     * This method gets the next data item as a Byte object.
     * @return Byte object, if that is the correct type of the next data item,
     *         or null if no more data items or data item is not a 8 bit signed or
     *         unsigned integer
     */
    public Byte getByte() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.CHAR8 && type != DataType.UCHAR8) return null;
        return (Byte) (items.get(getIndex++));
    }

    /**
     * This method gets the next data item as a Short object.
     * @return Short object, if that is the correct type of the next data item,
     *         or null if no more data items or data item is not a 16 bit signed or
     *         unsigned integer
     */
    public Short getShort() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.SHORT16 && type != DataType.USHORT16) return null;
        return (Short) (items.get(getIndex++));
    }

    /**
     * This method gets the next data item as a Long object.
     * @return Long object, if that is the correct type of the next data item,
     *         or null if no more data items or data item is not a 64 bit signed or
     *         unsigned integer
     */
    public Long getLong() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.LONG64 && type != DataType.ULONG64) return null;
        return (Long) (items.get(getIndex++));
    }

    /**
     * This method gets the next data item as a Float object.
     * @return Float object, if that is the correct type of the next data item,
     *         or null if no more data items or data item is not a 32 bit float.
     */
    public Float getFloat() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.FLOAT32) return null;
        return (Float) (items.get(getIndex++));
    }

    /**
     * This method gets the next data item as a Double object.
     * @return Double object, if that is the correct type of the next data item,
     *         or null if no more data items or data item is not a 32 bit double.
     */
    public Double getDouble() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.DOUBLE64) return null;
        return (Double) (items.get(getIndex++));
    }

    /**
     * This method gets the next data item as a String array.
     * @return String array, if that is the correct type of the next data item,
     *         or null if no more data items or data item is not a string or string array.
     */
    public String[] getStrings() {
        if (getIndex > types.size()) return null;
        DataType type = types.get(getIndex);
        if (type != DataType.CHARSTAR8)  return null;
        return (String[]) (items.get(getIndex++));
    }


    /**
     *  This method transforms a composite, format-containing
     *  ASCII string to an int array. It is to be used in
     *  conjunction with {@link #swapData(ByteBuffer, ByteBuffer, int, int, List)}
     *  to swap the endianness of composite data.
     *  It's translated from the eviofmt C function.
     * <pre><code>
     *   format code bits &lt;- format in ascii form
     *    [15:14] [13:8] [7:0]
     *      Nnm      #     0           #'('
     *        0      0     0            ')'
     *      Nnm      #     1           #'i'   unsigned int
     *      Nnm      #     2           #'F'   floating point
     *      Nnm      #     3           #'a'   8-bit char (C++)
     *      Nnm      #     4           #'S'   short
     *      Nnm      #     5           #'s'   unsigned short
     *      Nnm      #     6           #'C'   char
     *      Nnm      #     7           #'c'   unsigned char
     *      Nnm      #     8           #'D'   double (64-bit float)
     *      Nnm      #     9           #'L'   long long (64-bit int)
     *      Nnm      #    10           #'l'   unsigned long long (64-bit int)
     *      Nnm      #    11           #'I'   int
     *      Nnm      #    12           #'A'   hollerit (4-byte char with int endining)
     *
     *   NOTES:
     *    1. The number of repeats '#' must be the number between 2 and 63, number 1 assumed by default
     *    2. If the number of repeats is symbol 'N' instead of the number, it will be taken from data assuming 'int32' format;
     *       if the number of repeats is symbol 'n' instead of the number, it will be taken from data assuming 'int16' format;
     *       if the number of repeats is symbol 'm' instead of the number, it will be taken from data assuming 'int8' format;
     *       Two bits Nnm [15:14], if not zero, requires to take the number of repeats from data in appropriate format:
     *            [01] means that number is integer (N),
     *            [10] - short (n),
     *            [11] - char (m)
     *    3. If format ends but end of data did not reach, format in last parenthesis
     *       will be repeated until all data processed; if there are no parenthesis
     *       in format, data processing will be started from the beginning of the format
     *       (FORTRAN agreement)
     * </code></pre>
     *  @param  fmt composite data format string
     *  @return List of ints resulting from transformation of "fmt" string
     *  @throws EvioException if improper format string
     */
    public static List<Integer> compositeFormatToInt(String fmt) throws EvioException {

        char ch;
        int l, n, kf, lev, nr, nn, nb;
        boolean debug = false;

        ArrayList<Integer> ifmt = new ArrayList<Integer>(2*fmt.length());
//        int ifmtLen = ifmt.size();

        n   = 0; // ifmt[] index
        nr  = 0;
        nn  = 1;
        lev = 0;
        nb  = 0; // the number of bytes in length taken from data

        if (debug) {
            System.out.println("\\n=== eviofmt start, fmt >" + fmt + "< ===\n");
        }

        // loop over format string character-by-character
        for (l=0; l < fmt.length(); l++) {
            ch = fmt.charAt(l);

            if (ch == ' ') continue;

            if (debug) {
                System.out.println("ch = " + ch);
            }

            // if char = digit, following before comma will be repeated 'number' times
            if (Character.isDigit(ch)) {
                if (debug) {
                    System.out.println("the number of repeats before, nr = " + nr);
                }
                if (nr < 0) throw new EvioException("no negative repeats");
                nr = 10*Math.max(0,nr) + Character.digit(ch,10);
                if (nr > 15) throw new EvioException("no more than 15 repeats allowed");
                if (debug) {
                    System.out.println("the number of repeats nr = " + nr);
                }
            }
            // a left parenthesis -> 16*nr + 0
            else if (ch == '(')  {
                if (nr < 0) throw new EvioException("no negative repeats");
//                if (--ifmtLen < 0) throw new EvioException("ifmt array too small (" + ifmt.size() + ")");
                lev++;

                if (debug) {
                    System.out.println(String.format("111: nn=%d nr=%d\n",nn,nr));
                }

                // special case: if # repeats is in data, set bits [15:14]
                if (nn == 0) {
                    if (nb == 4)      {ifmt.add(1 << 14); n++;}
                    else if (nb == 2) {ifmt.add(2 << 14); n++;}
                    else if (nb == 1) {ifmt.add(3 << 14); n++;}
                    else {throw new EvioException("unknown nb = "+ nb);}
                }
                // # repeats hardcoded
                else {
                    ifmt.add((Math.max(nn,nr) & 0x3F) << 8);
                    n++;
                }

                nn = 1;
                nr = 0;

                if (debug) {
                    int ii = n - 1;
                    String s = String.format("  [%3d] S%3d (16 * %2d + %2d), nr=%d\n",
                                             ii,ifmt.get(ii),ifmt.get(ii)/16,
                                             ifmt.get(ii)-(ifmt.get(ii)/16)*16,nr);
                    System.out.println(s);
                }

            }
            // a right parenthesis -> (0<<8) + 0
            else if (ch == ')') {
                if (nr >= 0) throw new EvioException("cannot repeat right parenthesis");
//                if (--ifmtLen < 0) throw new EvioException("ifmt array too small (" + ifmt.size() + ")");
                lev--;
                ifmt.add(0);
                n++;
                nr = -1;

                if (debug) {
                    int ii = n - 1;
                    String s = String.format("  [%3d] S%3d (16 * %2d + %2d), nr=%d\n",
                                             ii,ifmt.get(ii),ifmt.get(ii)/16,
                                             ifmt.get(ii)-(ifmt.get(ii)/16)*16,nr);
                    System.out.println(s);
                }

            }
            // a comma, reset nr
            else if (ch == ',') {
                if (nr >= 0) throw new EvioException("cannot repeat comma");
                nr = 0;
                if (debug)
                    System.out.println(String.format("comma, nr=%d",nr));

            }
            // variable length format, int
            else if (ch == 'N') {
                nn = 0;
                nb = 4;
                if (debug)
                    System.out.println("N, nb=4");
            }
            // variable length format, short
            else if (ch == 'n') {
                nn = 0;
                nb = 2;
                if (debug)
                    System.out.println("n, nb=2");
            }
            // variable length format, byte
            else if (ch == 'm') {
                nn = 0;
                nb = 1;
                if (debug)
                    System.out.println("m, nb=1");
            }
            // actual format
            else {
                if      (ch == 'i') kf = 1;  // 32
                else if (ch == 'F') kf = 2;  // 32
                else if (ch == 'a') kf = 3;  //  8
                else if (ch == 'S') kf = 4;  // 16
                else if (ch == 's') kf = 5;  // 16
                else if (ch == 'C') kf = 6;  //  8
                else if (ch == 'c') kf = 7;  //  8
                else if (ch == 'D') kf = 8;  // 64
                else if (ch == 'L') kf = 9;  // 64
                else if (ch == 'l') kf = 10; // 64
                else if (ch == 'I') kf = 11; // 32
                else if (ch == 'A') kf = 12; // 32
                else                kf = 0;

                if (kf != 0) {
                    if (debug) {
                        System.out.println(String.format("222: nn=%d nr=%d\n",nn,nr));
                    }

                    if (nr < 0) throw new EvioException("no negative repeats");
//                    if (--ifmtLen < 0) throw new EvioException("ifmt array too small (" + ifmt.size() + ")");

                    int ifmtVal = ((Math.max(nn,nr) & 0x3F) << 8) + kf;

                    if (nb > 0) {
                        if (nb==4)      ifmtVal |= (1 << 14);
                        else if (nb==2) ifmtVal |= (2 << 14);
                        else if (nb==1) ifmtVal |= (3 << 14);
                        else {throw new EvioException("unknown nb="+ nb);}
                    }

                    ifmt.add(ifmtVal);
                    n++;
                    nn=1;

                    if (debug) {
                        int ii = n - 1;
                        String s = String.format("  [%3d] S%3d (16 * %2d + %2d), nr=%d\n",
                                                 ii,ifmt.get(ii),ifmt.get(ii)/16,
                                                 ifmt.get(ii)-(ifmt.get(ii)/16)*16,nr);
                        System.out.println(s);
                    }
                }
                else {
                    throw new EvioException("illegal character (value " + (byte)ch + ")");
                }
                nr = -1;
            }
        }

        if (lev != 0) {
            throw new EvioException("mismatched number of right/left parentheses");
        }

        if (debug) System.out.println("=== eviofmt end ===");

        return (ifmt);
    }


    /**
      * This method swaps the data of this composite type between big and
      * little endian. It swaps the entire type including the beginning tagsegment
      * header, the following format string it contains, the data's bank header,
      * and finally the data itself.
      *
      * @throws EvioException if internal error
      */
     public void swap() throws EvioException {
         swapAll(rawBytes, 0, null, 0, rawBytes.length/4, byteOrder);
         byteOrder = (byteOrder == ByteOrder.LITTLE_ENDIAN) ?
                     ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN;
         dataBuffer.order(byteOrder);
     }
     

    /**
     * This method converts (swaps) a buffer of EVIO composite type between big and
     * little endian. It swaps the entire type including the beginning tagsegment
     * header, the following format string it contains, the data's bank header,
     * and finally the data itself. The src array may contain an array of
     * composite type items and all will be swapped. If swapping in place,
     * destOff set equal to srcOff.
     *
     * @param src      source data array (of 32 bit words)
     * @param srcOff   # of bytes offset into source data array
     * @param dest     destination data array (of 32 bit words)
     * @param destOff  # of bytes offset into destination data array
     * @param length   length of data array in 32 bit words
     * @param srcOrder the byte order of data in src
     *
     * @throws EvioException if offsets &lt; 0; if length &lt; 4; if src = null;
     *                       if src or dest is too small
     */
    public static void swapAll (byte[] src, int srcOff, byte[] dest, int destOff,
                                int length, ByteOrder srcOrder) throws EvioException {

        if (src == null) {
            throw new EvioException("null argument(s)");
        }

        boolean inPlace = false;
        if (dest == null || dest == src) {
            dest = src;
            destOff = srcOff;
            inPlace = true;
        }

        if (srcOff < 0 || destOff < 0) {
            throw new EvioException("offsets must be >= 0");
        }

        if (length < 4) {
            throw new EvioException("length must be >= 4");
        }

        // Byte order of swapped data
        ByteOrder destOrder = (srcOrder == ByteOrder.BIG_ENDIAN) ?
                               ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN;

        // How many unused bytes are left in the src array?
        int totalBytes   = 4*length;
        int srcBytesLeft = totalBytes;

        // How many bytes taken for this CompositeData object?
        int dataOff = 0;

        // Wrap input & output arrays in ByteBuffers for convenience
        ByteBuffer  srcBuffer = ByteBuffer.wrap( src, srcOff,  4*length);
        ByteBuffer destBuffer = ByteBuffer.wrap(dest, destOff, 4*length);

        // Here is where we do some of the swapping
        srcBuffer.order(srcOrder);
        destBuffer.order(destOrder);

//Utilities.printBytes(src, srcOff, 32, "CD src bytes");

        while (srcBytesLeft > 0) {
//System.out.println("start src offset = " + (srcOff + dataOff));

            // First read the tag segment header
            TagSegmentHeader tsHeader = EventParser.createTagSegmentHeader(src, srcOff + dataOff, srcOrder);
            int headerLen  = tsHeader.getHeaderLength();
            int dataLength = tsHeader.getLength() - (headerLen - 1);

//System.out.println("tag len = " + tsHeader.getLength() + ", dataLen = " + dataLength);

            // Oops, no format data
            if (dataLength < 1) {
                throw new EvioException("no format data");
            }

            // Got all we needed from the tagseg header, now swap as it's written out.
            tsHeader.write(destBuffer);

            // Move to beginning of string data
            dataOff += 4*headerLen;

            // Read the format string it contains
            String[] strs = BaseStructure.unpackRawBytesToStrings(src, srcOff + dataOff,
                                                                  4*(tsHeader.getLength()));

            if (strs.length < 1) {
                throw new EvioException("bad format string data");
            }
            String format = strs[0];

            // Transform string format into int array format
            List<Integer> formatInts = compositeFormatToInt(format);
            if (formatInts.size() < 1) {
                throw new EvioException("bad format string data");
            }

            // Char data does not get swapped but needs
            // to be copied if not swapping in place.
            if (!inPlace) {
                System.arraycopy(src,   srcOff + dataOff,
                                 dest, destOff + dataOff, 4*dataLength);
            }

            // Move to beginning of bank header
            dataOff += 4*dataLength;

            // Read the data bank header
            BankHeader bHeader = EventParser.createBankHeader(src, srcOff + dataOff, srcOrder);
            headerLen  = bHeader.getHeaderLength();
            dataLength = bHeader.getLength() - (headerLen - 1);

//System.out.println("swapAll: bank len = " + bHeader.getLength() + ", dataLen = " + dataLength +
//        ", tag = " + bHeader.getTag() +
//        ", num = " + bHeader.getNumber() +
//        ", type = " + bHeader.getDataTypeName() +
//        ", pad = " + bHeader.getPadding());

            // Oops, no data
            if (dataLength < 1) {
                throw new EvioException("no data");
            }

            // Adjust data length by switching units from
            // ints to bytes and accounting for padding.
            int padding = bHeader.getPadding();
            dataLength = 4*dataLength - padding;

            // Got all we needed from the bank header, now swap as it's written out.
            destBuffer.position(destOff + dataOff);
            bHeader.write(destBuffer);

            // Move to beginning of data
            dataOff += 4*headerLen;
            srcBuffer.position(  srcOff + dataOff);
            destBuffer.position(destOff + dataOff);

            // Swap data
            swapData(srcBuffer, destBuffer, dataLength, padding, formatInts);

            // Set buffer positions and offset
            dataOff += dataLength + padding;
            srcBuffer.position( srcOff + dataOff);
            destBuffer.position(srcOff + dataOff);

            srcBytesLeft = totalBytes - dataOff;

//System.out.println("bytes left = " + srcBytesLeft + ",offset = " + dataOff + ", padding = " + padding);
//System.out.println("src pos = " + srcBuffer.position() + ", dest pos = " + destBuffer.position());
        }

        // Oops, things aren't coming out evenly
        if (srcBytesLeft != 0) {
            throw new EvioException("bad format");
        }
    }


    /**
     * This method converts (swaps) a buffer, containing EVIO composite type,
     * between big and little endian. It swaps the entire type including the beginning
     * tagsegment header, the following format string it contains, the data's bank header,
     * and finally the data itself. The src buffer may contain an array of
     * composite type items and all will be swapped.
     *
     * @param srcBuffer   source data buffer
     * @param destBuffer  destination data buffer
     * @param srcPos      position in srcBuffer to beginning swapping
     * @param destPos     position in destBuffer to beginning writing swapped data
     * @param len         length of data in srcBuffer to swap in 32 bit words
     * @param inPlace     if true, swap in place.
     *
     * @throws EvioException if srcBuffer not in evio format;
     *                       if destBuffer too small;
     *                       if bad values for srcPos/destPos/len args;
     */
    public static void swapAll(ByteBuffer srcBuffer, ByteBuffer destBuffer,
                               int srcPos, int destPos, int len, boolean inPlace)
                throws EvioException {

        // Minimum size of 4 words for composite data
        if (len < 4) {
            throw new EvioException("len arg must be >= 4");
        }

        if (4*len > destBuffer.limit() - destPos) {
            throw new EvioException("not enough room in destination buffer");
        }

        // Bytes to swap
        int totalBytes   = 4*len;
        int srcBytesLeft = totalBytes;
        int dataOff, byteLen;

        // Initialize
        dataOff = 0;

        while (srcBytesLeft > 0) {

            // Read & swap string tagsegment header
            EvioNode node = new EvioNode();
            ByteDataTransformer.swapTagSegmentHeader(node, srcBuffer, destBuffer, srcPos, destPos);

            // Move to beginning of string data
            srcPos  += 4;
            destPos += 4;
            dataOff += 4;

            // String data length in bytes
            byteLen = 4*node.dataLen;

            // Read the format string it contains
            String[] strs = BaseStructure.unpackRawBytesToStrings(srcBuffer, srcPos, byteLen);

            if (strs.length < 1) {
                throw new EvioException("bad format string data");
            }
            String format = strs[0];

            // Transform string format into int array format
            List<Integer> formatInts = compositeFormatToInt(format);
            if (formatInts.size() < 1) {
                throw new EvioException("bad format string data");
            }

            // Char data does not get swapped but needs
            // to be copied if not swapping in place.
            if (!inPlace) {
                for (int i=0; i < byteLen; i++) {
                    destBuffer.put(destPos+i, srcBuffer.get(srcPos+i));
                }
            }

            // Move to beginning of bank header
            srcPos  += byteLen;
            destPos += byteLen;
            dataOff += byteLen;

            // Read & swap data bank header
            ByteDataTransformer.swapBankHeader(node, srcBuffer, destBuffer, srcPos, destPos);

            // Oops, no data
            if (node.dataLen < 1) {
                throw new EvioException("no data");
            }

            // Move to beginning of bank data
            srcPos  += 8;
            destPos += 8;
            dataOff += 8;

            // Bank data length in bytes
            byteLen = 4*node.dataLen;

            // Swap data (accounting for padding)
            swapData(srcBuffer, destBuffer, srcPos, destPos, (byteLen - node.pad), node.pad, formatInts);

            // Move past bank data
            srcPos    += byteLen;
            destPos   += byteLen;
            dataOff   += byteLen;
            srcBytesLeft  = totalBytes - dataOff;

            //System.out.println("bytes left = " + srcBytesLeft);
            //System.out.println("src pos = " + srcBuffer.position() + ", dest pos = " + destBuffer.position());
        }

        // Oops, things aren't coming out evenly
        if (srcBytesLeft != 0) {
            throw new EvioException("bad format");
        }
    }




    /**
     * This method converts (swaps) an array of EVIO composite type data
     * between IEEE (big endian) and DECS (little endian). This
     * data does <b>NOT</b> include the composite type's beginning tagsegment and
     * the format string it contains. It also does <b>NOT</b> include the data's
     * bank header words. If swapping in place, destOff set equal to srcOff.
     * The dest array can be null or the same as srcBuf in which case data
     * is swapped in place.
     *
     * @param src      source data array (of 32 bit words)
     * @param srcOff   offset into source data array
     * @param dest     destination data array (of 32 bit words)
     * @param destOff  offset into destination data array
     * @param nBytes   length of data to swap in bytes (be sure to subtract padding)
     * @param padding  # of padding bytes at end.
     * @param ifmt     format list as produced by {@link #compositeFormatToInt(String)}
     * @param srcOrder byte order of the src data array
     *
     * @throws EvioException if src == null or ifmt == null;
     *                       if nBytes &lt; 8, or ifmt size &lt; 1;
     *                       srcOff or destOff negative;
     *                       buffer limit/position combo too small;
     */
    public static void swapData(byte[] src, int srcOff, byte[] dest, int destOff,
                                int nBytes, int padding, List<Integer> ifmt, ByteOrder srcOrder)
                        throws EvioException {

        if (src == null) throw new EvioException("src arg cannot be null");

        ByteBuffer srcBuf = ByteBuffer.wrap(src, srcOff, nBytes);
        srcBuf.order(srcOrder);

        ByteBuffer destBuf = null;

        if (src != dest && dest != null) {
            destBuf = ByteBuffer.wrap(dest, destOff, nBytes);
        }

        swapData(srcBuf, destBuf, nBytes, padding, ifmt);
    }


    /**
     * This method converts (swaps) EVIO composite type data
     * between Big endian and Little endian. This
     * data does <b>NOT</b> include the composite type's beginning tagsegment and
     * the format string it contains. It also does <b>NOT</b> include the data's
     * bank header words. Caller must be sure the endian value of the srcBuf
     * is set properly before the call.<p>
     * The destBuf can be null or the same as srcBuf in which case data
     * is swapped in place and the srcBuf byte order is switched in this method.
     *
     * @param srcBuf   source data buffer
     * @param destBuf  destination data buffer; if null, use srcBuf as destination
     * @param nBytes   length of data to swap in bytes (be sure to subtract padding)
     * @param padding  # of padding bytes at end.
     * @param ifmt     format list as produced by {@link #compositeFormatToInt(String)}
     *
     * @throws EvioException if ifmt null; ifmt size &lt; 1; nBytes &lt; 8;
     *                       srcBuf is null;
     *                       buffer limit/position combo too small;
     */
    public static void swapData(ByteBuffer srcBuf, ByteBuffer destBuf,
                                int nBytes, int padding, List<Integer> ifmt)
                        throws EvioException {

        if (srcBuf == null) {
            throw new EvioException("srcBuf arg is null");
        }

        int destPos;
        if (destBuf != null) {
            destPos = destBuf.position();
        }
        else {
            destPos = srcBuf.position();
        }

        swapData(srcBuf, destBuf, srcBuf.position(), destPos, nBytes, padding, ifmt);
    }



    /**
     * This method converts (swaps) EVIO composite type data
     * between Big endian and Little endian. This
     * data does <b>NOT</b> include the composite type's beginning tagsegment and
     * the format string it contains. It also does <b>NOT</b> include the data's
     * bank header words. Caller must be sure the endian value of the srcBuf
     * is set properly before the call.<p>
     * The destBuf can be null or the same as srcBuf in which case data
     * is swapped in place and the srcBuf byte order is switched in this method.
     * The positions of both srcBuf and destBuf are NOT changed.
     *
     * @param srcBuf   source data buffer
     * @param destBuf  destination data buffer; if null, use srcBuf as destination
     * @param srcPos   position in srcBuf to beginning swapping
     * @param destPos  position in destBuf to beginning writing swapped data
     * @param nBytes   length of data to swap in bytes (be sure to subtract padding)
     * @param padding  # of padding bytes at end.
     * @param ifmt     format list as produced by {@link #compositeFormatToInt(String)}
     *
     * @throws EvioException if ifmt null; ifmt size &lt; 1; nBytes &lt; 8;
     *                       srcBuf is null; srcPos or destPos negative;
     *                       buffer limit/position combo too small;
     */
    public static void swapData(ByteBuffer srcBuf, ByteBuffer destBuf,
                                int srcPos, int destPos, int nBytes, int padding, List<Integer> ifmt)
                        throws EvioException {

        boolean debug = false;
        int imt, ncnf, kcnf, mcnf, lev, iterm;

        // check args
        if (ifmt == null) {
            throw new EvioException("ifmt arg null");
        }

        if (nBytes < 8) {
            throw new EvioException("nBytes < 8, too small");
        }

        // size of int list
        int nfmt = ifmt.size();
        if (nfmt <= 0) throw new EvioException("empty format list");

        if (srcBuf == null) {
            throw new EvioException("srcBuf arg null");
        }

        boolean inPlace = false;
        ByteOrder destOrder;

        if (destBuf == null || srcBuf == destBuf) {
            // The trick is to swap in place and that can only be done if
            // we have 2 separate buffers with different endianness.
            // So duplicate the buffer which shares content but allows
            // different endianness.
            destBuf = srcBuf.duplicate();
            // Byte order of swapped data
            destOrder = (srcBuf.order() == ByteOrder.BIG_ENDIAN) ?
                                           ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN;

            destBuf.order(destOrder);
            destPos = srcPos;
            inPlace = true;
        }
        else {
            destOrder = destBuf.order();

            // Clear padding so double swapped data can be checked easily
            for (int i=0; i < padding; i++) {
                destBuf.put(destPos + nBytes + i, (byte)0);
            }
        }

        // Check position args
        if (srcPos < 0 || destPos < 0) {
            throw new EvioException("no neg values for pos args");
        }

        // Check to see if buffers are too small to handle this operation
        if ((srcBuf.limit() < nBytes + srcPos) || (destBuf.limit() < nBytes + destPos)) {
            throw new EvioException("buffer(s) too small to handle swap, decrease pos or increase limit");
        }

        LV[] lv = new LV[10];
        for (int i=0; i < lv.length; i++) {
            lv[i] = new LV();
        }

        imt   = 0;  // ifmt[] index
        lev   = 0;  // parenthesis level
        ncnf  = 0;  // how many times must repeat a format
        iterm = 0;

        // just past end of src data
        int srcEndIndex = srcPos + nBytes;

        // Sergey's original comments:
        //
        //   Converts the data of array (iarr[i], i=0...nwrd-1)
        //   using the format code      (ifmt[j], j=0...nfmt-1) .
        //
        //   Algorithm description:
        //      Data processed inside while(ib < nwrd) loop, where 'ib' is iarr[] index;
        //      loop breaks when 'ib' reaches the number of elements in iarr[]
        //
        //   Timmer's note: nwrd = # of 32 bit words in data,
        //                  but that's now been replaced by the
        //                  num of bytes since data may not be an
        //                  even multiple of 4 bytes.

        while (srcPos < srcEndIndex) {

            if (debug) System.out.println(String.format("+++ begin = %d, end = %d\n", srcPos, srcEndIndex));

            // get next format code
            while (true) {
                imt++;
                // end of format statement reached, back to iterm - last parenthesis or format begining
                if (imt > nfmt) {
                    //imt = iterm;
                    imt = 0;
                    if (debug) System.out.println("1\n");
                }
                // meet right parenthesis, so we're finished processing format(s) in parenthesis
                else if (ifmt.get(imt-1) == 0) {
                    // increment counter
                    lv[lev-1].irepeat++;

                    // if format in parenthesis was processed the required number of times
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        // store left parenthesis index minus 1
                        // (if will meet end of format, will start from format index imt=iterm;
                        // by default we continue from the beginning of the format (iterm=0)) */
                        iterm = lv[lev-1].left - 1;
                        // done with this level - decrease parenthesis level
                        lev--;

                        if (debug) System.out.println("2\n");
                    }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        // points ifmt[] index to the left parenthesis from the same level 'lev'
                        imt = lv[lev-1].left;
                        if (debug) System.out.println("3\n");
                    }
                }
                else {
                    ncnf = (ifmt.get(imt-1) >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  ifmt.get(imt-1) & 0xFF;        /* format code */
                    mcnf = (ifmt.get(imt-1) >> 14) & 0x3;  /* repeat code */

                    // left parenthesis - set new lv[]
                    if (kcnf == 0) {

                        // check repeats for left parenthesis

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as int
                        if (mcnf == 1) {
                            // set it to regular left parenthesis code
                            mcnf = 0;

                            // read "N" value from buffer
                            // (srcBuf automagically swaps in the getInt)
                            ncnf = srcBuf.getInt(srcPos);

                            // put swapped val back into buffer
                            // (destBuf automagically swaps in the putInt)
                            destBuf.putInt(destPos, ncnf);

                            srcPos  += 4;
                            destPos += 4;

                            if (debug) System.out.println("ncnf from data = " + ncnf + " (code 15)");
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as short
                        if (mcnf == 2) {
                            mcnf = 0;
                            ncnf = srcBuf.getShort(srcPos) & 0xffff;
                            destBuf.putShort(destPos, (short)ncnf);
                            srcPos  += 2;
                            destPos += 2;
                            if (debug) System.out.println("ncnf from data = " + ncnf + " (code 14)");
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as byte
                        if (mcnf == 3) {
                            mcnf = 0;
                            ncnf = srcBuf.get(srcPos) & 0xff;
                            destBuf.put(destPos, (byte)ncnf);
                            srcPos++;
                            destPos++;
                            if (debug) System.out.println("ncnf from data = " + ncnf + " (code 13)");
                        }

                        // store ifmt[] index
                        lv[lev].left = imt;
                        // how many time will repeat format code inside parenthesis
                        lv[lev].nrepeat = ncnf;
                        // cleanup place for the right parenthesis (do not get it yet)
                        lv[lev].irepeat = 0;
                        // increase parenthesis level
                        lev++;

                        if (debug) System.out.println( "4\n");
                    }
                    // format F or I or ...
                    else {
                        // there are no parenthesis, just simple format, go to processing
                        if (lev == 0) {
                            if (debug) System.out.println("51\n");
                        }
                        // have parenthesis, NOT the pre-last format element (last assumed ')' ?)
                        else if (imt != (nfmt-1)) {
                            if (debug) System.out.println(String.format("52: %d %d\n",imt,nfmt-1));
                        }
                        // have parenthesis, NOT the first format after the left parenthesis
                        else if (imt != lv[lev-1].left+1) {
                            if (debug) System.out.println(String.format("53: %d %d\n",imt,nfmt-1));
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
                            if (debug) System.out.println( "54\n");
                        }
                        break;
                    }
                }
            } // while(true)


            // if 'ncnf' is zero, get it from data
            if (ncnf == 0) {

                if (mcnf == 1) {
                    // read "N" value from buffer
                    // (srcBuf automagically swaps in the getInt)
                    ncnf = srcBuf.getInt(srcPos);

                    // put swapped val back into buffer
                    // (destBuf automagically swaps in the putInt)
                    destBuf.putInt(destPos, ncnf);

                    srcPos += 4;
                    destPos += 4;

                    if (debug) System.out.println("ncnf32 = " + ncnf);
                }
                else if (mcnf == 2) {
                    ncnf = srcBuf.getShort(srcPos) & 0xffff;
                    destBuf.putShort(destPos, (short)ncnf);
                    srcPos += 2;
                    destPos += 2;
                    if (debug) System.out.println("ncnf16 = " + ncnf);
                }
                else if (mcnf == 3) {
                    ncnf = srcBuf.get(srcPos) & 0xff;
                    destBuf.put(destPos, (byte)ncnf);
                    srcPos++;
                    destPos++;
                    if (debug) System.out.println("ncnf08 = " + ncnf);
                }
            }


            // At this point we are ready to process next piece of data;.
            // We have following entry info:
            //     kcnf - format type
            //     ncnf - how many times format repeated
            //     b8   - starting data pointer (it comes from previously processed piece)

            // Convert data in according to type kcnf

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                int b64EndIndex = srcPos + 8*ncnf;
                // make sure we don't go past end of data
                if (b64EndIndex > srcEndIndex) b64EndIndex = srcEndIndex;
                // swap all 64 bit items
                while (srcPos < b64EndIndex) {
                    // buffers are of opposite endianness so putLong will automagically swap
                    destBuf.putLong(destPos, srcBuf.getLong(srcPos));
                    srcPos  += 8;
                    destPos += 8;
                }

                if (debug) System.out.println("64bit: %d elements " + ncnf);
            }
            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                int b32EndIndex = srcPos + 4*ncnf;
                // make sure we don't go past end of data
                if (b32EndIndex > srcEndIndex) b32EndIndex = srcEndIndex;
                // swap all 32 bit items
                while (srcPos < b32EndIndex) {
                    // buffers are of opposite endianness so putInt will automagically swap
                    destBuf.putInt(destPos, srcBuf.getInt(srcPos));
                    srcPos  += 4;
                    destPos += 4;
                }

                if (debug) System.out.println("32bit: %d elements " + ncnf);
            }
            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                int b16EndIndex = srcPos + 2*ncnf;
                // make sure we don't go past end of data
                if (b16EndIndex > srcEndIndex) b16EndIndex = srcEndIndex;
                // swap all 16 bit items
                while (srcPos < b16EndIndex) {
                    // buffers are of opposite endianness so putShort will automagically swap
                    destBuf.putShort(destPos, srcBuf.getShort(srcPos));
                    srcPos  += 2;
                    destPos += 2;
                }

                if (debug) System.out.println("16bit: %d elements " + ncnf);
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                // copy characters if dest != src
                if (!inPlace) {
                    // Use absolute gets & puts
                    for (int j=0; j < ncnf; j++) {
                        destBuf.put(destPos+j, srcBuf.get(srcPos+j));
                    }
                }
                srcPos  += ncnf;
                destPos += ncnf;
                if (debug) System.out.println("8bit: %d elements " + ncnf);
            }

        } //while

        if (debug) System.out.println("\n=== eviofmtswap end ===");

        if (inPlace) {
            // If swapped in place, we need to update the buffer's byte order
            srcBuf.order(destOrder);
        }
    }


    /**
     * This method takes a CompositeData object and a transformed format string
     * and uses that to write data into a buffer/array in raw form.
     *
     * @param rawBuf   data buffer in which to put the raw bytes
     * @param data     data to convert to raw bytes
     * @param ifmt     format list as produced by {@link #compositeFormatToInt(String)}
     *
     * @throws EvioException if ifmt size &lt;= 0; if srcBuf or destBuf is too
     *                       small; not enough dataItems for the given format
     */
    public static void dataToRawBytes(ByteBuffer rawBuf, CompositeData.Data data,
                                      List<Integer> ifmt)
                        throws EvioException {

        boolean debug = false;
        int imt, ncnf, kcnf, mcnf, lev, iterm;

        // check args
        if (ifmt == null || data == null || rawBuf == null) throw new EvioException("arg is null");

        // size of format list
        int nfmt = ifmt.size();
        if (nfmt <= 0) throw new EvioException("empty format list");

        LV[] lv = new LV[10];
        for (int i=0; i < lv.length; i++) {
            lv[i] = new LV();
        }

        imt   = 0;  // ifmt[] index
        lev   = 0;  // parenthesis level
        ncnf  = 0;  // how many times must repeat a format
        iterm = 0;

        int itemIndex = 0;
        int itemCount = data.dataItems.size();

        // write each item
        for (itemIndex = 0; itemIndex < itemCount;) {

            // get next format code
            while (true) {
                imt++;
                // end of format statement reached, back to iterm - last parenthesis or format begining
                if (imt > nfmt) {
                    imt = 0;
                }
                // meet right parenthesis, so we're finished processing format(s) in parenthesis
                else if (ifmt.get(imt-1) == 0) {
                    // increment counter
                    lv[lev-1].irepeat++;

                    // if format in parenthesis was processed the required number of times
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        // store left parenthesis index minus 1
                        iterm = lv[lev-1].left - 1;
                        // done with this level - decrease parenthesis level
                        lev--;
                    }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        // points ifmt[] index to the left parenthesis from the same level 'lev'
                        imt = lv[lev-1].left;
                    }
                }
                else {
                    ncnf = (ifmt.get(imt-1) >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  ifmt.get(imt-1) & 0xFF;        /* format code */
                    mcnf = (ifmt.get(imt-1) >> 14) & 0x3;  /* repeat code */

                    // left parenthesis - set new lv[]
                    if (kcnf == 0) {

                        // check repeats for left parenthesis

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as int
                        if (mcnf == 1) {
                            // set it to regular left parenthesis code
                            mcnf = 0;

                            // get "N" value from List
                            if (data.dataTypes.get(itemIndex) != DataType.NVALUE) {
                                throw new EvioException("Data type mismatch, N val is not NVALUE, got " +
                                        data.dataTypes.get(itemIndex));
                            }
                            ncnf = (Integer)data.dataItems.get(itemIndex++);
                            if (debug) System.out.println("ncnf from list = " + ncnf + " (code 15)");

                            // put into buffer (relative put)
                            rawBuf.putInt(ncnf);
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as short
                        if (mcnf == 2) {
                            mcnf = 0;

                            // get "n" value from List
                            if (data.dataTypes.get(itemIndex) != DataType.nVALUE) {
                                throw new EvioException("Data type mismatch, n val is not nVALUE, got " +
                                        data.dataTypes.get(itemIndex));
                            }
                            // Get rid of sign extension to allow n to be unsigned
                            ncnf = ((Short)data.dataItems.get(itemIndex++)).intValue() & 0xffff;
                            if (debug) System.out.println("ncnf from list = " + ncnf + " (code 14)");

                            // put into buffer (relative put)
                            rawBuf.putShort((short)ncnf);
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as byte
                        if (mcnf == 3) {
                            mcnf = 0;

                            // get "m" value from List
                            if (data.dataTypes.get(itemIndex) != DataType.mVALUE) {
                                throw new EvioException("Data type mismatch, m val is not mVALUE, got " +
                                        data.dataTypes.get(itemIndex));
                            }
                            // Get rid of sign extension to allow m to be unsigned
                            ncnf = ((Byte)data.dataItems.get(itemIndex++)).intValue() & 0xff;
                            if (debug) System.out.println("ncnf from list = " + ncnf + " (code 13)");

                            // put into buffer (relative put)
                            rawBuf.put((byte)ncnf);
                        }

                        // store ifmt[] index
                        lv[lev].left = imt;
                        // how many time will repeat format code inside parenthesis
                        lv[lev].nrepeat = ncnf;
                        // cleanup place for the right parenthesis (do not get it yet)
                        lv[lev].irepeat = 0;
                        // increase parenthesis level
                        lev++;
                    }
                    // format F or I or ...
                    else {
                        // there are no parenthesis, just simple format, go to processing
                        if (lev == 0) {
                        }
                        // have parenthesis, NOT the pre-last format element (last assumed ')' ?)
                        else if (imt != (nfmt-1)) {
                        }
                        // have parenthesis, NOT the first format after the left parenthesis
                        else if (imt != lv[lev-1].left+1) {
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
                        }
                        break;
                    }
                }
            } // while(true)


            // if 'ncnf' is zero, get N,n,m from list
            if (ncnf == 0) {

                if (mcnf == 1) {
                    // get "N" value from List
                    if (data.dataTypes.get(itemIndex) != DataType.NVALUE) {
                        throw new EvioException("Data type mismatch, N val is not NVALUE, got " +
                                data.dataTypes.get(itemIndex));
                    }
                    ncnf = (Integer) data.dataItems.get(itemIndex++);

                    // put into buffer (relative put)
                    rawBuf.putInt(ncnf);

                    if (debug) System.out.println("ncnf32 = " + ncnf);
                }
                else if (mcnf == 2) {
                    // get "n" value from List
                    if (data.dataTypes.get(itemIndex) != DataType.nVALUE) {
                        throw new EvioException("Data type mismatch, n val is not nVALUE, got " +
                                                data.dataTypes.get(itemIndex));
                    }
                    ncnf = ((Short)data.dataItems.get(itemIndex++)).intValue() & 0xffff;
                    rawBuf.putShort((short)ncnf);
                    if (debug) System.out.println("ncnf16 = " + ncnf);
                 }
                 else if (mcnf == 3) {
                    // get "m" value from List
                    if (data.dataTypes.get(itemIndex) != DataType.mVALUE) {
                        throw new EvioException("Data type mismatch, m val is not mVALUE, got " +
                                data.dataTypes.get(itemIndex));
                    }
                    ncnf = ((Byte)data.dataItems.get(itemIndex++)).intValue() & 0xff;
                    rawBuf.put((byte)ncnf);
                    if (debug) System.out.println("ncnf08 = " + ncnf);
                 }
            }


            // At this point we are ready to process next piece of data;.
            // We have following entry info:
            //     kcnf - format type
            //     ncnf - how many times format repeated

            // Convert data type kcnf
if (debug) System.out.println("Convert data of type = " + kcnf + ", itemIndex = " + itemIndex);
            switch (kcnf) {
                // 64 Bits
                case 8:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.DOUBLE64) {
                            throw new EvioException("Data type mismatch, expecting DOUBLE64, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putDouble((Double)data.dataItems.get(itemIndex++));
                    }
                    break;

                case 9:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.LONG64) {
                            throw new EvioException("Data type mismatch, expecting LONG64, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putLong((Long)data.dataItems.get(itemIndex++));
                    }
                    break;

                case 10:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.ULONG64) {
                            throw new EvioException("Data type mismatch, expecting ULONG64, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putLong((Long)data.dataItems.get(itemIndex++));
                    }
                    break;

                // 32 Bits
                case 11:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.INT32) {
                            throw new EvioException("Data type mismatch, expecting INT32, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putInt((Integer)data.dataItems.get(itemIndex++));
                    }
                    break;

                case 1:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.UINT32) {
                            throw new EvioException("Data type mismatch, expecting UINT32, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putInt((Integer)data.dataItems.get(itemIndex++));
                    }
                    break;

                case 2:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.FLOAT32) {
                            throw new EvioException("Data type mismatch, expecting FLOAT32, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putFloat((Float)data.dataItems.get(itemIndex++));
                    }
                    break;

                case 12:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.HOLLERIT) {
                            throw new EvioException("Data type mismatch, expecting HOLLERIT, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putInt((Integer)data.dataItems.get(itemIndex++));
                    }
                    break;

                // 16 bits
                case 4:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.SHORT16) {
                            throw new EvioException("Data type mismatch, expecting SHORT16, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putShort((Short)data.dataItems.get(itemIndex++));
                    }
                    break;

                case 5:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.USHORT16) {
                            throw new EvioException("Data type mismatch, expecting USHORT16, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.putShort((Short)data.dataItems.get(itemIndex++));
                    }
                    break;

                // 8 bits
                case 6:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.CHAR8) {
                            throw new EvioException("Data type mismatch, expecting CHAR8, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.put((Byte)data.dataItems.get(itemIndex++));
                    }
                    break;

                case 7:
                    for (int j=0; j < ncnf; j++) {
                        if (data.dataTypes.get(itemIndex) != DataType.UCHAR8) {
                            throw new EvioException("Data type mismatch, expecting UCHAR8, got " +
                                                     data.dataTypes.get(itemIndex));
                        }
                        rawBuf.put((Byte)data.dataItems.get(itemIndex++));
                    }
                    break;

                // String array
                case 3:
                    if (data.dataTypes.get(itemIndex) != DataType.CHARSTAR8) {
                        throw new EvioException("Data type mismatch, expecting string, got " +
                                                 data.dataTypes.get(itemIndex));
                    }

                    // Convert String array into evio byte representation
                    String[] strs = (String[]) data.dataItems.get(itemIndex++);
                    byte[] rb = BaseStructure.stringsToRawBytes(strs);
                    rawBuf.put(rb);

                    if (ncnf != rb.length) {
                        throw new EvioException("String format mismatch with string (array)");
                    }
                    break;

                default:
            }
        }
    }



    /**
     * This method extracts and stores all the data items and their types in various lists.
     */
    public void process() {

        boolean debug = false;
        int imt, ncnf, kcnf, mcnf, lev, iterm;

        // size of int list
        int nfmt = formatInts.size();

        items = new ArrayList<Object>(100);
        types = new ArrayList<DataType>(100);
        NList = new ArrayList<Integer>(100);
        nList = new ArrayList<Short>(100);
        mList = new ArrayList<Byte>(100);

        LV[] lv = new LV[10];
        for (int i=0; i < lv.length; i++) {
            lv[i] = new LV();
        }

        imt   = 0;  // formatInts[] index
        lev   = 0;  // parenthesis level
        ncnf  = 0;  // how many times must repeat a format
        iterm = 0;

        // start of data, index into data array
        int dataIndex = 0;
        // just past end of data -> size of data in bytes
        int endIndex = dataBytes;


        while (dataIndex < endIndex) {
            if (debug) System.out.println(String.format("+++ %d %d", dataIndex, endIndex));

            // get next format code
            while (true) {
                imt++;
                // end of format statement reached, back to iterm - last parenthesis or format begining
                if (imt > nfmt) {
                    imt = 0;
                }
                // meet right parenthesis, so we're finished processing format(s) in parenthesis
                else if (formatInts.get(imt-1) == 0) {
                    // increment counter
                    lv[lev-1].irepeat++;

                    // if format in parenthesis was processed the required number of times
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        // store left parenthesis index minus 1
                        iterm = lv[lev-1].left - 1;
                        // done with this level - decrease parenthesis level
                        lev--;
                    }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        // points formatInts[] index to the left parenthesis from the same level 'lev'
                        imt = lv[lev-1].left;
                    }
                }
                else {

                    ncnf = (formatInts.get(imt-1) >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  formatInts.get(imt-1) & 0xFF;        /* format code */
                    mcnf = (formatInts.get(imt-1) >> 14) & 0x3;  /* repeat code */

                    // left parenthesis - set new lv[]
                    if (kcnf == 0) {

                        // left parenthesis, SPECIAL case: #repeats must be taken from int data
                        if (mcnf == 1) {
                            // set it to regular left parenthesis code
                            mcnf = 0;

                            // read "N" value from buffer
                            ncnf = dataBuffer.getInt(dataIndex);

                            NList.add(ncnf);
                            items.add(ncnf);
                            types.add(DataType.NVALUE);
                            if (debug) System.out.println(String.format("+++ adding N %d", ncnf));

                            dataIndex += 4;
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from short data
                        if (mcnf == 2) {
                            mcnf = 0;
                            // read "n" value from buffer
                            ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                            nList.add((short)ncnf);
                            items.add((short)ncnf);
                            types.add(DataType.nVALUE);
                            if (debug) System.out.println(String.format("+++ adding n %hd", (short)ncnf));
                            dataIndex += 2;
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from byte data
                        if (mcnf == 3) {
                            mcnf = 0;
                            // read "m" value from buffer
                            ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                            mList.add((byte)ncnf);
                            items.add((byte)ncnf);
                            types.add(DataType.mVALUE);
                            if (debug) System.out.println(String.format("+++ adding m %c", (byte)ncnf));
                            dataIndex++;
                        }

//                        if (ncnf == 0) { //special case: if N=0, skip to the right paren
//                            iterm = imt-1;
//                            while (formatInts.get(imt-1) != 0) {
//                                imt++;
//                            }
//                            continue;
//                        }

                        // store formatInts[] index
                        lv[lev].left = imt;
                        // how many time will repeat format code inside parenthesis
                        lv[lev].nrepeat = ncnf;
                        // cleanup place for the right parenthesis (do not get it yet)
                        lv[lev].irepeat = 0;
                        // increase parenthesis level
                        lev++;
                    }
                    // format F or I or ...
                    else {
                        // there are no parenthesis, just simple format, go to processing
                        if (lev == 0) {
                        }
                        // have parenthesis, NOT the pre-last format element (last assumed ')' ?)
                        else if (imt != (nfmt-1)) {
                        }
                        // have parenthesis, NOT the first format after the left parenthesis
                        else if (imt != lv[lev-1].left+1) {
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
                        }
                        break;
                    }
                }
            } // while(true)


            // if 'ncnf' is zero, get N,n,m from list
            if (ncnf == 0) {
                if (mcnf == 1) {
                    // read "N" value from buffer
                    ncnf = dataBuffer.getInt(dataIndex);
                    NList.add(ncnf);
                    items.add(ncnf);
                    types.add(DataType.NVALUE);
                    if (debug) System.out.println(String.format("+++ adding N %d", ncnf));
                    dataIndex += 4;
                }
                else if (mcnf == 2) {
                    // read "n" value from buffer
                    ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                    nList.add((short)ncnf);
                    items.add((short)ncnf);
                    types.add(DataType.nVALUE);
                    if (debug) System.out.println(String.format("+++ adding n %hd", (short)ncnf));
                    dataIndex += 2;
                }
                else if (mcnf == 3) {
                    // read "m" value from buffer
                    ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                    mList.add((byte)ncnf);
                    items.add((byte)ncnf);
                    types.add(DataType.mVALUE);
                    if (debug) System.out.println(String.format("+++ adding m %c", (byte)ncnf));
                    dataIndex++;
                }
            }


            // At this point we are ready to process next piece of data;.
            // We have following entry info:
            //     kcnf - format type
            //     ncnf - how many times format repeated
            //     b8   - starting data pointer (it comes from previously processed piece)

            // Convert data in according to type kcnf

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                long lng;
                int b64EndIndex = dataIndex + 8*ncnf;
                // make sure we don't go past end of data
                if (b64EndIndex > endIndex) b64EndIndex = endIndex;

                while (dataIndex < b64EndIndex) {
                    lng = dataBuffer.getLong(dataIndex);

                    // store its data type
                    types.add(DataType.getDataType(kcnf));

                    // double
                    if (kcnf == 8) {
                        items.add(Double.longBitsToDouble(lng));
                    }
                    // 64 bit int/uint
                    else {
                        items.add(lng);
                    }

                    dataIndex += 8;
                }
            }
            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                int i, b32EndIndex = dataIndex + 4*ncnf;
                // make sure we don't go past end of data
                if (b32EndIndex > endIndex) b32EndIndex = endIndex;

                while (dataIndex < b32EndIndex) {
                    i = dataBuffer.getInt(dataIndex);

                    // Hollerit
                    if (kcnf == 12) {
                        types.add(DataType.HOLLERIT);
                        items.add(i);
                    }
                    // 32 bit float
                    else if (kcnf == 2) {
                        types.add(DataType.getDataType(kcnf));
                        items.add(Float.intBitsToFloat(i));
                    }
                    // 32 bit int/uint
                    else {
                        types.add(DataType.getDataType(kcnf));
                        items.add(i);
                    }

                    dataIndex += 4;
                }
            }
            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                short s;
                int b16EndIndex = dataIndex + 2*ncnf;
                // make sure we don't go past end of data
                if (b16EndIndex > endIndex) b16EndIndex = endIndex;

                // swap all 16 bit items
                while (dataIndex < b16EndIndex) {
                    s = dataBuffer.getShort(dataIndex);

                    types.add(DataType.getDataType(kcnf));
                    items.add(s);
                    dataIndex += 2;
                }
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                int b8EndIndex = dataIndex + ncnf;
                // make sure we don't go past end of data
                if (b8EndIndex > endIndex) {
                    //b8EndIndex = endIndex;
                    //ncnf = endIndex - b8EndIndex; // BUG found 9/5/2018
                    ncnf = endIndex - dataIndex;
                }

                dataBuffer.position(dataIndex);
                byte[] bytes = new byte[ncnf];
                // relative read
                dataBuffer.get(bytes);

                // string array
                if (kcnf == 3) {
                    String[] strs = BaseStructure.unpackRawBytesToStrings(bytes, 0, ncnf);
                    items.add(strs);
                }
                // char & unsigned char ints
                else {
                    for (int i=0; i < ncnf; i++) items.add(bytes[i]);
                }

                // reset position
                dataBuffer.position(0);
                if (debug) System.out.println("pushing type = " + DataType.getDataType(kcnf).toString() + " onto types");

                types.add(DataType.getDataType(kcnf));
                dataIndex += ncnf;
            }

        } //while

    }


    /**
     * Obtain a string representation of the composite data.
     * @return a string representation of the composite data.
     */
    @Override
    public String toString() {
        return toString("");
    }


    /**
     * Obtain a string representation of the composite data.
     * This string has an indent inserted in front of each group
     * of 5 items. After each group of 5, a newline is inserted.
     * Useful for writing data in xml format.
     *
     * @param  indent a string to insert in front of each group of 5 items
     * @return a string representation of the composite data.
     */
    public String toString(String indent) {
        getIndex = 0;
        StringBuilder sb = new StringBuilder(1024);
        int numItems = items.size();

        for (int i=0; i < numItems; i++) {
            if (i%5 == 0) sb.append(indent);

            switch (types.get(i)) {
                case NVALUE:
                    sb.append("N="); sb.append(getNValue());
                    break;
                case nVALUE:
                    sb.append("n="); sb.append(getnValue());
                    break;
                case mVALUE:
                    sb.append("m="); sb.append(getmValue());
                    break;
                case INT32:
                    sb.append("I="); sb.append(getInt());
                    break;
                case UINT32:
                    sb.append("i="); sb.append(getInt());
                    break;
                case HOLLERIT:
                    sb.append("A="); sb.append(getInt());
                    break;
                case CHAR8:
                    sb.append("C="); sb.append(getByte());
                    break;
                case UCHAR8:
                    sb.append("c="); sb.append(getByte());
                    break;
                case SHORT16:
                    sb.append("S="); sb.append(getShort());
                    break;
                case USHORT16:
                    sb.append("s="); sb.append(getShort());
                    break;
                case LONG64:
                    sb.append("L="); sb.append(getLong());
                    break;
                case ULONG64:
                    sb.append("l="); sb.append(getLong());
                    break;
                case DOUBLE64:
                    sb.append("D="); sb.append(getDouble());
                    break;
                case FLOAT32:
                    sb.append("F="); sb.append(getFloat());
                    break;
                case CHARSTAR8:
                    sb.append("a=");
                    String[] strs = getStrings();
                    for (int j=0; j < strs.length; j++) {
                        sb.append(strs[j]);
                        if (j < strs.length - 1) sb.append(",");
                    }
                    break;
                default:
            }

            if (i < numItems - 1) sb.append(", ");
            if (((i+1)%5 == 0) && (i < numItems - 1)) sb.append("\n");
        }
        return sb.toString();
    }


    /**
     * This method returns an xml string representation of this CompositeData object.
     * @param hex if <code>true</code> then print integers in hexadecimal.
     * @return an xml string representation of this CompositeData object.
     */
    public String toXML(boolean hex) {
        StringWriter sWriter = null;
        XMLStreamWriter xmlWriter = null;
        try {
            sWriter = new StringWriter();
            xmlWriter = XMLOutputFactory.newInstance().createXMLStreamWriter(sWriter);
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }

        String xmlIndent = "";

        try {
            toXML(xmlWriter, xmlIndent, hex);
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }

        return sWriter.toString();
    }


    /**
     * This method writes an xml string representation of this CompositeData object
     * into the xmlWriter given.
     *
     * @param xmlWriter the writer used to write the events to XML. It is tied to an open file.
     * @param bs evio container object that called this method. Allows us to use
     *           some convenient methods.
     * @param hex if <code>true</code> then print integers in hexadecimal
     * @throws XMLStreamException if writing bad format XML
     */
    void toXML(XMLStreamWriter xmlWriter, BaseStructure bs, boolean hex)
                        throws XMLStreamException {

        // size of int list
        int nfmt = formatInts.size();

        LV[] lv = new LV[10];
        for (int i=0; i < lv.length; i++) {
            lv[i] = new LV();
        }

        int imt   = 0;  // formatInts[] index
        int lev   = 0;  // parenthesis level
        int ncnf  = 0;  // how many times must repeat a format
        int iterm = 0;
        int kcnf, mcnf;
        int rowNumber = 2;

        // start of data, index into data array
        int dataIndex = 0;
        // just past end of data -> size of data in bytes
        int endIndex = dataBytes;
        // track repeat xml element so it can be ended properly
        int repeat = 0;

        //---------------------------------
        // First we have the format string
        //---------------------------------
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeStartElement("comp");

        bs.increaseXmlIndent();

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeStartElement("format");
        xmlWriter.writeAttribute("data_type","0x3");
        xmlWriter.writeAttribute("tag",""+tsHeader.tag);
        xmlWriter.writeAttribute("length",""+tsHeader.length);
        xmlWriter.writeAttribute("ndata","1");

        bs.increaseXmlIndent();

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeCharacters(format);

        bs.decreaseXmlIndent();

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeEndElement();

        //---------------------------------
        // Next the data
        //---------------------------------
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeStartElement("data");
        xmlWriter.writeAttribute("tag", "" + bHeader.tag);
        xmlWriter.writeAttribute("num", "" + bHeader.number);

        bs.increaseXmlIndent();

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeStartElement("row");
        xmlWriter.writeAttribute("num","1");

        bs.increaseXmlIndent();

        // Keep track if repeat value if from reading N or
        // is a specific  int embedded in the format statement.
        boolean repeatFromN;
        int Nnm;

        while (dataIndex < endIndex) {

            // get next format code
            while (true) {
                repeatFromN = false;
                Nnm = 0;
                imt++;
                // end of format statement reached, back to format beginning
                if (imt > nfmt) {
                    imt = 0;

                    bs.decreaseXmlIndent();
                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(bs.xmlIndent);
                    xmlWriter.writeEndElement();   // </row>

                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(bs.xmlIndent);
                    xmlWriter.writeStartElement("row");
                    xmlWriter.writeAttribute("num",""+rowNumber++);

                    bs.increaseXmlIndent();
                }
                // right parenthesis, so we're finished processing format(s) in parenthesis
                else if (formatInts.get(imt-1) == 0) {
                    lv[lev-1].irepeat++;
                    // if format in parenthesis was processed
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        iterm = lv[lev-1].left - 1;
                        lev--;

                        bs.decreaseXmlIndent();
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                        xmlWriter.writeEndElement();// </paren>

                        bs.decreaseXmlIndent();
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                        xmlWriter.writeEndElement();// </repeat>
                        repeat--;
                    }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        imt = lv[lev-1].left;

                        bs.decreaseXmlIndent();
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                        xmlWriter.writeEndElement();// </paren>

                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                        xmlWriter.writeStartElement("paren");
                        bs.increaseXmlIndent();
                    }
                }
                else {
                    ncnf = (formatInts.get(imt-1) >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  formatInts.get(imt-1) & 0xFF;        /* format code */
                    mcnf = (formatInts.get(imt-1) >> 14) & 0x3;  /* repeat code */

                    // left parenthesis - set new lv[]
                    if (kcnf == 0) {

                        // left parenthesis, SPECIAL case: #repeats must be taken from int data
                        if (mcnf == 1) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = dataBuffer.getInt(dataIndex);
                            dataIndex += 4;
                            repeatFromN = true;
                            Nnm = 1;
                        }
                        // left parenthesis, SPECIAL case: #repeats must be taken from short data
                        if (mcnf == 2) {
                            mcnf = 0;
                            // read "n" value from buffer
                            ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                            dataIndex += 2;
                            repeatFromN = true;
                            Nnm = 2;
                        }
                        // left parenthesis, SPECIAL case: #repeats must be taken from byte data
                        if (mcnf == 3) {
                            mcnf = 0;
                            // read "m" value from buffer
                            ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                            dataIndex++;
                            repeatFromN = true;
                            Nnm = 3;
                        }

                        // left parenthesis - set new lv[]

                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                        xmlWriter.writeStartElement("repeat");

                        if (repeatFromN) {
                            if (Nnm == 2)
                                xmlWriter.writeAttribute("n", ""+ncnf);
                            else if (Nnm == 3)
                                xmlWriter.writeAttribute("m", ""+ncnf);
                            else
                                xmlWriter.writeAttribute("N", ""+ncnf);
                        }
                        else {
                            xmlWriter.writeAttribute("count", ""+ncnf);
                        }
                        repeat++;

                        bs.increaseXmlIndent();
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                        xmlWriter.writeStartElement("paren");

                        lv[lev].left = imt;
                        lv[lev].nrepeat = ncnf;
                        lv[lev].irepeat = 0;
                        bs.increaseXmlIndent();

                        lev++;
                    }
                    // single format (e.g. F, I, etc.)
                    else {
                        if ((lev == 0) ||
                            (imt != (nfmt-1)) ||
                            (imt != lv[lev-1].left+1)) {
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
                        }
                        break;
                    }
                }
            }

            if (ncnf == 0) {
                if (mcnf == 1) {
                    // read "N" value from buffer
                    ncnf = dataBuffer.getInt(dataIndex);
                    dataIndex += 4;
                    repeatFromN = true;
                    Nnm = 1;
                }
                if (mcnf == 2) {
                    // read "n" value from buffer
                    ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                    dataIndex += 2;
                    repeatFromN = true;
                    Nnm = 2;
                }
                if (mcnf == 3) {
                    // read "m" value from buffer
                    ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                    dataIndex++;
                    repeatFromN = true;
                    Nnm = 3;
                }
            }

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                long lng;
                double d;
                int count=1, itemsOnLine=2;
                boolean oneLine = (ncnf <= itemsOnLine);

                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(bs.xmlIndent);
                if (kcnf ==  8) {
                    xmlWriter.writeStartElement("float64");
                }
                else if (kcnf ==  9) {
                    xmlWriter.writeStartElement("int64");
                }
                else if (kcnf == 10) {
                    xmlWriter.writeStartElement("uint64");
                }

                if (repeatFromN) {
                    if (Nnm == 2)
                        xmlWriter.writeAttribute("n", ""+ncnf);
                    else if (Nnm == 3)
                        xmlWriter.writeAttribute("m", ""+ncnf);
                    else
                        xmlWriter.writeAttribute("N", ""+ncnf);
                }
                else {
                    xmlWriter.writeAttribute("count", ""+ncnf);
                }

                if (!oneLine) {
                    bs.increaseXmlIndent();
                    xmlWriter.writeCharacters("\n");
                }
                else {
                    xmlWriter.writeCharacters(" ");
                }

                int b64EndIndex = dataIndex + 8*ncnf;
                if (b64EndIndex > endIndex) b64EndIndex = endIndex;

                while (dataIndex < b64EndIndex) {
                    lng = dataBuffer.getLong(dataIndex);

                    if (!oneLine && count % itemsOnLine == 1) {
                        xmlWriter.writeCharacters(bs.xmlIndent);
                    }

                    // double
                    if (kcnf == 8) {
                        d = Double.longBitsToDouble(lng);
                        xmlWriter.writeCharacters(String.format("%25.17g  ",d));
                    }
                    // hex
                    else if (hex) {
                        xmlWriter.writeCharacters(String.format("0x%016x  ", lng));
                    }
                    // 64 bit int
                    else if (kcnf == 9) {
                        xmlWriter.writeCharacters(String.format("%20d  ",lng));
                    }
                    // 64 bit uint
                    else {
                        BigInteger bg = new BigInteger(1, ByteDataTransformer.toBytes(lng, ByteOrder.BIG_ENDIAN));
                        String s = String.format("%20s  ", bg.toString());
                        // For java version 8+, no BigIntegers necessary:
                        //String s = String.format("%20s  ", Long.toUnsignedString(lng));
                        xmlWriter.writeCharacters(s);
                    }

                    if (!oneLine && count++ % itemsOnLine == 0) {
                        xmlWriter.writeCharacters("\n");
                    }
                    dataIndex += 8;
                }

                if (!oneLine) {
                    bs.decreaseXmlIndent();
                    if ((count - 1) % itemsOnLine != 0) {
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                    }
                }
                xmlWriter.writeEndElement();
            }
            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                float f;
                int count=1, itemsOnLine=4;
                boolean oneLine = (ncnf <= itemsOnLine);

                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(bs.xmlIndent);
                if (kcnf ==  2) {
                    xmlWriter.writeStartElement("float32");
                }
                else if (kcnf ==  1) {
                    xmlWriter.writeStartElement("uint32");
                }
                else if (kcnf == 11) {
                    xmlWriter.writeStartElement("int32");
                }
                else {
                    xmlWriter.writeStartElement("Hollerit");
                }

                if (repeatFromN) {
                    if (Nnm == 2)
                        xmlWriter.writeAttribute("n", ""+ncnf);
                    else if (Nnm == 3)
                        xmlWriter.writeAttribute("m", ""+ncnf);
                    else
                        xmlWriter.writeAttribute("N", ""+ncnf);
                }
                else {
                    xmlWriter.writeAttribute("count", ""+ncnf);
                }

                if (!oneLine) {
                    bs.increaseXmlIndent();
                    xmlWriter.writeCharacters("\n");
                }
                else {
                    xmlWriter.writeCharacters(" ");
                }

                int i, b32EndIndex = dataIndex + 4*ncnf;
                if (b32EndIndex > endIndex) b32EndIndex = endIndex;

                while (dataIndex < b32EndIndex) {
                    i = dataBuffer.getInt(dataIndex);

                    if (!oneLine && count % itemsOnLine == 1) {
                        xmlWriter.writeCharacters(bs.xmlIndent);
                    }

                    // float
                    if (kcnf == 2) {
                        f = Float.intBitsToFloat(i);
                        xmlWriter.writeCharacters(String.format("%15.8g  ",f));
                    }
                    else if (hex) {
                        xmlWriter.writeCharacters(String.format("0x%08x  ",i));
                    }
                    // uint
                    else if (kcnf == 1) {
                        xmlWriter.writeCharacters(String.format("%11d  ", ((long) i) & 0xffffffffL));
                    }
                    // int, Hollerit
                    else {
                        xmlWriter.writeCharacters(String.format("%11d  ",i));
                    }

                    if (!oneLine && count++ % itemsOnLine == 0) {
                        xmlWriter.writeCharacters("\n");
                    }
                    dataIndex += 4;
                }

                if (!oneLine) {
                    bs.decreaseXmlIndent();
                    if ((count - 1) % itemsOnLine != 0) {
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                    }
                }
                xmlWriter.writeEndElement();
            }
            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                short s;
                int count=1, itemsOnLine=8;
                boolean oneLine = (ncnf <= itemsOnLine);


                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(bs.xmlIndent);
                if (kcnf ==  4) {
                    xmlWriter.writeStartElement("int16");
                }
                else if (kcnf ==  5) {
                    xmlWriter.writeStartElement("uint16");
                }

                if (repeatFromN) {
                    if (Nnm == 2)
                        xmlWriter.writeAttribute("n", ""+ncnf);
                    else if (Nnm == 3)
                        xmlWriter.writeAttribute("m", ""+ncnf);
                    else
                        xmlWriter.writeAttribute("N", ""+ncnf);
                }
                else {
                    xmlWriter.writeAttribute("count", ""+ncnf);
                }

                if (!oneLine) {
                    bs.increaseXmlIndent();
                    xmlWriter.writeCharacters("\n");
                }
                else {
                    xmlWriter.writeCharacters(" ");
                }

                int b16EndIndex = dataIndex + 2*ncnf;
                if (b16EndIndex > endIndex) b16EndIndex = endIndex;

                // swap all 16 bit items
                while (dataIndex < b16EndIndex) {
                    s = dataBuffer.getShort(dataIndex);

                    if (!oneLine && count % itemsOnLine == 1) {
                        xmlWriter.writeCharacters(bs.xmlIndent);
                    }
                    else if (hex) {
                        xmlWriter.writeCharacters(String.format("0x%04x  ", s));
                    }
                    else if (kcnf ==  4) {
                        xmlWriter.writeCharacters(String.format("%6d  ", s));
                    }
                    else {
                        xmlWriter.writeCharacters(String.format("%6d  ", ((int)s & 0xffff)));
                    }

                    if (!oneLine && count++ % itemsOnLine == 0) {
                        xmlWriter.writeCharacters("\n");
                    }
                    dataIndex += 2;
                }

                if (!oneLine) {
                    bs.decreaseXmlIndent();
                    if ((count - 1) % itemsOnLine != 0) {
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(bs.xmlIndent);
                    }
                }
                xmlWriter.writeEndElement();
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                dataBuffer.position(dataIndex);
                byte[] bytes = new byte[ncnf];
                // relative read
                dataBuffer.get(bytes);

                // string array
                if (kcnf == 3) {
                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(bs.xmlIndent);
                    xmlWriter.writeStartElement("string");

                    if (repeatFromN) {
                        if (Nnm == 2)
                            xmlWriter.writeAttribute("n", ""+ncnf);
                        else if (Nnm == 3)
                            xmlWriter.writeAttribute("m", ""+ncnf);
                        else
                            xmlWriter.writeAttribute("N", ""+ncnf);
                    }
                    else {
                        xmlWriter.writeAttribute("count", ""+ncnf);
                    }

                    bs.increaseXmlIndent();
                    xmlWriter.writeCharacters("\n");

                    String[] strs = BaseStructure.unpackRawBytesToStrings(bytes, 0, ncnf);
                    for (String s: strs) {
                        xmlWriter.writeCharacters(bs.xmlIndent);
                        xmlWriter.writeCData(s);
                        xmlWriter.writeCharacters("\n");
                    }

                    bs.decreaseXmlIndent();
                    xmlWriter.writeCharacters(bs.xmlIndent);
                    xmlWriter.writeEndElement();
                }
                // char & unsigned char ints
                else {
                    int count=1, itemsOnLine=8;
                    boolean oneLine = (ncnf <= itemsOnLine);

                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(bs.xmlIndent);
                    if (kcnf == 6) {
                        xmlWriter.writeStartElement("int8");
                    }
                    else if (kcnf == 7) {
                        xmlWriter.writeStartElement("uint8");
                    }

                    if (repeatFromN) {
                        if (Nnm == 2)
                            xmlWriter.writeAttribute("n", ""+ncnf);
                        else if (Nnm == 3)
                            xmlWriter.writeAttribute("m", ""+ncnf);
                        else
                            xmlWriter.writeAttribute("N", ""+ncnf);
                    }
                    else {
                        xmlWriter.writeAttribute("count", ""+ncnf);
                    }

                    if (!oneLine) {
                        bs.increaseXmlIndent();
                        xmlWriter.writeCharacters("\n");
                    }
                    else {
                        xmlWriter.writeCharacters(" ");
                    }

                    for (int i=0; i < ncnf; i++) {
                        if (!oneLine && count % itemsOnLine == 1) {
                            xmlWriter.writeCharacters(bs.xmlIndent);
                        }

                        if (hex) {
                            xmlWriter.writeCharacters(String.format("0x%02x  ", bytes[i]));
                        }
                        else if (kcnf == 6) {
                            xmlWriter.writeCharacters(String.format("%4d  ", bytes[i]));
                        }
                        else {
                            xmlWriter.writeCharacters(String.format("%4d  ", ((short) bytes[i]) & 0xff));
                        }

                        if (!oneLine && count++ % itemsOnLine == 0) {
                            xmlWriter.writeCharacters("\n");
                        }
                    }

                    if (!oneLine) {
                        bs.decreaseXmlIndent();
                        if ((count - 1) % itemsOnLine != 0) {
                            xmlWriter.writeCharacters("\n");
                            xmlWriter.writeCharacters(bs.xmlIndent);
                        }
                    }
                    xmlWriter.writeEndElement();
                }

                // reset position
                dataBuffer.position(0);

                dataIndex += ncnf;
            }
        } //while

        // There may be 1 "<repeat" left to close
        if (repeat > 0) {
            bs.decreaseXmlIndent();
            xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(bs.xmlIndent);
            xmlWriter.writeEndElement();// </paren>

            bs.decreaseXmlIndent();
            xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(bs.xmlIndent);
            xmlWriter.writeEndElement();// </repeat>
        }

        // row
        bs.decreaseXmlIndent();
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeEndElement();

        // data
        bs.decreaseXmlIndent();
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeEndElement();

        // comp
        bs.decreaseXmlIndent();
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(bs.xmlIndent);
        xmlWriter.writeEndElement();
    }


    /**
     * This method writes an xml string representation of this CompositeData object
     * into the xmlWriter given. This is called by methods operating on an EvioNode
     * object.
     *
     * @param xmlWriter the writer used to write the events to XML.
     * @param xmlIndent indentation for writing XML.
     * @param hex if <code>true</code> then print integers in hexadecimal
     * @throws XMLStreamException if writing bad format XML
     */
    void toXML(XMLStreamWriter xmlWriter, String xmlIndent, boolean hex)
                        throws XMLStreamException {

        // size of int list
        int nfmt = formatInts.size();
        LV[] lv = new LV[10];
        for (int i=0; i < lv.length; i++) {
            lv[i] = new LV();
        }

        int imt   = 0;  // formatInts[] index
        int lev   = 0;  // parenthesis level
        int ncnf  = 0;  // how many times must repeat a format
        int iterm = 0;
        int kcnf, mcnf;
        int rowNumber = 2;

        // start of data, index into data array
        int dataIndex = 0;
        // just past end of data -> size of data in bytes
        int endIndex = dataBytes;
        // track repeat xml element so it can be ended properly
        int repeat = 0;

        //---------------------------------
        // First we have the format string
        //---------------------------------
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeStartElement("comp");

        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeStartElement("format");
        xmlWriter.writeAttribute("data_type","0x3");
        xmlWriter.writeAttribute("tag",""+tsHeader.tag);
        xmlWriter.writeAttribute("length",""+tsHeader.length);
        xmlWriter.writeAttribute("ndata","1");

        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeCharacters(format);

        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeEndElement();

        //---------------------------------
        // Next the data
        //---------------------------------
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeStartElement("data");
        xmlWriter.writeAttribute("tag", "" + bHeader.tag);
        xmlWriter.writeAttribute("num",""+bHeader.number);

        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);

        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeStartElement("row");
        xmlWriter.writeAttribute("num","1");

        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);

        // Keep track if repeat value if from reading N or
        // is a specific  int embedded in the format statement.
        boolean repeatFromN;
        int Nnm;

        while (dataIndex < endIndex) {

            // get next format code
            while (true) {
                repeatFromN = false;
                Nnm = 0;
                imt++;
                // end of format statement reached, back to format beginning
                if (imt > nfmt) {
                    imt = 0;

                    xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(xmlIndent);
                    xmlWriter.writeEndElement();   // </row>

                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(xmlIndent);
                    xmlWriter.writeStartElement("row");
                    xmlWriter.writeAttribute("num",""+rowNumber++);

                    xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                }
                // right parenthesis, so we're finished processing format(s) in parenthesis
                else if (formatInts.get(imt-1) == 0) {
                    lv[lev-1].irepeat++;
                    // if format in parenthesis was processed
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        iterm = lv[lev-1].left - 1;
                        lev--;

                        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                        xmlWriter.writeEndElement();// </paren>

                        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                        xmlWriter.writeEndElement();// </repeat>
                        repeat--;
                    }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        imt = lv[lev-1].left;

                        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                        xmlWriter.writeEndElement();// </paren>

                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                        xmlWriter.writeStartElement("paren");
                        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                    }
                }
                else {
                    ncnf = (formatInts.get(imt-1) >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  formatInts.get(imt-1) & 0xFF;        /* format code */
                    mcnf = (formatInts.get(imt-1) >> 14) & 0x3;  /* repeat code */

                    // left parenthesis - set new lv[]
                    if (kcnf == 0) {

//System.out.println("REPEAT = " + ncnf + ", formatInt = " + formatInts.get(imt-1) +", kcnf = " + kcnf);
                        // left parenthesis, SPECIAL case: #repeats must be taken from int data
                        if (mcnf == 1) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = dataBuffer.getInt(dataIndex);
//System.out.println("ncnf = " + ncnf + " databuf order = " + dataBuffer.order());
                            dataIndex += 4;
                            repeatFromN = true;
                            Nnm = 1;
                        }
                        // left parenthesis, SPECIAL case: #repeats must be taken from short data
                        if (mcnf == 2) {
                            mcnf = 0;
                            // read "n" value from buffer
                            ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                            dataIndex += 2;
                            repeatFromN = true;
                            Nnm = 2;
                        }
                        // left parenthesis, SPECIAL case: #repeats must be taken from byte data
                        if (mcnf == 3) {
                            mcnf = 0;
                            // read "m" value from buffer
                            ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                            dataIndex++;
                            repeatFromN = true;
                            Nnm = 3;
                        }

                        // left parenthesis - set new lv[]

                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                        xmlWriter.writeStartElement("repeat");

                        if (repeatFromN) {
                            if (Nnm == 2)
                                xmlWriter.writeAttribute("n", ""+ncnf);
                            else if (Nnm == 3)
                                xmlWriter.writeAttribute("m", ""+ncnf);
                            else
                                xmlWriter.writeAttribute("N", ""+ncnf);
                        }
                        else {
                            xmlWriter.writeAttribute("count", ""+ncnf);
                        }
                        repeat++;

                        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                        xmlWriter.writeStartElement("paren");

                        lv[lev].left = imt;
                        lv[lev].nrepeat = ncnf;
                        lv[lev].irepeat = 0;
                        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);

                        lev++;
                    }
                    // single format (e.g. F, I, etc.)
                    else {
                        if ((lev == 0) ||
                            (imt != (nfmt-1)) ||
                            (imt != lv[lev-1].left+1)) {
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
                        }
                        break;
                    }
                }
            }

            // if 'ncnf' is zero, get "N" from data (always in 'int' format)
            if (ncnf == 0) {
                if (mcnf == 1) {
                    // read "N" value from buffer
                    ncnf = dataBuffer.getInt(dataIndex);
                    dataIndex += 4;
                    repeatFromN = true;
                    Nnm = 1;
                }
                if (mcnf == 2) {
                    // read "n" value from buffer
                    ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                    dataIndex += 2;
                    repeatFromN = true;
                    Nnm = 2;
                }
                if (mcnf == 3) {
                    // read "m" value from buffer
                    ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                    dataIndex++;
                    repeatFromN = true;
                    Nnm = 3;
                }
            }

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                long lng;
                double d;
                int count=1, itemsOnLine=2;
                boolean oneLine = (ncnf <= itemsOnLine);

                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(xmlIndent);
                if (kcnf ==  8) {
                    xmlWriter.writeStartElement("float64");
                }
                else if (kcnf ==  9) {
                    xmlWriter.writeStartElement("int64");
                }
                else if (kcnf == 10) {
                    xmlWriter.writeStartElement("uint64");
                }

                if (repeatFromN) {
                    if (Nnm == 2)
                        xmlWriter.writeAttribute("n", ""+ncnf);
                    else if (Nnm == 3)
                        xmlWriter.writeAttribute("m", ""+ncnf);
                    else
                        xmlWriter.writeAttribute("N", ""+ncnf);
                }
                else {
                    xmlWriter.writeAttribute("count", ""+ncnf);
                }

                if (!oneLine) {
                    xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                    xmlWriter.writeCharacters("\n");
                }
                else {
                    xmlWriter.writeCharacters(" ");
                }

                int b64EndIndex = dataIndex + 8*ncnf;
                if (b64EndIndex > endIndex) b64EndIndex = endIndex;

                while (dataIndex < b64EndIndex) {
                    lng = dataBuffer.getLong(dataIndex);

                    if (!oneLine && count % itemsOnLine == 1) {
                        xmlWriter.writeCharacters(xmlIndent);
                    }

                    // double
                    if (kcnf == 8) {
                        d = Double.longBitsToDouble(lng);
                        xmlWriter.writeCharacters(String.format("%25.17g  ", d));
                    }
                    // hex
                    else if (hex) {
                        xmlWriter.writeCharacters(String.format("0x%016x  ", lng));
                    }
                    // 64 bit int
                    else if (kcnf == 9) {
                        xmlWriter.writeCharacters(String.format("%20d  ",lng));
                    }
                    // 64 bit uint
                    else {
                        BigInteger bg = new BigInteger(1, ByteDataTransformer.toBytes(lng, ByteOrder.BIG_ENDIAN));
                        String s = String.format("%20s  ", bg.toString());
                        // For java version 8+, no BigIntegers necessary:
                        //String s = String.format("%20s  ", Long.toUnsignedString(lng));
                        xmlWriter.writeCharacters(s);
                    }

                    if (!oneLine && count++ % itemsOnLine == 0) {
                        xmlWriter.writeCharacters("\n");
                    }
                    dataIndex += 8;
                }

                if (!oneLine) {
                    xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                    if ((count - 1) % itemsOnLine != 0) {
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                    }
                }
                xmlWriter.writeEndElement();
            }
            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                float f;
                int count=1, itemsOnLine=4;
                boolean oneLine = (ncnf <= itemsOnLine);

                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(xmlIndent);
                if (kcnf ==  2) {
                    xmlWriter.writeStartElement("float32");
                }
                else if (kcnf ==  1) {
                    xmlWriter.writeStartElement("uint32");
                }
                else if (kcnf == 11) {
                    xmlWriter.writeStartElement("int32");
                }
                else {
                    xmlWriter.writeStartElement("Hollerit");
                }

                if (repeatFromN) {
                    if (Nnm == 2)
                        xmlWriter.writeAttribute("n", ""+ncnf);
                    else if (Nnm == 3)
                        xmlWriter.writeAttribute("m", ""+ncnf);
                    else
                        xmlWriter.writeAttribute("N", ""+ncnf);
                }
                else {
                    xmlWriter.writeAttribute("count", ""+ncnf);
                }

                if (!oneLine) {
                    xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                    xmlWriter.writeCharacters("\n");
                }
                else {
                    xmlWriter.writeCharacters(" ");
                }

                int i, b32EndIndex = dataIndex + 4*ncnf;
                if (b32EndIndex > endIndex) b32EndIndex = endIndex;

                while (dataIndex < b32EndIndex) {
                    i = dataBuffer.getInt(dataIndex);

                    if (!oneLine && count % itemsOnLine == 1) {
                        xmlWriter.writeCharacters(xmlIndent);
                    }

                    // float
                    if (kcnf == 2) {
                        f = Float.intBitsToFloat(i) ;
                        xmlWriter.writeCharacters(String.format("%15.8g  ",f));
                    }
                    else if (hex) {
                        xmlWriter.writeCharacters(String.format("0x%08x  ",i));
                    }
                    // uint
                    else if (kcnf == 1) {
                        xmlWriter.writeCharacters(String.format("%11d  ", ((long) i) & 0xffffffffL));
                    }
                    // int, Hollerit
                    else {
                        xmlWriter.writeCharacters(String.format("%11d  ",i));
                    }

                    if (!oneLine && count++ % itemsOnLine == 0) {
                        xmlWriter.writeCharacters("\n");
                    }
                    dataIndex += 4;
                }

                if (!oneLine) {
                    xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                    if ((count - 1) % itemsOnLine != 0) {
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                    }
                }
                xmlWriter.writeEndElement();
            }
            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                short s;
                int count=1, itemsOnLine=8;
                boolean oneLine = (ncnf <= itemsOnLine);


                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(xmlIndent);
                if (kcnf ==  4) {
                    xmlWriter.writeStartElement("int16");
                }
                else if (kcnf ==  5) {
                    xmlWriter.writeStartElement("uint16");
                }

                if (repeatFromN) {
                    if (Nnm == 2)
                        xmlWriter.writeAttribute("n", ""+ncnf);
                    else if (Nnm == 3)
                        xmlWriter.writeAttribute("m", ""+ncnf);
                    else
                        xmlWriter.writeAttribute("N", ""+ncnf);
                }
                else {
                    xmlWriter.writeAttribute("count", ""+ncnf);
                }

                if (!oneLine) {
                    xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                    xmlWriter.writeCharacters("\n");
                }
                else {
                    xmlWriter.writeCharacters(" ");
                }

                int b16EndIndex = dataIndex + 2*ncnf;
                if (b16EndIndex > endIndex) b16EndIndex = endIndex;

                // swap all 16 bit items
                while (dataIndex < b16EndIndex) {
                    s = dataBuffer.getShort(dataIndex);

                    if (!oneLine && count % itemsOnLine == 1) {
                        xmlWriter.writeCharacters(xmlIndent);
                    }
                    else if (hex) {
                        xmlWriter.writeCharacters(String.format("0x%04x  ", s));
                    }
                    else if (kcnf ==  4) {
                        xmlWriter.writeCharacters(String.format("%6d  ", s));
                    }
                    else {
                        xmlWriter.writeCharacters(String.format("%6d  ", ((int)s & 0xffff)));
                    }

                    if (!oneLine && count++ % itemsOnLine == 0) {
                        xmlWriter.writeCharacters("\n");
                    }
                    dataIndex += 2;
                }

                if (!oneLine) {
                    xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                    if ((count - 1) % itemsOnLine != 0) {
                        xmlWriter.writeCharacters("\n");
                        xmlWriter.writeCharacters(xmlIndent);
                    }
                }
                xmlWriter.writeEndElement();
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                dataBuffer.position(dataIndex);
                byte[] bytes = new byte[ncnf];
                // relative read
                dataBuffer.get(bytes);

                // string array
                if (kcnf == 3) {
                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(xmlIndent);
                    xmlWriter.writeStartElement("string");

                    if (repeatFromN) {
                        if (Nnm == 2)
                            xmlWriter.writeAttribute("n", ""+ncnf);
                        else if (Nnm == 3)
                            xmlWriter.writeAttribute("m", ""+ncnf);
                        else
                            xmlWriter.writeAttribute("N", ""+ncnf);
                    }
                    else {
                        xmlWriter.writeAttribute("count", ""+ncnf);
                    }

                    xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                    xmlWriter.writeCharacters("\n");

                    String[] strs = BaseStructure.unpackRawBytesToStrings(bytes, 0, ncnf);
                    for (String s: strs) {
                        xmlWriter.writeCharacters(xmlIndent);
                        xmlWriter.writeCData(s);
                        xmlWriter.writeCharacters("\n");
                    }

                    xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                    xmlWriter.writeCharacters(xmlIndent);
                    xmlWriter.writeEndElement();
                }
                // char & unsigned char ints
                else {
                    int count=1, itemsOnLine=8;
                    boolean oneLine = (ncnf <= itemsOnLine);

                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCharacters(xmlIndent);
                    if (kcnf == 6) {
                        xmlWriter.writeStartElement("int8");
                    }
                    else if (kcnf == 7) {
                        xmlWriter.writeStartElement("uint8");
                    }

                    if (repeatFromN) {
                        if (Nnm == 2)
                            xmlWriter.writeAttribute("n", ""+ncnf);
                        else if (Nnm == 3)
                            xmlWriter.writeAttribute("m", ""+ncnf);
                        else
                            xmlWriter.writeAttribute("N", ""+ncnf);
                    }
                    else {
                        xmlWriter.writeAttribute("count", ""+ncnf);
                    }

                    if (!oneLine) {
                        xmlIndent = Utilities.increaseXmlIndent(xmlIndent);
                        xmlWriter.writeCharacters("\n");
                    }
                    else {
                        xmlWriter.writeCharacters(" ");
                    }

                    for (int i=0; i < ncnf; i++) {
                        if (!oneLine && count % itemsOnLine == 1) {
                            xmlWriter.writeCharacters(xmlIndent);
                        }

                        if (hex) {
                            xmlWriter.writeCharacters(String.format("0x%02x  ", bytes[i]));
                        }
                        else if (kcnf == 6) {
                            xmlWriter.writeCharacters(String.format("%4d  ", bytes[i]));
                        }
                        else {
                            xmlWriter.writeCharacters(String.format("%4d  ", ((short) bytes[i]) & 0xff));
                        }

                        if (!oneLine && count++ % itemsOnLine == 0) {
                            xmlWriter.writeCharacters("\n");
                        }
                    }

                    if (!oneLine) {
                        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
                        if ((count - 1) % itemsOnLine != 0) {
                            xmlWriter.writeCharacters("\n");
                            xmlWriter.writeCharacters(xmlIndent);
                        }
                    }
                    xmlWriter.writeEndElement();
                }

                // reset position
                dataBuffer.position(0);

                dataIndex += ncnf;
            }
        } //while

        // There may be 1 "<repeat" left to close
        if (repeat > 0) {
            xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
            xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(xmlIndent);
            xmlWriter.writeEndElement();// </paren>

            xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
            xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(xmlIndent);
            xmlWriter.writeEndElement();// </repeat>
        }

        // row
        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeEndElement();

        // data
        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeEndElement();

        // composite
        xmlIndent = Utilities.decreaseXmlIndent(xmlIndent);
        xmlWriter.writeCharacters("\n");
        xmlWriter.writeCharacters(xmlIndent);
        xmlWriter.writeEndElement();
    }


    /**
     * This method returns a string representation of this CompositeData object
     * suitable for displaying in a
     * gui. Each data item is separated from those before and after by a line.
     * Non-parenthesis repeats are printed together.
     *
     * @param hex if <code>true</code> then print integers in hexadecimal
     * @return  a string representation of this CompositeData object.
     */
    public String toString(boolean hex) {

        StringBuilder sb = new StringBuilder(1000);

        // size of int list
        int nfmt = formatInts.size();

        LV[] lv = new LV[10];
        for (int i=0; i < lv.length; i++) {
            lv[i] = new LV();
        }

        int imt   = 0;  // formatInts[] index
        int lev   = 0;  // parenthesis level
        int ncnf  = 0;  // how many times must repeat a format
        int iterm = 0;
        int kcnf, mcnf;

        // start of data, index into data array
        int dataIndex = 0;
        // just past end of data -> size of data in bytes
        int endIndex = dataBytes;

        //---------------------------------
        // First we have the format string
        //---------------------------------
        sb.append(format);
        sb.append("\n");

        while (dataIndex < endIndex) {

            // get next format code
            while (true) {
                imt++;
                // end of format statement reached, back to format beginning
                if (imt > nfmt) {
                    imt = 0;
                    sb.append("\n");  // end of one & beginning of another row
                 }
                // right parenthesis, so we're finished processing format(s) in parenthesis
                else if (formatInts.get(imt-1) == 0) {
                    lv[lev-1].irepeat++;
                    // if format in parenthesis was processed
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        iterm = lv[lev-1].left - 1;
                        lev--;
                    }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        imt = lv[lev-1].left;
                        sb.append("\n");
                    }
                }
                else {
                    ncnf = (formatInts.get(imt-1) >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  formatInts.get(imt-1) & 0xFF;        /* format code */
                    mcnf = (formatInts.get(imt-1) >> 14) & 0x3;  /* repeat code */

                    // left parenthesis - set new lv[]
                    if (kcnf == 0) {

                        // left parenthesis, SPECIAL case: #repeats must be taken from int data
                        if (mcnf == 1) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = dataBuffer.getInt(dataIndex);
                            dataIndex += 4;
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from short data
                        if (mcnf == 2) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                            dataIndex += 2;
                        }
                        // left parenthesis, SPECIAL case: #repeats must be taken from byte data
                        if (mcnf == 3) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                            dataIndex++;
                        }

                        // left parenthesis - set new lv[]
                        lv[lev].left = imt;
                        lv[lev].nrepeat = ncnf;
                        lv[lev].irepeat = 0;

                        sb.append("\n");  // start of a repeat
                        lev++;
                    }
                    // single format (e.g. F, I, etc.)
                    else {
                        if ((lev == 0) ||
                            (imt != (nfmt-1)) ||
                            (imt != lv[lev-1].left+1)) {
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
                        }
                        break;
                    }
                }
            }

            // if 'ncnf' is zero, get N,n,m from list
            if (ncnf == 0) {
                if (mcnf == 1) {
                     // read "N" value from buffer
                     ncnf = dataBuffer.getInt(dataIndex);
                     dataIndex += 4;
                 }
                 else if (mcnf == 2) {
                     // read "n" value from buffer
                     ncnf = (dataBuffer.getShort(dataIndex)) & 0xffff;
                     dataIndex += 2;
                 }
                 else if (mcnf == 3) {
                     // read "m" value from buffer
                     ncnf = (dataBuffer.get(dataIndex)) & 0xff;
                     dataIndex++;
                 }
            }

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                long lng;
                double d;
                int count=1, itemsOnLine=2;

                sb.append("\n");

                int b64EndIndex = dataIndex + 8*ncnf;
                if (b64EndIndex > endIndex) b64EndIndex = endIndex;

                while (dataIndex < b64EndIndex) {
                    lng = dataBuffer.getLong(dataIndex);

                    // double
                    if (kcnf == 8) {
                        d = Double.longBitsToDouble(lng);
                        sb.append(String.format("%-23.16g ",d));
                    }
                    // 64 bit int/uint
                    else {
                        if (hex) sb.append(String.format("0x%016x  ",lng));
                        else     sb.append(String.format("%-18d ",lng));
                    }

                    if (count++ % itemsOnLine == 0) {
                        sb.append("\n");
                    }
                    dataIndex += 8;
                }

                if ((count - 1) % itemsOnLine != 0) {
                    sb.append("\n");
                }
            }

            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                float f;
                int count=1, itemsOnLine=4;

                sb.append("\n");

                int i, b32EndIndex = dataIndex + 4*ncnf;
                if (b32EndIndex > endIndex) b32EndIndex = endIndex;

                while (dataIndex < b32EndIndex) {
                    i = dataBuffer.getInt(dataIndex);

                    // float
                    if (kcnf == 2) {
                        f = Float.intBitsToFloat(i);
                        sb.append(String.format("%-14.8g ",f));
                    }
                    // Hollerit, 32 bit int/uint
                    else {
                        if (hex) sb.append(String.format("0x%08x  ",i));
                        else     sb.append(String.format("%-10d ",i));
                    }

                    if (count++ % itemsOnLine == 0) {
                        sb.append("\n");
                    }
                    dataIndex += 4;
                }

                if ((count - 1) % itemsOnLine != 0) {
                    sb.append("\n");
                }
            }

            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                short s;
                int count=1, itemsOnLine=6;

                sb.append("\n");

                int b16EndIndex = dataIndex + 2*ncnf;
                if (b16EndIndex > endIndex) b16EndIndex = endIndex;

                // swap all 16 bit items
                while (dataIndex < b16EndIndex) {
                    s = dataBuffer.getShort(dataIndex);

                    if (hex) sb.append(String.format("0x%04x  ", s));
                    else     sb.append(String.format("%-6d ", s));

                    if (count++ % itemsOnLine == 0) {
                        sb.append("\n");
                    }
                    dataIndex += 2;
                }

                if ((count - 1) % itemsOnLine != 0) {
                    sb.append("\n");
                }
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                dataBuffer.position(dataIndex);
                byte[] bytes = new byte[ncnf];
                // relative read
                dataBuffer.get(bytes);

                // string array
                if (kcnf == 3) {
                    sb.append("\n");

                    String[] strs = BaseStructure.unpackRawBytesToStrings(bytes, 0);
                    for (String s: strs) {
                        sb.append(s);
                        sb.append("\n");
                    }
                }
                // char & unsigned char ints
                else {
                    int count=1, itemsOnLine=8;

                    sb.append("\n");

                    for (int i=0; i < ncnf; i++) {
                        if (hex) {
                            sb.append(String.format("0x%02x  ", bytes[i]));
                        }
                        else {
                            sb.append(String.format("%-4d ", bytes[i]));
                        }

                        if (count++ % itemsOnLine == 0) {
                            sb.append("\n");
                        }
                    }

                    if ((count - 1) % itemsOnLine != 0) {
                        sb.append("\n");
                    }
                }

                // reset position
                dataBuffer.position(0);

                dataIndex += ncnf;
            }

        } //while

        sb.append("\n");
        return sb.toString();
    }



}
