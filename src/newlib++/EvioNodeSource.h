//
// Created by timmer on 9/16/19.
//

#ifndef EVIO_6_0_EVIONODESOURCE_H
#define EVIO_6_0_EVIONODESOURCE_H


#include <atomic>
#include <vector>
#include "EvioNode.h"



/**
 * Vector-based pool of EvioNode objects. This class is currently used for
 * parsing Evio data in event builders. Not thread safe.
 */
class EvioNodeSource {

private:

    /** Index into nodePool array of the next pool object to use. */
    uint32_t poolIndex;

    /** Total size of the pool. */
    uint32_t size;

    /** Id of this pool. For debugging. */
    uint32_t id;

    /** EvioNode objects used for parsing Evio data in EBs. */
    vector<EvioNode> nodePool;

    /** For assigning pool id numbers. */
    static std::atomic<unsigned int> idCounter;

public:

    /**
     * Constructor.
     * @param initialSize number of EvioNode objects in pool initially.
     */
    EvioNodeSource(int initialSize) {
        id = idCounter.fetch_add(1);
        size = initialSize;
        nodePool.reserve(initialSize);
        // Create pool objects
        for (int i = 0; i < size; i++) {
            nodePool.emplace_back(id);
        }
    }


    /**
     * Get the id number of this pool.
     * Used for debugging.
     * @return id number of this pool.
     */
    int getId() {return id;}

    /**
     * Get the number of nodes taken from pool.
     * @return number of nodes taken from pool.
     */
    int getUsed() {return poolIndex;}

    /**
     * Get the number of nodes in the pool.
     * @return number of nodes in the pool.
     */
    int getSize() {return size;}

    /**
     * Get a single EvioNode object.
     * @return EvioNode object.
     */
    EvioNode & getNode() {
        int currentIndex = poolIndex;
        if (poolIndex++ >= size) {
            // It's better to increase the pool so these objects
            // don't get reallocated with each new event being parsed.
            increasePool();
        }
        return nodePool[currentIndex];
    }

    /** Reset the source to initial condition. */
    void reset() {
        for (int i=0; i < poolIndex; i++) {
            nodePool[i].clear();
        }
        poolIndex = 0;
    }

private:

    /** Increase the size of the pool by 20% or at least 1. */
    void increasePool() {
        // Grow it by 20% at a shot
        int newPoolSize = size + (size + 4)/5;

        nodePool.reserve(newPoolSize);

        // Create new pool objects
        for (int i = size; i < newPoolSize; i++) {
            nodePool.emplace_back(id);
        }

        size = newPoolSize;
    }

};


#endif //EVIO_6_0_EVIONODESOURCE_H
