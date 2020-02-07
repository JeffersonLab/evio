/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/22/2019
 * @author timmer
 */

#ifndef EVIO_6_0_EVIONODE_H
#define EVIO_6_0_EVIONODE_H

#include <cstdint>
#include <sstream>
#include <memory>
#include <vector>
#include <algorithm>
#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "DataType.h"
#include "RecordNode.h"
#include "EvioException.h"


namespace evio {


// Forward declaration to fix chicken-egg problem in compilation
class EvioNodeSource;


/**
 * This class is used to store relevant info about an evio container
 * (bank, segment, or tag segment), without having
 * to de-serialize it into many objects and arrays.
 * It is not thread-safe and is designed for speed.
 *
 * @author timmer
 * @date 07/22/2019
 */
class EvioNode {

private:

    /** Header's length value (32-bit words). */
    uint32_t len = 0;
    /** Header's tag value. */
    uint32_t tag = 0;
    /** Header's num value. */
    uint32_t num = 0;
    /** Header's padding value. */
    uint32_t pad = 0;
    /** Position of header in buffer in bytes.  */
    uint32_t pos = 0;
    /** This node's (evio container's) type. Must be bank, segment, or tag segment. */
    uint32_t type = 0;

    /** Length of node's data in 32-bit words. */
    uint32_t dataLen = 0;
    /** Position of node's data in buffer in bytes. */
    uint32_t dataPos = 0;
    /** Type of data stored in node. */
    uint32_t dataType = 0;

    /** Position of the record in buffer containing this node in bytes
     *  @since version 6. */
    uint32_t recordPos = 0;

    /** Store data in int array form if calculated. */
    vector<uint32_t> data;

    /** Does this node represent an event (top-level bank)? */
    bool izEvent = false;

    /** If the data this node represents is removed from the buffer,
*  then this object is obsolete. */
    bool obsolete = false;

    /** ByteBuffer that this node is associated with. */
    shared_ptr<ByteBuffer> buffer;

    /** List of child nodes ordered according to placement in buffer. */
    vector<shared_ptr<EvioNode>> childNodes;

    //-------------------------------
    // For event-level node
    //-------------------------------

    /**
     * Place of containing event in file/buffer. First event = 0, second = 1, etc.
     * Useful for converting node to EvioEvent object (de-serializing).
     */
    uint32_t place = 0;

    /**
     * If top-level event node, was I scanned and all my banks
     * already placed into a list?
     */
    bool scanned = false;

    /** List of all nodes in the event including the top-level object
     *  ordered according to placement in buffer.
     *  Only created at the top-level (with constructor).
     *  All nodes have a reference to the top-level allNodes object. */
    vector<shared_ptr<EvioNode>> allNodes;

    //-------------------------------
    // For sub event-level node
    //-------------------------------

    /** Node of event containing this node. Is null if this is an event node. */
    shared_ptr<EvioNode> eventNode;

    /** Node containing this node. Is null if this is an event node. */
    shared_ptr<EvioNode> parentNode;

    //-------------------------------
    // For testing
    //-------------------------------
    /** If in pool, the pool's id. */
    int poolId = -1;



private:

    /** Record containing this node. */
    RecordNode recordNode;

private:

    void copyParentForScan(std::shared_ptr<EvioNode> & parent);
    void addChild(std::shared_ptr<EvioNode> & node);
    void addToAllNodes(std::shared_ptr<EvioNode> & node);
    void removeFromAllNodes(std::shared_ptr<EvioNode> & node);
    void removeChild(std::shared_ptr<EvioNode> & node);
    RecordNode & getRecordNode(); // public?
    void copy(const EvioNode & src);

protected:

    explicit EvioNode(shared_ptr<EvioNode> & firstNode);

public:

    EvioNode();
    EvioNode(const EvioNode & firstNode);
    explicit EvioNode(int id);
    explicit EvioNode(const std::shared_ptr<EvioNode> & src);
    EvioNode(EvioNode && src) noexcept;
    EvioNode(uint32_t pos, uint32_t place, shared_ptr<ByteBuffer> & buffer, RecordNode & blockNode);
    EvioNode(uint32_t pos, uint32_t place, uint32_t recordPos, std::shared_ptr<ByteBuffer> & buffer);
    EvioNode(uint32_t tag, uint32_t num, uint32_t pos, uint32_t dataPos,
             DataType & type, DataType & dataType, shared_ptr<ByteBuffer> & buffer);

    ~EvioNode() = default;

    static void scanStructure(std::shared_ptr<EvioNode> & node);
    static void scanStructure(std::shared_ptr<EvioNode> & node, EvioNodeSource & nodeSource);

    static std::shared_ptr<EvioNode> & extractNode(std::shared_ptr<EvioNode> & bankNode, uint32_t position);
    static std::shared_ptr<EvioNode> extractEventNode(std::shared_ptr<ByteBuffer> & buffer,
                                                      RecordNode & recNode,
                                                      uint32_t position, uint32_t place);
    static std::shared_ptr<EvioNode> extractEventNode(std::shared_ptr<ByteBuffer> & buffer,
                                                      EvioNodeSource & pool,
                                                      RecordNode & recNode,
                                                      uint32_t position, uint32_t place);
    static std::shared_ptr<EvioNode> extractEventNode(std::shared_ptr<ByteBuffer> & buffer,
                                                      uint32_t recPosition,
                                                      uint32_t position, uint32_t place);
    static std::shared_ptr<EvioNode> extractEventNode(std::shared_ptr<ByteBuffer> & buffer, EvioNodeSource & pool,
                                                      uint32_t recPosition, uint32_t position,
                                                      uint32_t place);

    EvioNode & operator=(const EvioNode& other);
    bool operator==(const EvioNode& src) const;

    EvioNode & shift(int deltaPos);
    string toString();

    void clearLists();
    void clear();
    void clearObjects();
    void clearIntArray();

    void setBuffer(shared_ptr<ByteBuffer> & buf);
    void setData(uint32_t position, uint32_t plc, std::shared_ptr<ByteBuffer> & buf, RecordNode & recNode);
    void setData(uint32_t position, uint32_t plc, uint32_t recPos, std::shared_ptr<ByteBuffer> & buf);


    bool isObsolete();
    void setObsolete(bool obsolete);
    std::vector<std::shared_ptr<EvioNode>> & getAllNodes();
    std::vector<std::shared_ptr<EvioNode>> & getChildNodes();
    void getAllDescendants(std::vector<std::shared_ptr<EvioNode>> & descendants);
    std::shared_ptr<EvioNode> getChildAt(uint32_t index);
    uint32_t getChildCount();

    std::shared_ptr<ByteBuffer> getBuffer();

    uint32_t getLength();
    uint32_t getTotalBytes();
    uint32_t getTag();
    uint32_t getNum();
    uint32_t getPad();
    uint32_t getPosition();
    uint32_t getType();
    DataType getTypeObj();
    uint32_t getDataLength();
    uint32_t getDataPosition();
    uint32_t getDataType();
    DataType getDataTypeObj();
    uint32_t getRecordPosition();
    uint32_t getPlace();

    std::shared_ptr<EvioNode> getParentNode();
    uint32_t getEventNumber();
    bool isEvent();

    void updateLengths(int deltaLen);
    void updateTag(uint32_t newTag);
    void updateNum(uint8_t newNum);

    ByteBuffer & getByteData(ByteBuffer & dest, bool copy);
    std::vector<uint32_t> & getIntData();
    void getIntData(vector<uint32_t> & intData);
    void getLongData(vector<uint64_t> & longData);
    void getShortData(vector<uint16_t> & shortData);
    ByteBuffer & getStructureBuffer(ByteBuffer & dest, bool copy);
};

}

#endif //EVIO_6_0_EVIONODE_H
