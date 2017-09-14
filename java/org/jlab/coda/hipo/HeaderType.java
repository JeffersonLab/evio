/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

/**
 * Numerical values associated with types or variations of RecordHeader.
 * The value associated with each enum member is stored in the
 * record header's bit-info word in the top 4 bits.
 * 
 * @version 6.0
 * @since 6.0 9/6/17
 * @author timmer
 */
public enum HeaderType {
    EVIO_RECORD        (0),
    EVIO_FILE          (1),
    EVIO_FILE_EXTENDED (2),
    EVIO_TRAILER       (3),
    HIPO_RECORD        (4),
    HIPO_FILE          (5),
    HIPO_FILE_EXTENDED (6),
    HIPO_TRAILER       (7);

    /** Value of this header type. */
    private int value;

    /** Constructor. */
    HeaderType(int val) {value = val;}

    /**
     * Get the integer value associated with this header type.
     * @return integer value associated with this header type.
     */
    int getValue() {return value;}

    /**
     * Is this an evio file header?
     * @return <code>true</code> if is an evio file header, else <code>false</code>
     */
    public boolean isEvioFileHeader() {return (this == EVIO_FILE || this == EVIO_FILE_EXTENDED);}

    /**
     * Is this a HIPO file header?
     * @return <code>true</code> if is an HIPO file header, else <code>false</code>
     */
    public boolean isHipoFileHeader() {return (this == HIPO_FILE || this == HIPO_FILE_EXTENDED);}

    /**
     * Is this a file header?
     * @return <code>true</code> if is a file header, else <code>false</code>
     */
    public boolean isFileHeader() {return (isEvioFileHeader() | isHipoFileHeader());}

    /** Fast way to convert integer values into HeaderType objects. */
    private static HeaderType[] intToType;

    // Fill array after all enum objects created
    static {
        intToType = new HeaderType[8];
        for (HeaderType type : values()) {
            intToType[type.value] = type;
        }
    }

    /**
     * Get the enum from the integer value.
     * @param val the value to match.
     * @return the matching enum, or <code>null</code>.
     */
    public static HeaderType getEventType(int val) {
        if (val > 7 || val < 0) return null;
        return intToType[val];
    }

    /**
     * Get the name from the integer value.
     * @param val the value to match.
     * @return the name, or <code>null</code>.
     */
    public static String getName(int val) {
        if (val > 7 || val < 0) return null;
        HeaderType type = getEventType(val);
        if (type == null) return null;
        return type.name();
    }

}
