/*
 * Copyright (c) 2018, Jefferson Science Associates
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
 * This interface is used for a compact reader to class to get EvioNode objects
 * when parsing evio data.
 */
public interface EvioNodeSource {
    /**
     * Get a single EvioNode object.
     * @return EvioNode object.
     */
    EvioNode getNode();

    /**
     * Get the number of nodes taken from pool.
     * @return number of nodes taken from pool.
     */
    int getUsed();

    /**
     * Get the number of nodes in the pool.
     * @return number of nodes in the pool.
     */
    int getSize();

    /** Reset the source to initial condition. */
    void reset();
}
