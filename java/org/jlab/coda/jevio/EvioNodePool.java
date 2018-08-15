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
 * Array-based pool of EvioNode objects. This class is currently used for
 * parsing Evio data in event builders.
 */
public class EvioNodePool implements EvioNodeSource {
    /** Pool of EvioNode objects used for parsing Evio data in EBs. */
    private EvioNode[] nodePool;

    /** Current number of the pool objects in use. */
    private int poolIndex;

    /** Total size of the pool. */
    private int poolLimit;

    
    /**
     * Constructor.
     * @param initialSize number of EvioNode objects in pool initially.
     */
    public EvioNodePool(int initialSize) {
        poolLimit = initialSize;
        nodePool = new EvioNode[poolLimit];
        // Create pool objects
        for (int i=0; i < poolLimit; i++) {
            nodePool[i] = new EvioNode();
        }
    }


    /** {@inheritDoc} */
    public EvioNode getNode() {
        int currentIndex = poolIndex;
        if (poolIndex++ >= poolLimit) {
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
        int newPoolLimit = poolLimit + (poolLimit + 4)/5;

        EvioNode[] newPool = new EvioNode[newPoolLimit];

        // Copy over references of existing pool objects
        System.arraycopy(nodePool, 0, newPool, 0, poolLimit);

        // Create new pool objects
        for (int i=poolLimit; i < newPoolLimit; i++) {
            newPool[i] = new EvioNode();
        }

        nodePool  = newPool;
        poolLimit = newPoolLimit;
    }
}
