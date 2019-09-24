//
// Created by timmer on 7/22/19.
//

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

// Forward declaration to fix chicken-egg problem in compilation
class EvioNodeSource;


class EvioNode {

private:

    /** Header's length value (32-bit words). */
    uint32_t len;
    /** Header's tag value. */
    uint32_t tag;
    /** Header's num value. */
    uint32_t num;
    /** Header's padding value. */
    uint32_t pad;
    /** Position of header in buffer in bytes.  */
    uint32_t pos;
    /** This node's (evio container's) type. Must be bank, segment, or tag segment. */
    uint32_t type;

    /** Length of node's data in 32-bit words. */
    uint32_t dataLen;
    /** Position of node's data in buffer in bytes. */
    uint32_t dataPos;
    /** Type of data stored in node. */
    uint32_t dataType;

    /** Position of the record in buffer containing this node in bytes
     *  @since version 6. */
    uint32_t recordPos;

    /** Store data in int array form if calculated. */
    vector<uint32_t> data;

    /** Does this node represent an event (top-level bank)? */
    bool izEvent;

    /** If the data this node represents is removed from the buffer,
*  then this object is obsolete. */
    bool obsolete;

    /** ByteBuffer that this node is associated with. */
    ByteBuffer buffer;

    /** List of child nodes ordered according to placement in buffer. */
    vector<shared_ptr<EvioNode>> childNodes;

    //-------------------------------
    // For event-level node
    //-------------------------------

    /**
     * Place of containing event in file/buffer. First event = 0, second = 1, etc.
     * Useful for converting node to EvioEvent object (de-serializing).
     */
    uint32_t place;

    /**
     * If top-level event node, was I scanned and all my banks
     * already placed into a list?
     */
    bool scanned;

// TODO: put pointers in here???
    /** List of all nodes in the event including the top-level object
     *  ordered according to placement in buffer.
     *  Only created at the top-level (with constructor). All lower-level
     *  nodes are created with clone() so all nodes have a reference to
     *  the top-level allNodes object. */
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

    void copyParentForScan(EvioNode* parent);
    void addChild(EvioNode* node);
    void addToAllNodes(EvioNode & node);
    void removeFromAllNodes(shared_ptr<EvioNode> & node);
    void removeChild(shared_ptr<EvioNode> & node);
    RecordNode & getRecordNode(); // public?


protected:

    explicit EvioNode(shared_ptr<EvioNode> & firstNode);

public:

    EvioNode();
    explicit EvioNode(int id);
    EvioNode(const EvioNode & firstNode);
    EvioNode(EvioNode && src) noexcept ;
    EvioNode(uint32_t pos, uint32_t place, ByteBuffer & buffer, RecordNode & blockNode);
    EvioNode(uint32_t pos, uint32_t place, uint32_t recordPos, ByteBuffer & buffer);
    EvioNode(uint32_t tag, uint32_t num, uint32_t pos, uint32_t dataPos,
             DataType & type, DataType & dataType, ByteBuffer & buffer);

    //~EvioNode();

    static void scanStructure(EvioNode & node);
    static void scanStructure(EvioNode & node, EvioNodeSource & nodeSource);

    static EvioNode & extractNode(EvioNode & bankNode, uint32_t position);
    static EvioNode & extractEventNode(ByteBuffer & buffer,
                                       RecordNode & recNode,
                                       uint32_t position, uint32_t place);
    static EvioNode & extractEventNode(ByteBuffer & buffer,
                                       EvioNodeSource & pool,
                                       RecordNode & recNode,
                                       uint32_t position, uint32_t place);
    static EvioNode & extractEventNode(ByteBuffer & buffer,
                                       uint32_t recPosition,
                                       uint32_t position, uint32_t place);
    static EvioNode & extractEventNode(ByteBuffer & buffer, EvioNodeSource & pool,
                                       uint32_t recPosition, uint32_t position, uint32_t place);

    EvioNode & operator=(const EvioNode& other);
    bool operator==(const EvioNode& src) const;

    EvioNode & shift(int deltaPos);
    // instead of clone, let's do copy constructor
    //EvioNode & clone();
    string toString();

    void clearLists();
    void clear();
    void clearObjects();
    void clearAll();
    void clearIntArray();

    void setBuffer(ByteBuffer & buf);
    void setData(uint32_t position, uint32_t plc, ByteBuffer & buf, RecordNode & recNode);
    void setData(uint32_t position, uint32_t plc, uint32_t recPos, ByteBuffer & buf);


    bool isObsolete();
    void setObsolete(bool obsolete);
    vector<shared_ptr<EvioNode>> & getAllNodes();
    vector<shared_ptr<EvioNode>> & getChildNodes();
    void getAllDescendants(vector<shared_ptr<EvioNode>> & descendants);
    shared_ptr<EvioNode> getChildAt(uint32_t index);
    uint32_t getChildCount();

    ByteBuffer & getBuffer();

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

    shared_ptr<EvioNode> getParentNode();
    uint32_t getEventNumber();
    bool isEvent();

    void updateLengths(int deltaLen);
    void updateTag(uint32_t newTag);

    void updateNum(uint8_t newNum);
    ByteBuffer & getByteData(ByteBuffer & dest, bool copy);

    vector<uint32_t> & getIntData();
    void getIntData(vector<uint32_t> & intData);
    void getLongData(vector<uint64_t> & longData);
    void getShortData(vector<uint16_t> & shortData);
    ByteBuffer & getStructureBuffer(ByteBuffer & dest, bool copy);



    };

#endif //EVIO_6_0_EVIONODE_H
