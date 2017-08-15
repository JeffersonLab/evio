package org.jlab.coda.jevio;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.BitSet;

/**
 * This holds an evio block header, also known as a physical record header.
 * Unfortunately, in versions 1, 2 and 3, evio files impose an anachronistic
 * block structure. The complication that arises is that logical records
 * (events) will sometimes cross physical record boundaries. This block structure
 * is changed in version 4 so that blocks only contain integral numbers of events.
 * The information stored in this block header has also changed.
 *
 *
 * <pre><code>
 * ################################
 * Evio block header, version 4:
 * ################################
 *
 * MSB(31)                          LSB(0)
 * &lt;---  32 bits ------------------------&gt;
 * _______________________________________
 * |            Block Length             |
 * |_____________________________________|
 * |            Block Number             |
 * |_____________________________________|
 * |          Header Length = 8          |
 * |_____________________________________|
 * |             Event Count             |
 * |_____________________________________|
 * |  CODA ID or Compressed Data Length  |
 * |_____________________________________|
 * |        Bit Info &amp; Version       |
 * |_____________________________________|
 * |              Reserved               |
 * |_____________________________________|
 * |             Magic Int               |
 * |_____________________________________|
 * |         Indexes to Events           |
 * |_____________________________________|
 *
 *
 *      Block Length       = number of uncompressed ints in block (including this one).
 *
 *      Block Number       = id number (starting at 1).
 *
 *      Header Length      = number of ints in this header (8).
 *
 *      Event Count        = number of events in this block (always an integral #).
 *                           NOTE: this value should not be used to parse the following
 *                           events since the first block may have a dictionary whose
 *                           presence is not included in this count.
 *
 *      CODA id /
 *      compressed length  = In CODA online, if bits 11-14 in bit info are RocRaw (0),
 *                           then (in the first block) this contains the CODA id of the source.
 *                           
 *                           When writing to or reading from a file, and bit 16 in bit info is
 *                           true (lz4 compression is on), this is the number of compressed data
 *                           BYTES (NOT WORDS!) in this block. Note, this does NOT include the
 *                           header.
 *
 *      Bit info &amp; Version = Lowest 8 bits are the version number (4).
 *                           Upper 24 bits contain bit info.
 *                           If a dictionary is included as the first event, bit #9 is set (=1)
 *                           If a last block, bit #10 is set.
 *                           For first block of CODA online, bits #11-14 are CODA id.
 *                           If dictionary included, bit #15 is set.
 *                           If data is compressed, bit #16 is set.
 *                           If event indexes are included, bit #17 is set.
 *
 *      Reserved           = unused.
 *
 *      Magic Int          = magic number (0xc0da0100) used to check endianness.
 *
 *      Optionally, after the Magic Int are indexes pointing to each UNcompressed event
 *      following the header.
 *
 *
 *
 * Bit info has the following bits defined (bit #s start with 1):
 *   Bit  9     = true if dictionary is included (relevant for first block only)
 *
 *   Bit  10    = true if this block is the last block in file or network transmission
 *
 *   Bits 11-14 = type of events following (ROC Raw = 0, Physics = 1, PartialPhysics = 2,
 *                DisentangledPhysics = 3, User = 4, Control = 5, Other = 15).
 *
 *   Bit 15     = true if next (non-dictionary) event in this block is a "first event" to
 *                be placed at the beginning of each written file and its splits.
 *
 *                Bits 11-15 are useful ONLY for the CODA online use of evio.
 *                That's because only a single CODA event type is placed into
 *                a single (ET, cMsg) buffer, and each user or control event has its own
 *                buffer as well. That buffer then is parsed by an EvioReader or
 *                EvioCompactReader object. Thus all events will be of a single CODA type.
 *
 *   Bit 16     = true if data following this block header is compressed in LZ4 format.
 *   Bit 17     = true if lengths of events are included in this header.
 *
 *
 * </code></pre>
 *
 *
 * @author heddle
 * @author timmer
 *
 */
public class BlockHeaderV4 implements Cloneable, IEvioWriter, IBlockHeader {

    /** The minimum and expected block header size in 32 bit ints. */
    public static final int HEADER_SIZE = 8;

    /** Dictionary presence is 9th bit in version/info word. */
    public static final int EV_DICTIONARY_MASK = 0x100;

    /** "Last block" is 10th bit in version/info word. */
    public static final int EV_LASTBLOCK_MASK  = 0x200;

    /** "Event type" is 11-14th bits` in version/info word. */
    public static final int EV_EVENTTYPE_MASK  = 0x3C00;

    /** "First event" is 15th bit in version/info word. */
    public static final int EV_FIRSTEVENT_MASK  = 0x4000;

    /** Data (all except block headers) are compressed in LZ4 format. */
    public static final int EV_LZ4COMPRESSION_MASK  = 0x8000;

    /** Event lengths are included in block headers. */
    public static final int EV_EVENTLENGTHS_MASK  = 0x10000;

    /** Position of word for size of block in 32-bit words. */
    public static final int EV_BLOCKSIZE = 0;
    /** Position of word for block number, starting at 1. */
    public static final int EV_BLOCKNUM = 1;
    /** Position of word for size of header in 32-bit words (=8). */
    public static final int EV_HEADERSIZE = 2;
    /** Position of word for number of events in block. */
    public static final int EV_COUNT = 3;
    /** Position of word for CODA id or compressed data length. */
    public static final int EV_COMPRESSEDLENGTH = 4;
    /** Position of word for version of file format. */
    public static final int EV_VERSION = 5;
    /** Position of word for reserved. */
    public static final int EV_RESERVED2 = 6;
    /** Position of word for magic number for endianness tracking. */
    public static final int EV_MAGIC = 7;
    /** Position of word for event indexes. */
    public static final int EV_INDEXES = 8;


    /** Index into bitInfo - has Dictionary? */
    public static final int EV_BIT_DICTIONARY = 0;
    /** Index into bitInfo - is last block? */
    public static final int EV_BIT_LASTBLOCK = 1;
    /** Index into bitInfo - has first event? */
    public static final int EV_BIT_FIRSTEVENT = 6;
    /** Index into bitInfo - is data compressed? */
    public static final int EV_BIT_COMPRESSED = 7;
    /** Index into bitInfo - this header has event lengths? */
    public static final int EV_BIT_EVENTLENGTHS = 8;


    /** Do we need to swap data from buffer? */
   	private boolean swap;

	/** The block (physical record) size in 32 bit ints. */
	private int size;

	/** The block number. In a file, this is usually sequential. */
	private int number;

	/** The block header length which is usually 8. */
	private int headerLength;

    /**
     * Since blocks only contain whole events in this version,
     * this stores the number of events contained in a block.
     */
    private int eventCount;

	/** The evio version which is always 4. */
	private int version;

    /**
     * Length of the compressed data in bytes if reading-from / writing-to file.
     * For CODA online, CODA id.
     */
    private int compressedLength;

    /** Value of second reserved word. */
    private int reserved2;

    /** Bit information. Bit one: is the first event a dictionary? */
    private BitSet bitInfo = new BitSet(24);

    /** Is the data following this block header compressed in LZ4 format? */
    private boolean isCompressed;

	/** This is the magic word, 0xc0da0100, used to check endianness. */
	private int magicNumber;

    /** This is the byte order of the data being read. */
    private ByteOrder byteOrder;

	/**
	 * This is not part of the block header proper. It is a position in a memory buffer
     * of the start of the block (physical record). It is kept for convenience.
	 */
	private long bufferStartingPosition = -1;


	
	/**
	 * Null constructor initializes all fields to zero/false, except block# = 1.
	 */
	public BlockHeaderV4() {
		number = 1;
	}

    /**
     * Creates a BlockHeader for evio version 4 format. Only the <code>block size</code>
     * and <code>block number</code> are provided. The other six words, which can be
     * modified by setters, are initialized to these values:<br>
     *<ul>
     *<li><code>headerLength</code> is initialized to 8<br>
     *<li><code>start</code> is initialized to 8<br>
     *<li><code>end</code> is initialized to <code>size</code><br>
     *<li><code>version</code> is initialized to 4<br>
     *<li><code>bitInfo</code> is initialized to all bits off<br>
     *<li><code>magicNumber</code> is initialized to <code>MAGIC_NUMBER</code><br>
     *</ul>
     * @param size the size of the block in ints.
     * @param number the block number--usually sequential.
     */
    public BlockHeaderV4(int size, int number) {
        this.size = size;
        this.number = number;
        headerLength = 8;
        version = 4;
        eventCount = 0;
        compressedLength = 0;
        reserved2 = 0;
        magicNumber = MAGIC_NUMBER;
    }

    /**
     * This copy constructor creates an evio version 4 BlockHeader
     * from another object of this class.
     * @param blkHeader block header object to copy
     */
    public BlockHeaderV4(BlockHeaderV4 blkHeader) {
        if (blkHeader == null) {
            return;
        }
        swap             = blkHeader.swap;
        size             = blkHeader.size;
        number           = blkHeader.number;
        headerLength     = blkHeader.headerLength;
        version          = blkHeader.version;
        eventCount       = blkHeader.eventCount;
        compressedLength = blkHeader.compressedLength;
        reserved2        = blkHeader.reserved2;
        byteOrder        = blkHeader.byteOrder;
        magicNumber      = blkHeader.magicNumber;
        bitInfo          = (BitSet)  blkHeader.bitInfo.clone();
        bufferStartingPosition = blkHeader.bufferStartingPosition;
    }



    /*** {@inheritDoc}  */
    public Object clone() {
        try {
            BlockHeaderV4 result = (BlockHeaderV4)super.clone();
            result.bitInfo = (BitSet)bitInfo.clone();
            return result;
        }
        catch (CloneNotSupportedException e) {
           return null;
        }
    }

    /**
 	 * Was the data from buffer from which this header was read, swapped?
 	 *
 	 * @return <code>true</code> if data from buffer was swapped, else <code>false</code>
 	 */
 	public boolean isSwapped() {return swap;}

    /**
     * Set whether the data from buffer from which this header was read was swapped.
   	 *
   	 * @param swap <code>true</code> if data from buffer was swapped, else <code>false</code>
   	 */
   	public void swapped(boolean swap) { this.swap = swap; }

    /**
	 * Get the size of the block (physical record).
	 *
	 * @return the size of the block (physical record) in ints.
	 */
	public int getSize() {
		return size;
	}

    /**
   	 * Set the size of the block (physical record). Some trivial checking is done.
   	 *
   	 * @param size the new value for the size, in ints.
     * @throws EvioException if size arg &lt; 8
     */
    public void setSize(int size) throws EvioException {
        if (size < 8) {
            throw new EvioException(String.format("Bad value for size in block (physical record) header: %d", size));
        }

        this.size = size;
    }

    /**
     * Get the number of events completely contained in the block.
     * NOTE: There are no partial events, only complete events stored in one block.
     *
     * @return the number of events in the block.
     */
    public int getEventCount() {
        return eventCount;
    }

    /**
     * Set the number of events completely contained in the block.
     * NOTE: There are no partial events, only complete events stored in one block.
     *
     * @param count the new number of events in the block.
     * @throws EvioException if count arg negative
     */
    public void setEventCount(int count) throws EvioException {
        if (count < 0) {
            throw new EvioException(String.format("Bad value for event count in block (physical record) header: %d", count));
        }
        this.eventCount = count;
    }

	/**
	 * Get the block number for this block (physical record).
     * In a file, this is usually sequential, starting at 1.
	 *
	 * @return the block number for this block (physical record).
	 */
	public int getNumber() {
		return number;
	}

	/**
	 * Set the block number for this block (physical record).
     * In a file, this is usually sequential starting at 1.
     * This is not checked.
	 *
	 * @param number the number of the block (physical record).
	 */
	public void setNumber(int number) {
		this.number = number;
	}

	/**
	 * Get the block header length, in ints. This should be 8.
	 *
	 * @return the block header length. This should be 8.
	 */
	public int getHeaderLength() {
		return headerLength;
	}

	/**
	 * Set the block header length, in ints. Although technically speaking this value
     * is variable, it must be &gt;= 8. Unless compressed data is used, this should
     * be exactly 8.
	 *
	 * @param headerLength the new block header length. Normally 8.
     * @throws EvioException if headerLength arg &lt; 8
	 */
	public void setHeaderLength(int headerLength) throws EvioException  {
		if (headerLength < HEADER_SIZE) {
            throw new EvioException("header length too small (must be >= 8)");
		}

		this.headerLength = headerLength;
	}

    //----------------------------------------------------------------------------------------

    /**
     * Get the evio version of the block (physical record) header.
     *
     * @return the evio version of the block (physical record) header.
     */
    public int getVersion() {
        return version;
    }

    /**
     * Sets the evio version. Should be 4 but no check is performed here, see
     * {@link EvioReader#processNextBlock()}.
     *
     * @param version the evio version of evio.
     */
    public void setVersion(int version) {
        this.version = version;
    }

    //----------------------------------------------------------------------------------------

    /**
     * Does this integer indicate that there is an evio dictionary
     * (assuming it's the header's sixth word)?
     *
     * @param i a block header's sixth word
     * @return <code>true</code> if this int indicates an evio dictionary, else <code>false</code>
     */
    static public boolean hasDictionary(int i) {
        return ((i & EV_DICTIONARY_MASK) > 0);
    }

    /**
     * Is this block's first event an evio dictionary?
     *
     * @return <code>true</code> if this block's first event is an evio dictionary, else <code>false</code>
     */
    public boolean hasDictionary() {
        return bitInfo.get(0);
    }

    //----------------------------------------------------------------------------------------

    /** {@inheritDoc} */
    public boolean isLastBlock() {
        return bitInfo.get(1);
    }

    /**
     * Does this integer indicate that this is the last block
     * (assuming it's the header's sixth word)?
     *
     * @param i a block header's sixth word
     * @return <code>true</code> if this int indicates the last block, else <code>false</code>
     */
    static public boolean isLastBlock(int i) {
        return ((i & EV_LASTBLOCK_MASK) > 0);
    }

    /**
     * Set the bit in the given arg which indicates this is the last block.
     * @param i integer in which to set the last-block bit
     * @return  arg with last-block bit set
     */
    static public int setLastBlockBit(int i)   {
        return (i |= EV_LASTBLOCK_MASK);
    }

    /**
     * Clear the bit in the given arg to indicate it is NOT the last block.
     * @param i integer in which to clear the last-block bit
     * @return arg with last-block bit cleared
     */
    static public int clearLastBlockBit(int i) {
        return (i &= ~EV_LASTBLOCK_MASK);
    }

    //----------------------------------------------------------------------------------------

    /**
     * Does this block in the file contain the "first event" (first event
     * to be written to each file split)?
     *
     * @return <code>true</code> if this is the first event, else <code>false</code>
     */
    public boolean hasFirstEvent() { return bitInfo.get(EV_BIT_FIRSTEVENT); }

    /**
     * Does this integer indicate that block has the first event
     * (assuming it's the header's sixth word)? Only makes sense if the
     * integer arg comes from the first block header of a file or buffer.
     *
     * @param i a block header's sixth word
     * @return <code>true</code> if this int indicates the block has a first event,
     *         else <code>false</code>
     */
    static public boolean hasFirstEvent(int i) {
        return ((i & EV_FIRSTEVENT_MASK) > 0);
    }

    /**
     * Set the bit in the given arg which indicates this block has a first event.
     * @param i integer in which to set the last-block bit
     * @return  arg with first event bit set
     */
    static public int setFirstEventBit(int i)   {
        return (i |= EV_FIRSTEVENT_MASK);
    }

    /**
     * Clear the bit in the given arg to indicate this block does NOT have a first event.
     * @param i integer in which to clear the first event bit
     * @return arg with first event bit cleared
     */
    static public int clearFirstEventBit(int i) {
        return (i &= ~EV_FIRSTEVENT_MASK);
    }

   //----------------------------------------------------------------------------------------

    /**
     * Does this block contain an array of event byte lengths?
     * @return {@code true} if block contains array of event byte lengths,
     *         else {@code false}.
     */
    public boolean hasEventLengths() { return bitInfo.get(EV_BIT_EVENTLENGTHS); }

    /**
     * Does this integer indicate that this block contains an array
     * of event byte lengths (assuming it's the header's sixth word)?
     * @param i a block header's sixth word
     * @return {@code true} if this int indicates block contains array of event byte lengths,
     *         else {@code false}.
     */
    static public boolean hasEventLengths(int i) {
        return ((i & EV_EVENTLENGTHS_MASK) > 0);
    }

    /**
     * Set the bit in the given arg which indicates this block contains an array
     * of event byte lengths.
     * @param i integer in which to set the events-lengths bit
     * @return  arg with events-lengths bit set
     */
    static public int setEventLengthsBit(int i)   {
        return (i |= EV_EVENTLENGTHS_MASK);
    }

    /**
     * Clear the bit in the given arg to indicate this block does NOT contain an array
     * of event byte lengths.
     * @param i integer in which to clear the events-lengths bit
     * @return arg with events-lengths bit cleared
     */
    static public int clearEventLengthsBit(int i) {
        return (i &= ~EV_EVENTLENGTHS_MASK);
    }

    //----------------------------------------------------------------------------------------

    /** {@inheritDoc} */
    public boolean isCompressed() { return bitInfo.get(EV_BIT_COMPRESSED); }

    /**
     * Does this integer indicate that the data following this block header
     * is compressed in LZ4 format (assuming it's the header's sixth word)?
     *
     * @param i a block header's sixth word
     * @return <code>true</code> if this int indicates that the data
     *         following this block header is compressed in LZ4 format,
     *         else <code>false</code>.
     */
    static public boolean isCompressed(int i) {
        return ((i & EV_LZ4COMPRESSION_MASK) > 0);
    }

    /**
     * Set the bit in the given arg which indicates this block has compressed data.
     * @param i integer in which to set the compressed-data bit
     * @return  arg with compressed-data bit set
     */
    static public int setCompressedBit(int i)   {
        return (i |= EV_LZ4COMPRESSION_MASK);
    }

    /**
     * Clear the bit in the given arg to indicate this block does NOT have compressed data.
     * @param i integer in which to clear the compressed-data bit
     * @return arg with compressed-data bit cleared
     */
    static public int clearCompressedBit(int i) {
        return (i &= ~EV_LZ4COMPRESSION_MASK);
    }

    //----------------------------------------------------------------------------------------

    /**
     * Get the value of bits 2-5. It represents the type of event being sent.
     *
     * @return bits 2-5 as an integer, representing event type.
     */
    public int getEventType() {
        int type=0;
        boolean bitSet;
        for (int i=0; i < 4; i++) {
            bitSet = bitInfo.get(i+2);
            if (bitSet) type |= 1 << i;
        }
        return type;
    }

    /**
     * Encode the "is first event" into the bit info word
     * which will be in evio block header.
     *
     * @param bSet bit set which will become part of the bit info word
     */
    static public void setFirstEvent(BitSet bSet) {
        if (bSet == null || bSet.size() < 7) {
            return;
        }

        // Encoding bit #15 (#6 since first is bit #9)
        bSet.set(6, true);
    }


    /**
     * Encode the "is NOT first event" into the bit info word
     * which will be in evio block header.
     *
     * @param bSet bit set which will become part of the bit info word
     */
    static public void unsetFirstEvent(BitSet bSet) {
        if (bSet == null || bSet.size() < 7) {
            return;
        }

        // Encoding bit #15 (#6 since first is bit #9)
        bSet.set(6, false);
    }



    /**
     * Gets a copy of all stored bit information.
     *
     * @return copy of BitSet containing all stored bit information.
     */
    public BitSet getBitInfo() {
        return (BitSet)bitInfo.clone();
    }

    /**
     * Gets the value of a particular bit in the bitInfo field.
     *
     * @param bitIndex index of bit to get
     * @return BitSet containing all stored bit information.
     */
    public boolean getBitInfo(int bitIndex) {
        if (bitIndex < 0 || bitIndex > 23) {
            return false;
        }
        return bitInfo.get(bitIndex);
    }

    /**
     * Sets a particular bit in the bitInfo field.
     *
     * @param bitIndex index of bit to change
     * @param value value to set bit to
     */
    public void setBit(int bitIndex, boolean value) {
        if (bitIndex < 0 || bitIndex > 23) {
            return;
        }
        bitInfo.set(bitIndex, value);
    }

    /**
     * Sets the right bits in bit set (2-5 when starting at 0)
     * to hold 4 bits of the given type value. Useful when
     * generating a bitset for use with {@link EventWriter}
     * constructor.
     *
     * @param bSet Bitset containing all bits to be set
     * @param type event type as int
     */
    static public void setEventType(BitSet bSet, int type) {
        if (type < 0) type = 0;
        else if (type > 15) type = 15;

        if (bSet.size() < 6) return;

        for (int i=2; i < 6; i++) {
            bSet.set(i, ((type >>> i-2) & 0x1) > 0);
        }
    }

    /**
     * Calculates the sixth word of this header which has the version number
     * in the lowest 8 bits and the bit info in the highest 24 bits.
     *
     * @return sixth word of this header.
     */
    public int getSixthWord() {
        int v = version & 0xff;

        for (int i=0; i < bitInfo.length(); i++) {
            if (bitInfo.get(i)) {
                v |= (0x1 << (8+i));
            }
        }

        return v;
    }

    /**
     * Calculates the sixth word of this header which has the version number (4)
     * in the lowest 8 bits and the set in the upper 24 bits.
     *
     * @param set Bitset containing all bits to be set
     * @return generated sixth word of this header.
     */
    static public int generateSixthWord(BitSet set) {
        return generateSixthWord(set, 4, false,
                                 false, false, false,
                                 false, 0);
    }

    /**
     * Calculates the sixth word of this header which has the version number (4)
     * in the lowest 8 bits and the set in the upper 24 bits. The arg isDictionary
     * is set in the 9th bit and isEnd is set in the 10th bit.
     *
     * @param bSet Bitset containing all bits to be set
     * @param hasDictionary does this block include an evio xml dictionary as the first event?
     * @param isEnd is this the last block of a file or a buffer?
     * @return generated sixth word of this header.
     */
    static public int generateSixthWord(BitSet bSet, boolean hasDictionary, boolean isEnd) {
        return generateSixthWord(bSet, 4, hasDictionary,
                                 isEnd, false, false,
                                 false, 0);
    }

    /**
     * Calculates the sixth word of this header which has the version number
     * in the lowest 8 bits. The arg hasDictionary
     * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
     * (event type) are set in bits 11-14.
     *
     * @param version evio version number
     * @param hasDictionary does this block include an evio xml dictionary as the first event?
     * @param isEnd is this the last block of a file or a buffer?
     * @param eventType 4 bit type of events header is containing
     * @return generated sixth word of this header.
     */
    static public int generateSixthWord(int version, boolean hasDictionary,
                                        boolean isEnd, int eventType) {

        return generateSixthWord(null, version, hasDictionary, isEnd,
                                 false, false,
                                 false, eventType);
    }

    /**
      * Calculates the sixth word of this header which has the version number (4)
      * in the lowest 8 bits and the set in the upper 24 bits. The arg isDictionary
      * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
      * (event type) are set in bits 11-14.
      *
      * @param bSet Bitset containing all bits to be set
      * @param version evio version number
      * @param hasDictionary does this block include an evio xml dictionary as the first event?
      * @param isEnd is this the last block of a file or a buffer?
      * @param eventType 4 bit type of events header is containing
      * @return generated sixth word of this header.
      */
     static public int generateSixthWord(BitSet bSet, int version,
                                         boolean hasDictionary,
                                         boolean isEnd, int eventType) {
         return generateSixthWord(bSet, version, hasDictionary,
                                  isEnd, false, false,
                                  false, eventType);
     }

    /**
     * Calculates the sixth word of this header which has the version number (4)
     * in the lowest 8 bits and the set in the upper 24 bits. The arg isDictionary
     * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
     * (event type) are set in bits 11-14.
     *
     * @param bSet Bitset containing all bits to be set
     * @param version evio version number
     * @param hasDictionary does this block include an evio xml dictionary as the first event?
     * @param isEnd is this the last block of a file or a buffer?
     * @param isCompressed is data of this block compressed?
     * @param hasFirst does this block include a first event for file splitting?
     * @param hasEventSizes does this block include an array of event sizes in header?
     * @param eventType 4 bit type of events header is containing
     * @return generated sixth word of this header.
     */
     static public int generateSixthWord(BitSet bSet, int version,
                                         boolean hasDictionary,
                                         boolean isEnd, boolean hasFirst,
                                         boolean isCompressed, boolean hasEventSizes,
                                         int eventType) {
         int v = version; // version

         if (bSet != null) {
             for (int i=0; i < bSet.length(); i++) {
                 if (i > 23) {
                     break;
                 }
                 if (bSet.get(i)) {
                     v |= (0x1 << (8+i));
                 }
             }
         }

         v =  hasDictionary ? (v | EV_DICTIONARY_MASK)     : v;
         v =  isEnd ?         (v | EV_LASTBLOCK_MASK)      : v;
         v =  hasFirst ?      (v | EV_FIRSTEVENT_MASK)     : v;
         v =  isCompressed ?  (v | EV_LZ4COMPRESSION_MASK) : v;
         v =  hasEventSizes ? (v | EV_EVENTLENGTHS_MASK)   : v;
         v |= ((eventType & 0xf) << 10);

         return v;
     }


    /**
     * Parses the argument into the bit info fields.
     * This ignores the version in the lowest 8 bits.
     *
     * @param word integer to parse into bit info fields
     */
    public void parseToBitInfo(int word) {
        for (int i=0; i < 24; i++) {
            bitInfo.set(i, ((word >>> 8+i) & 0x1) > 0);
        }
    }


    /**
     * Get the compressed data length in BYTES if data is compressed.
     * Get the CODA id if being using in CODA online.
     * @return compressed data length.
     */
    public int getCompressedLength() {
        return compressedLength;
    }

    /**
     * Set the compressed data length in BYTES if data is compressed.
     * Set the CODA id if being using in CODA online.
     * @param compressedLength compressed data length.
     */
    public void setCompressedLength(int compressedLength) {
        this.compressedLength = compressedLength;
    }

    /**
     * Get the second reserved word.
     * @return the second reserved word.
     */
    public int getReserved2() {
        return reserved2;
    }

    /**
     * Sets the value of second reserved word.
     * @param reserved2 the value for second reserved word.
     */
    public void setReserved2(int reserved2) {
        this.reserved2 = reserved2;
    }

    /**{@inheritDoc} */
	public int getMagicNumber() {
		return magicNumber;
	}

    /**
     * Sets the value of magicNumber. Strictly speaking this method should
     * have no argument, but it does act as and error check.
     * It must match the constant MAGIC_NUMBER.
     * If it doesn't, some obvious possibilities: <br>
     * 1) The evio data (perhaps from a file) is screwed up.<br>
     * 2) The reading algorithm is screwed up. <br>
     * 3) The endianess is not being handled properly.
     *
     * @param magicNumber the new value for magic number.
     * @throws EvioException if arg is not equal to {@link #MAGIC_NUMBER}
     */
	public void setMagicNumber(int magicNumber) throws EvioException {
		if (magicNumber != MAGIC_NUMBER) {
			throw new EvioException(String.format("Value for magicNumber 0x%08x does not match MAGIC_NUMBER 0xc0da0100.",
                                                  magicNumber));
		}
		this.magicNumber = magicNumber;
	}

    /** {@inheritDoc} */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
   	 * Sets the byte order of data being read.
   	 *
   	 * @param byteOrder the new value for data's byte order.
   	 */
   	public void setByteOrder(ByteOrder byteOrder) {
   		this.byteOrder = byteOrder;
   	}

    /**{@inheritDoc} */
	@Override
	public String toString() {
		StringBuffer sb = new StringBuffer(512);
		sb.append(String.format("block size:     %d\n", size));
		sb.append(String.format("number:         %d\n", number));
		sb.append(String.format("headerLen:      %d\n", headerLength));
        sb.append(String.format("event count:    %d\n", eventCount));
        sb.append(String.format("compressedLen:  %d\n", compressedLength));
        sb.append(String.format("bit info:       %s\n", bitInfo));
        sb.append(String.format("has dictionary: %b\n", hasDictionary()));
        sb.append(String.format("has 1st event:  %b\n", hasFirstEvent()));
        sb.append(String.format("is compressed:  %b\n", isCompressed()));
        sb.append(String.format("is end:         %b\n", isLastBlock()));
        sb.append(String.format("version:        %d\n", version));
		sb.append(String.format("magicNumber:    %8x\n", magicNumber));
		sb.append(String.format(" *buffer start: %d\n", getBufferStartingPosition()));
		sb.append(String.format(" *next   start: %d\n", nextBufferStartingPosition()));
		return sb.toString();
	}

    /**{@inheritDoc} */
    public long getBufferEndingPosition() {
        return bufferStartingPosition + 4*size;
    }

    /**{@inheritDoc} */
    public long getBufferStartingPosition() {
        return bufferStartingPosition;
    }

    /**{@inheritDoc} */
	public void setBufferStartingPosition(long bufferStartingPosition) {
		this.bufferStartingPosition = bufferStartingPosition;
	}

    /**{@inheritDoc} */
	public long nextBufferStartingPosition() {
        return getBufferEndingPosition();
	}

    /**{@inheritDoc} */
	public long firstEventStartingPosition() {
		// first data word is start of first event
        return bufferStartingPosition + 4*headerLength;
	}

    /**{@inheritDoc} */
   	public int bytesRemaining(long position) throws EvioException {
   		if (position < bufferStartingPosition) {
   			throw new EvioException("Provided position is less than buffer starting position.");
   		}

   		long nextBufferStart = nextBufferStartingPosition();
   		if (position > nextBufferStart) {
   			throw new EvioException("Provided position beyond buffer end position.");
   		}

   		return (int)(nextBufferStart - position);
   	}

    /**
	 * Gives the data (not header) bytes remaining in this block given a buffer position.
     * The position is an absolute position in a byte buffer. However, in the case of
     * compressed data, no header is contained in this buffer. Therefore this needs
     * to be accounted for. Otherwise this method is like {@link #bytesRemaining(long)}.
     *
     * @param position the absolute current position is a byte buffer.
     * @return the number of data bytes remaining in this block.
     * @throws EvioException if position arg out of bounds
     */
   	public int dataBytesRemaining(long position) throws EvioException {
   		if (position < bufferStartingPosition) {
   			throw new EvioException("Provided position is less than buffer starting position.");
   		}

   		long nextBufferStart = bufferStartingPosition + 4*(size - headerLength);
   		if (position > nextBufferStart) {
   			throw new EvioException("Provided position beyond buffer end position.");
   		}

   		return (int)(nextBufferStart - position);
   	}

    /**{@inheritDoc} */
	public int write(ByteBuffer byteBuffer) {
		byteBuffer.putInt(size);
		byteBuffer.putInt(number);
		byteBuffer.putInt(headerLength); // should always be 8
		byteBuffer.putInt(eventCount);
		if (isCompressed) {
            byteBuffer.putInt(compressedLength);
        }
        else {
            byteBuffer.putInt(0);            // unused
        }
		byteBuffer.putInt(getSixthWord());
		byteBuffer.putInt(0);            // unused
		byteBuffer.putInt(magicNumber);
		return 32;
	}
}