//
// Created by timmer on 7/22/19.
//

#ifndef EVIO_6_0_RECORDNODE_H
#define EVIO_6_0_RECORDNODE_H

#include <cstdint>
#include <sstream>


/**
 * This class is used to store relevant info about an evio record
 * along with its position in a buffer.
 *
 * @author timmer
 * Date: 7/23/2019
 */
class RecordNode {

private:

    /** Record's length value (32-bit words). */
    uint32_t len;
    /** Number of events in record. */
    uint32_t count;
    /** Position of record in file/buffer.  */
    uint32_t pos;
    /**
     * Place of this record in file/buffer. First record = 0, second = 1, etc.
     * Useful for appending banks to EvioEvent object.
     */
    uint32_t place;

public:

    RecordNode() {
        len = pos = count = place = 0;
    }

    void clear() {
        len = count = pos = place = 0;
    }


    uint32_t getLen() {return len;}
    void setLen(uint32_t l) {len = l;}

    uint32_t getCount() {return count;}
    void setCount(uint32_t c) {count = c;}

    uint32_t getPos() {return pos;}
    void setPos(uint32_t p) {pos = p;}

    uint32_t getPlace() {return place;}
    void setPlace(uint32_t p) {place = p;}

    string toString() {
        stringstream ss;

        ss << "len = "      << len;
        ss << ", count = "  << count;
        ss << ", pos = "    << pos;
        ss << ", place = "  << place;

        return ss.str();
    }


};


#endif //EVIO_6_0_RECORDNODE_H
