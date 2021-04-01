package org.jlab.coda.hipo;

public enum CompressionType {
    /** No data compression. */
    RECORD_UNCOMPRESSED(0, "None"),
    /** Data compression using fastest LZ4 method. */
    RECORD_COMPRESSION_LZ4(1, "Lz4"),
    /** Data compression using slowest but most compact LZ4 method. */
    RECORD_COMPRESSION_LZ4_BEST(2, "Lz4 best"),
    /** Data compression using gzip method. */
    RECORD_COMPRESSION_GZIP(3, "Gzip");

    /** Value of this compression type. */
    private final int value;

    /** Shortened string describing this compression type. */
    private final String description;

    /**
     * Constructor.
     * @param val numerical value associated with this compression type.
     * @param d string describing this compression type.
     */
    CompressionType(int val, String d) {
        value = val;
        description = d;
    }


    /**
     * Get the description.
     * @return description.
     */
    public String getDescription() {
        return description;
    }

    /**
     * Get the integer value associated with this header type.
     * @return integer value associated with this header type.
     */
    int getValue() {return value;}

    /**
     * Is this a form of compression?
     * @return <code>true</code> if is a form of compression, else <code>false</code>
     */
    public boolean isCompressed() {return (this != RECORD_UNCOMPRESSED);}

    // Fill array after all enum objects created
    /** Fast way to convert integer values into CompressionType objects. */
    private static CompressionType[] intToType;

    static {
        intToType = new CompressionType[4];
        for (CompressionType type : values()) {
            intToType[type.value] = type;
        }
    }

    /**
     * Get the enum from the integer value.
     * @param val the value to match.
     * @return the matching enum, or <code>null</code>.
     */
    public static CompressionType getCompressionType(int val) {
        if (val > 4 || val < 0) return null;
        return intToType[val];
    }
}
