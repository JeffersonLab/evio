package org.jlab.coda.jevio;

/**
 * This class is used to store relevant info about an evio block
 * along with its position in a buffer, without having to de-serialize anything.
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
}
