//
// Created by Carl Timmer on 2019-05-14.
//

#ifndef EVIO_6_0_CONCURRENTQUEUE_H
#define EVIO_6_0_CONCURRENTQUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>

using namespace std;

template<typename Data>

class ConcurrentQueue {

private:

    queue<Data> q;
    mutable mutex mtx;
    condition_variable cv;

public:

    /** Move assignment operator which moves the contents of other to lhs. */
    ConcurrentQueue & operator=(ConcurrentQueue&& other) noexcept {
        if (this == &other) {
            other.mtx.lock();
            q = std::move(other.q);
            other.mtx.unlock();
        }
        return *this;
    }

    /** Copy assignment operator. */
    ConcurrentQueue & operator=(const ConcurrentQueue& other) {
        // Note: mutexes and cvs cannot be copied or moved
        if (this == &other) {
            other.mtx.lock();
            q = other.q;
            other.mtx.unlock();
        }
        return *this;
    }

    void push(Data const& data) {
        mtx.lock();
        q.push(data);
        mtx.unlock();
        cv.notify_one();
    }

    bool empty() const {
        mtx.lock();
        bool b = q.empty();
        mtx.unlock();
        return b;
    }

    bool tryPop(Data& popped_value) {
        mtx.lock();
        if (q.empty()) {
            mtx.unlock();
            return false;
        }

        popped_value = q.front();
        q.pop();
        mtx.unlock();
        return true;
    }

    void waitPop(Data& popped_value) {
        mtx.lock();
        while(q.empty()) {
            cv.wait(lock);
        }

        popped_value = q.front();
        q.pop();
        mtx.unlock();
    }

    /**
     * Timed pop from this queue.
     * @param popped_value returned queue item.
     * @param millisec number of milliseconds to wait
     * @return true if returned value is valid, false if timeout.
     */
    bool waitPop(Data& popped_value, uint32_t millisec) {
        mtx.lock();
        while (q.empty()) {
            if (cv.wait_for(lock, millisec * 1ms) == std::cv_status::timeout) {
                return false;
            }
        }

        popped_value = q.front();
        q.pop();
        mtx.unlock();
        return true;
    }

};


#endif //EVIO_6_0_CONCURRENTQUEUE_H
