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

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Array-based pool of EvioNode objects. This class is currently used for
 * parsing Evio data in event builders. Not thread safe.
 */
public class EvioNodePool implements EvioNodeSource {

    /** Pool of EvioNode objects used for parsing Evio data in EBs. */
    private EvioNode[] nodePool;

    /** Index into nodePool array of the next pool object to use. */
    private int poolIndex;

    /** Total size of the pool. */
    private int size;

    /** Id of this pool. For debugging. */
    private int id;
    
    /** For assigning pool id numbers. */
    static private AtomicInteger idCounter = new AtomicInteger();


    
    /**
     * Constructor.
     * @param initialSize number of EvioNode objects in pool initially.
     */
    public EvioNodePool(int initialSize) {
        id = idCounter.getAndIncrement();
        size = initialSize;
        nodePool = new EvioNode[size];
        // Create pool objects
        for (int i = 0; i < size; i++) {
            nodePool[i] = new EvioNode();
            nodePool[i].poolId = id;
        }
    }


    /**
     * Get the id number of this pool.
     * Used for debugging.
     * @return id number of this pool.
     */
    public int getId() {return id;}

    /**
     * Get the number of nodes taken from pool.
     * @return number of nodes taken from pool.
     */
    public int getUsed() {return poolIndex;}

    /**
     * Get the number of nodes in the pool.
     * @return number of nodes in the pool.
     */
    public int getSize() {return size;}

    /** {@inheritDoc} */
    public EvioNode getNode() {
        int currentIndex = poolIndex;
        if (poolIndex++ >= size) {
            // It's better to increase the pool so these objects
            // don't get reallocate with each new event being parsed.
            increasePool();
        }
        return nodePool[currentIndex];
    }


    /** {@inheritDoc} */
    public void reset() {
        for (int i=0; i < poolIndex; i++) {
            nodePool[i].clear();
        }
        poolIndex = 0;
    }


    /** Increase the size of the pool by 20% or at least 1. */
    private void increasePool() {
        // Grow it by 20% at a shot
        int newPoolSize = size + (size + 4)/5;

        EvioNode[] newPool = new EvioNode[newPoolSize];

        // Copy over references of existing pool objects
        System.arraycopy(nodePool, 0, newPool, 0, size);

        // Create new pool objects
        for (int i = size; i < newPoolSize; i++) {
            newPool[i] = new EvioNode();
        }

        nodePool = newPool;
        size = newPoolSize;
    }
}
