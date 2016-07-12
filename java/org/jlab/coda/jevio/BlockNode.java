/*
 * Copyright (c) 2016, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio;


/**
 * This class is used to store relevant info about an evio block
 * along with its position in a buffer.
 *
 * @author timmer
 * Date: 11/13/12
 */
final class BlockNode {
    /** Block's length value (32-bit words). */
    int len;
    /** Number of events in block. */
    int count;
    /** Position of block in file/buffer.  */
    int pos;
    /**
     * Place of this block in file/buffer. First block = 0, second = 1, etc.
     * Useful for appending banks to EvioEvent object.
     */
    int place;

    //----------------------------------
    // Constructor (package accessible)
    //----------------------------------

    BlockNode() {}

    //-------------------------------
    // Methods
    //-------------------------------

    final public String toString() {
        StringBuilder builder = new StringBuilder(100);
        builder.append("len = ");     builder.append(len);
        builder.append(", count = "); builder.append(count);
        builder.append(", pos = ");   builder.append(pos);
        builder.append(", place = "); builder.append(place);

        return builder.toString();
    }

}
