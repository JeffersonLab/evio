package org.jlab.coda.jevio;


import java.util.HashMap;

/**
 * This an enum used to convert data type numerical values to a more meaningful name.
 * For example, the data type with value 0x20 corresponds to the enum BANK.
 * Mostly this is used for printing. In this version of evio, the
 * ALSOTAGSEGMENT (0x40) value was removed from this enum because
 * the upper 2 bits of a byte containing the datatype are now used
 * to store padding data.
 * 
 * @author heddle
 * @author timmer
 *
 */
public enum DataType {

	UNKNOWN32      (0x0), 
	UINT32         (0x1), 
	FLOAT32        (0x2), 
	CHARSTAR8      (0x3), 
	SHORT16        (0x4), 
	USHORT16       (0x5), 
	CHAR8          (0x6), 
	UCHAR8         (0x7), 
	DOUBLE64       (0x8), 
	LONG64         (0x9), 
	ULONG64        (0xa), 
	INT32          (0xb), 
	TAGSEGMENT     (0xc), 
	ALSOSEGMENT    (0xd),
    ALSOBANK       (0xe),
    COMPOSITE      (0xf),
	BANK           (0x10),
	SEGMENT        (0x20),

    //  Remove ALSOTAGSEGMENT (0x40) since it was never
    //  used and we now need that to store padding data.
    //	ALSOTAGSEGMENT (0x40),

    // These 2 types are only used when dealing with COMPOSITE data.
    // They are never transported independently and are stored in integers.
    HOLLERIT       (0x41),
    NVALUE         (0x42);

    /** Each name is associated with a specific evio integer value. */
    private int value;


    /** Faster way to convert integer values into names. */
    private static HashMap<Integer, String> names = new HashMap<Integer, String>(32);

    /** Faster way to convert integer values into DataType objects. */
    private static HashMap<Integer, DataType> dataTypes = new HashMap<Integer, DataType>(32);


    // Fill static hashmaps after all enum objects created
    static {
        for (DataType item : DataType.values()) {
            dataTypes.put(item.value, item);
            names.put(item.value, item.name());
        }
    }


	/**
	 * Obtain the enum from the value.
	 *
	 * @param val the value to match.
	 * @return the matching enum, or <code>null</code>.
	 */
    public static DataType getDataType(int val) {
        return dataTypes.get(val);
    }


    /**
     * Obtain the name from the value.
     *
     * @param val the value to match.
     * @return the name, or "UNKNOWN".
     */
    public static String getName(int val) {
        String n = names.get(val);
        if (n != null) return n;
        return "UNKNOWN";
    }


    /**
     * Return a string which is usually the same as the name of the
     * enumerated value, except in the cases of ALSOSEGMENT and
     * ALSOBANK which return SEGMENT and BANK respectively.
     *
     * @return name of the enumerated type
     */
    public String toString() {
        if      (this == ALSOBANK) return "BANK";
        else if (this == ALSOSEGMENT) return "SEGMENT";
        return super.toString();
    }


    /**
     * Constructor.
     * @param value
     */
	private DataType(int value) {
		this.value = value;
	}


	/**
	 * Get the enum's value.
	 * 
	 * @return the value, e.g., 0xe for a BANK
	 */
	public int getValue() {
		return value;
	}


	/**
	 * Convenience routine to see if "this" data type is a structure (a container.)
	 * @return <code>true</code> if the data type corresponds to one of the structure
	 * types: BANK, SEGMENT, or TAGSEGMENT.
	 */
	public boolean isStructure() {
		switch (this) {
		case BANK:
		case SEGMENT:
		case TAGSEGMENT:
		case ALSOBANK:
		case ALSOSEGMENT:
//		case ALSOTAGSEGMENT:
			return true;
		default:
			return false;
		}
	}

    /**
   	 * Convenience routine to see if the given integer arg represents a data type which
     * is a structure (a container).
   	 * @return <code>true</code> if the data type corresponds to one of the structure
   	 * types: BANK, SEGMENT, or TAGSEGMENT.
   	 */
   	static public boolean isStructure(int dataType) {
   		switch (getDataType(dataType)) {
   		case BANK:
   		case SEGMENT:
   		case TAGSEGMENT:
   		case ALSOBANK:
   		case ALSOSEGMENT:
   			return true;
   		default:
   			return false;
   		}
   	}

}
