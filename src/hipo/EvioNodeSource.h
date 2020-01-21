/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 09/16/2019
 * @author timmer
 */

#ifndef EVIO_6_0_EVIONODESOURCE_H
#define EVIO_6_0_EVIONODESOURCE_H


#include <atomic>
#include <vector>
#include <memory>
#include "EvioNode.h"


namespace evio {


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
    vector<std::shared_ptr<EvioNode>> nodePool;

    /** For assigning pool id numbers. Initialized in EvioNode.cpp. */
    static std::atomic<unsigned int> poolIdCounter;

public:

    /** Default constructor with pool size = 1. */
    EvioNodeSource() {
        id = poolIdCounter.fetch_add(1);
        nodePool.push_back(std::make_shared<EvioNode>(id));
        size = 1;
    }


    /**
     * Constructor.
     * @param initialSize number of EvioNode objects in pool initially.
     */
    EvioNodeSource(uint32_t initialSize) {
        if (initialSize < 1) {
            initialSize = 1;
        }
        id = poolIdCounter.fetch_add(1);
        size = initialSize;
        nodePool.reserve(initialSize);
        // Create pool objects
        for (int i = 0; i < size; i++) {
            nodePool.push_back(std::make_shared<EvioNode>(id));
        }
    }


    /**
     * Get the id number of this pool.
     * Used for debugging.
     * @return id number of this pool.
     */
    uint32_t getId() const {return id;}

    /**
     * Get the number of nodes taken from pool.
     * @return number of nodes taken from pool.
     */
    uint32_t getUsed() const {return poolIndex;}

    /**
     * Get the number of nodes in the pool.
     * @return number of nodes in the pool.
     */
    uint32_t getSize() const {return size;}

    /**
     * Get a single EvioNode object.
     * @return EvioNode object.
     */
    std::shared_ptr<EvioNode> getNode() {
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
            nodePool[i]->clear();
        }
        poolIndex = 0;
    }

private:

    /** Increase the size of the pool by about 20% or at least 1. */
    void increasePool() {
        // Grow it by 20% at a shot
        int newPoolSize = size + (size + 4)/5;

        nodePool.reserve(newPoolSize);

        // Create new pool objects
        for (int i = size; i < newPoolSize; i++) {
            nodePool.push_back(std::make_shared<EvioNode>(id));
        }

        size = newPoolSize;
    }

};

}


#endif //EVIO_6_0_EVIONODESOURCE_H
