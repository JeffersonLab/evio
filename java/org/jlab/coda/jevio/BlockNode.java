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
final public class BlockNode {
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

    public BlockNode() {}

    //-------------------------------
    // Methods
    //-------------------------------

    final void clear() {
        len = count = pos = place = 0;
    }

    final public int getLen() {return len;}
    final public void setLen(int len) {this.len = len;}

    final public int getCount() {return count;}
    final public void setCount(int count) {this.count = count;}

    final public int getPos() {return pos;}
    final public void setPos(int pos) {this.pos = pos;}

    final public int getPlace() {return place;}
    final public void setPlace(int place) {this.place = place;}

    final public String toString() {
        StringBuilder builder = new StringBuilder(100);
        builder.append("len = ");     builder.append(len);
        builder.append(", count = "); builder.append(count);
        builder.append(", pos = ");   builder.append(pos);
        builder.append(", place = "); builder.append(place);

        return builder.toString();
    }

}
