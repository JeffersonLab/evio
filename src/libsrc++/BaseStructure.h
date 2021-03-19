//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_BASESTRUCTURE_H
#define EVIO_6_0_BASESTRUCTURE_H


#include <vector>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <string>
#include <iomanip>
#include <limits>
#include <memory>
#include <type_traits>
#include <iterator>
#include <stack>
#include <vector>
#include <queue>
#include <utility>
#include <stdexcept>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "DataType.h"
#include "StructureType.h"
#include "EvioException.h"
#include "BaseStructureHeader.h"
#include "CompositeData.h"
#include "Util.h"
#include "IEvioListener.h"
#include "IEvioFilter.h"




namespace evio {


    /////////////////////////////////// DEPTH FIRST ITERATOR

    template<typename R>
    class nodeIterator {

        // iterator of vector contained shared pointers to node's children
        typedef typename std::vector<R>::iterator KidIter;

    protected:

        // Stack of pair containing 2 iterators, each iterating over vector
        // of node's children (shared pts).
        // In each pair, first is current iterator, second is end iterator.
        std::stack<std::pair<KidIter, KidIter>> stack;

        // Where we are now in the tree
        R currentNode;

        // Is this the end iterator?
        bool isEnd;

    public:

        // Copy shared pointer arg
        explicit nodeIterator(R &node, bool isEnd) : currentNode(node), isEnd(isEnd) {
            // store current-element and end of vector in pair
            if (!node->children.empty()) {
                std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
                stack.push(p);
            }
        }

        R operator*() const { return currentNode; }

        bool operator==(const nodeIterator &other) const {
            // Identify end iterator
            if (isEnd && other.isEnd) {
                return true;
            }
            return this == &other;
        }

        bool operator!=(const nodeIterator &other) const {
            // Identify end iterator
            if (isEnd && other.isEnd) {
                return false;
            }
            return this != &other;
        }

        bool isEndIter() {return isEnd;}

        // post increment gets ignored arg of 0 to distinguish from pre, A++
        const nodeIterator operator++(int) {

            if (isEnd) return *this;

            // copy this iterator here
            nodeIterator niter = *this;

            // If gone thru the whole tree ...
            if (stack.empty()) {
                isEnd = true;
                return niter;
            }

            // Look at top vector of stack
            auto &topPair = stack.top();
            // iterator of vector @ current position
            auto &curIter = topPair.first;
            // end iterator of vector
            auto &endIter = topPair.second;
            // current element of vector
            auto &node = *curIter;

            // If this vector has no more nodes ...
            if (curIter - (endIter - 1) == 0) {
                stack.pop();
            }

            // Prepare to look at the next node in the vector (next call)
            ++curIter;

            // If it has children, put pair of iterators on stack
            if (!node->children.empty()) {
                // Look at node's children
                std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
                stack.push(p);
            }

            currentNode = node;
            // return copy of this iterator before changes
            return niter;
        }


        // pre increment, ++A
        const nodeIterator &operator++() {

            if (isEnd) return *this;

            // If gone thru the whole tree ...
            if (stack.empty()) {
                isEnd = true;
                return *this;
            }

            // Look at top vector of stack
            auto &topPair = stack.top();
            // iterator of vector @ current position
            auto &curIter = topPair.first;
            // end iterator of vector
            auto &endIter = topPair.second;
            // current element of vector
            auto &node = *curIter;

            // If this vector has no more nodes ...
            if (curIter - (endIter - 1) == 0) {
                stack.pop();
            }

            // Prepare to look at the next node in the vector (next call)
            ++curIter;

            // If it has children, put pair of iterators on stack
            if (!node->children.empty()) {
                // Look at node's children
                std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
                stack.push(p);
            }

            currentNode = node;
            return *this;
        }
    };


    /////////////////////////////////// BREADTH FIRST ITERATOR

    template<typename R>
    class nodeBreadthIterator {

    protected:

        // iterator of vector contained shared pointers to node's children
        typedef typename std::vector<R>::iterator KidIter;

        // Stack of iterators of vector of shared pointers.
        // Each vector contains the children of a node,
        // thus each iterator gives all a node's kids.

        // Stack of iterators over node's children.
        // In each pair, first is current iterator, second is end
        std::queue<std::pair<KidIter, KidIter>> que;

        // Where we are now in the tree
        R currentNode;

        // Is this the end iterator?
        bool isEnd;

    public:

        // Copy shared pointer arg
        nodeBreadthIterator(R & node, bool isEnd) : currentNode(node), isEnd(isEnd) {
            // store current-element and end of vector in pair
            if (!node->children.empty()) {
                std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
                que.push(p);
            }
        }

        R operator*() const { return currentNode; }

        bool operator==(const nodeBreadthIterator &other) const {
            if (isEnd && other.isEnd) {
                return true;
            }
            return this == &other;
        }

        bool operator!=(const nodeBreadthIterator &other) const {
            if (isEnd && other.isEnd) {
                return false;
            }
            return this != &other;
        }

        bool isEndIter() {return isEnd;}

        // TODO: How does one handle going too far???

        // post increment gets ignored arg of 0 to distinguish from pre, A++
        nodeBreadthIterator operator++(int) {

            if (isEnd) {
                return *this;
            }

            // copy this iterator here
            nodeBreadthIterator niter = *this;

            // If gone thru the whole tree ...
            if (que.empty()) {
                isEnd = true;
                return *this;
            }

            // Look at top vector of Q
            auto &topPair = que.front();
            // iterator of vector @ current position
            auto &curIter = topPair.first;
            // end iterator of vector
            auto &endIter = topPair.second;
            // current element of vector
            auto &node = *curIter;

            // If this vector has no more nodes ...
            if (curIter - (endIter - 1) == 0) {
                que.pop();
            }

            // Prepare to look at the next node in the vector (next call)
            ++curIter;

            // If it has children, put pair of iterators on stack
            if (!node->children.empty()) {
                // Look at node's children
                std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
                que.push(p);
            }

            currentNode = node;
            // return copy of this iterator before changes
            return niter;
        }


        // pre increment, ++A
        nodeBreadthIterator operator++() {

            if (isEnd) {
                return *this;
            }

            // If gone thru the whole tree ...
            if (que.empty()) {
                isEnd = true;
                return *this;
            }

            // Look at top vector of Q
            auto &topPair = que.front();
            // iterator of vector @ current position
            auto &curIter = topPair.first;
            // end iterator of vector
            auto &endIter = topPair.second;
            // current element of vector
            auto &node = *curIter;

            // If this vector has no more nodes ...
            if (curIter - (endIter - 1) == 0) {
                que.pop();
            }

            // Prepare to look at the next node in the vector (next call)
            ++curIter;

            // If it has children, put pair of iterators on stack
            if (!node->children.empty()) {
                // Look at node's children
                std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
                que.push(p);
            }

            currentNode = node;
            return *this;
        }
    };



    /**
     * This is the base class for all evio structures: Banks, Segments, and TagSegments.
     * It is also a tree node which allows the evio structures to be represented as a tree
     * and iterated over.
     *
     * The tree code is taken from Java's <code>DefaultMutableTreeNode</code>
     * class which has been ported to C++ and included here. In this code, all nodes,
     * such as children and parent, are shared pointers. That explains inheriting from
     * enable_shared_from_this since "this" object must also be used as a shared pointer.
     *
     * @author heddle - author of original Java BaseStructure class.
     * @author timmer - add byte order tracking, make setAllHeaderLengths more efficient in Java.
     *                  Ported to C++.
     * @date 4/2/2020
     *
     */
    class BaseStructure : public std::enable_shared_from_this<BaseStructure> {

        friend class EventParser;
        friend class EvioReader;
        friend class EvioReaderV4;
        friend class EventBuilder;
        friend class StructureTransformer;

        //---------------------------------------------
        //---------- Tree structure members  ----------
        //---------------------------------------------

    public:

        // For defining our own iterator
        typedef size_t size_type;
        typedef std::ptrdiff_t difference_type;
        typedef std::input_iterator_tag iterator_category;

        typedef std::shared_ptr<BaseStructure> value_type;
        typedef std::shared_ptr<BaseStructure> reference;
        typedef std::shared_ptr<BaseStructure> pointer;

        typedef nodeIterator<std::shared_ptr<BaseStructure>> iterator;
        typedef nodeBreadthIterator<std::shared_ptr<BaseStructure>> breadth_iterator;

        friend class nodeIterator<std::shared_ptr<BaseStructure>>;
        friend class nodeBreadthIterator<std::shared_ptr<BaseStructure>>;

        // Can't figure out how to implement a const iterator, so I give up.
        //typedef nodeIterator_const<std::shared_ptr<const BaseStructure>> const_iterator;
        //typedef nodeBreadthIterator_const<std::shared_ptr<BaseStructure>> breadth_iterator_const;
        //friend class nodeIterator_const<std::shared_ptr<BaseStructure>>;
        //friend class nodeBreadthIterator_const<std::shared_ptr<const BaseStructure>>;

        /**
         * Get the beginning depth-first iterator.
         * @return beginning depth-first iterator.
         */
        iterator begin() { auto arg = getThis(); return iterator(arg, false); }
        /**
         * Get the end depth-first iterator.
         * @return end depth-first iterator.
         */
        iterator end()   { auto arg = getThis(); return iterator(arg, true); }

        /**
         * Get the beginning breadth-first iterator.
         * @return beginning breadth-first iterator.
         */
        breadth_iterator bbegin() { auto arg = getThis(); return breadth_iterator(arg, false); }
        /**
         * Get the end breadth-first iterator.
         * @return end breadth-first iterator.
         */
        breadth_iterator bend()   { auto arg = getThis(); return breadth_iterator(arg, true); }

    protected:

        /** This node's parent, or null if this node has no parent. */
        std::shared_ptr<BaseStructure> parent = nullptr;

        /** Array of children, may be null if this node has no children. */
        std::vector<std::shared_ptr<BaseStructure>> children;

        /** True if the node is able to have children. */
        bool allowsChildren = true;

    public:

        std::shared_ptr<BaseStructure> getThis() {return shared_from_this();}
        std::shared_ptr<const BaseStructure> getThisConst() const {return shared_from_this();}

    protected:

        void setParent(const std::shared_ptr<BaseStructure> &newParent);

    public:

        void insert(const std::shared_ptr<BaseStructure> newChild, size_t childIndex);
        void remove(size_t childIndex);

        std::shared_ptr<BaseStructure> getParent() const;
        std::vector<std::shared_ptr<BaseStructure>> getChildren() const;
        std::shared_ptr<BaseStructure> getChildAt(size_t index) const;

        size_t getChildCount() const;
        ssize_t getIndex(const std::shared_ptr<BaseStructure> aChild);
        std::vector<std::shared_ptr<BaseStructure>>::iterator childrenBegin();
        std::vector<std::shared_ptr<BaseStructure>>::iterator childrenEnd();

        void setAllowsChildren(bool allows);
        bool getAllowsChildren() const;

        //
        //  Derived methods
        //

        void removeFromParent();
        void remove(const std::shared_ptr<BaseStructure> aChild);
        void removeAllChildren();
        void add(std::shared_ptr<BaseStructure> newChild);

        //
        //  Tree Queries
        //

        bool isNodeAncestor(const std::shared_ptr<BaseStructure> anotherNode) const;
        bool isNodeDescendant(std::shared_ptr<BaseStructure> anotherNode);
        std::shared_ptr<BaseStructure> getSharedAncestor(std::shared_ptr<BaseStructure> aNode);
        bool isNodeRelated(std::shared_ptr<BaseStructure> aNode);
        uint32_t getDepth();
        uint32_t getLevel() const;
        std::vector<std::shared_ptr<BaseStructure>> getPath();

    protected:

        std::vector<std::shared_ptr<BaseStructure>> getPathToRoot(const std::shared_ptr<BaseStructure> & aNode, int depth) const;

    public:

        std::shared_ptr<BaseStructure> getRoot();
        bool isRoot() const;
        std::shared_ptr<BaseStructure> getNextNode();
        std::shared_ptr<BaseStructure> getPreviousNode();

        //
        //  Child Queries
        //

        bool isNodeChild(const std::shared_ptr<BaseStructure> aNode) const;
        std::shared_ptr<BaseStructure> getFirstChild() const;
        std::shared_ptr<BaseStructure> getLastChild() const;
        std::shared_ptr<BaseStructure> getChildAfter(const std::shared_ptr<BaseStructure> aChild);
        std::shared_ptr<BaseStructure> getChildBefore(const std::shared_ptr<BaseStructure> aChild);

        //
        //  Sibling Queries
        //

        bool isNodeSibling(const std::shared_ptr<BaseStructure> anotherNode) const;
        size_t getSiblingCount() const;
        std::shared_ptr<BaseStructure> getNextSibling();
        std::shared_ptr<BaseStructure> getPreviousSibling();

        //
        //  Leaf Queries
        //

        bool isLeaf() const;
        std::shared_ptr<BaseStructure> getFirstLeaf();
        std::shared_ptr<BaseStructure> getLastLeaf();
        std::shared_ptr<BaseStructure> getNextLeaf();
        std::shared_ptr<BaseStructure> getPreviousLeaf();
        ssize_t getLeafCount();

        //
        //  Tree Traversal and Searching
        //

        void visitAllStructures(std::shared_ptr<IEvioListener> listener);
        void visitAllStructures(std::shared_ptr<IEvioListener> listener,
                               std::shared_ptr<IEvioFilter> filter);
        void getMatchingStructures(std::shared_ptr<IEvioFilter> filter,
                                   std::vector<std::shared_ptr<BaseStructure>> & vec);
    private:
        void visitAllDescendants(std::shared_ptr<BaseStructure> structure,
                                 std::shared_ptr<IEvioListener> listener,
                                 std::shared_ptr<IEvioFilter> filter);



        //---------------------------------------------
        //-------- CODA evio structure members -------
        //---------------------------------------------


    protected:

        /** Holds the header of the bank as a shared pointer. */
        std::shared_ptr<BaseStructureHeader> header;

        /** The raw data of the structure. May contain padding. */
        std::vector<uint8_t> rawBytes;

        /** Used if raw data should be interpreted as shorts. */
        std::vector<int16_t> shortData;

        /** Used if raw data should be interpreted as unsigned shorts. */
        std::vector<uint16_t> ushortData;

        /** Used if raw data should be interpreted as ints. */
        std::vector<int32_t> intData;

        /** Used if raw data should be interpreted as unsigned ints. */
        std::vector<uint32_t> uintData;

        /** Used if raw data should be interpreted as longs. */
        std::vector<int64_t> longData;

        /** Used if raw data should be interpreted as unsigned longs. */
        std::vector<uint64_t> ulongData;

        /** Used if raw data should be interpreted as doubles. */
        std::vector<double> doubleData;

        /** Used if raw data should be interpreted as floats. */
        std::vector<float> floatData;

        /** Used if raw data should be interpreted as composite type. */
        std::vector<std::shared_ptr<CompositeData>> compositeData;

        /**
         * Used if raw data should be interpreted as signed chars.
         * The reason rawBytes is not used directly is because
         * it may be padded and it may not, depending on whether
         * this object was created by EvioReader or by EventBuilder, etc., etc.
         * We don't want to return rawBytes when a user calls getCharData() if there
         * are padding bytes in it.
         */
        std::vector<signed char> charData;

        /** Used if raw data should be interpreted as unsigned chars. */
        std::vector<unsigned char> ucharData;

        //------------------- STRING STUFF -------------------

        /** Used if raw data should be interpreted as a string. */
        std::vector<std::string > stringList;

        /**
         * Keep track of end of the last string added to stringData
         * (including null but not padding).
         */
        int stringEnd = 0;

        /**
         * True if char data has non-ascii or non-printable characters,
         * or has too little data to be in proper format.
         */
        bool badStringFormat = false;

        //----------------------------------------------------

        /**
         * The number of stored data items like number of banks, ints, floats, etc.
         * (not the size in ints or bytes). Some items may be padded such as shorts
         * and bytes. This will tell the meaningful number of such data items.
         * In other words, no padding is included.
         * In the case of containers, returns number of bytes not in header.
         */
        size_t numberDataItems = 0;

        /** Endianness of the raw data if appropriate. Initialize to local endian. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** Keep track of whether header length data is up-to-date or not. */
        bool lengthsUpToDate = false;

    private:

        /** Bytes with which to pad short and byte data. */
        static const uint8_t padValues[3];

        /** Number of bytes to pad short and byte data. */
        static const uint32_t padCount[4];

    protected:

        bool getLengthsUpToDate() const;
        void setLengthsUpToDate(bool lengthsUpToDate);
        uint32_t dataLength();
        void stringsToRawBytes();

    private:

        void clearData();
        void copyData(BaseStructure const & other);
        void copyData(std::shared_ptr<BaseStructure> const & other);

    protected:

        BaseStructure();
        explicit BaseStructure(std::shared_ptr<BaseStructureHeader> head);

        // TODO: are these really necesary???
        BaseStructure(const BaseStructure & srcBuf);
        BaseStructure(BaseStructure && srcBuf) noexcept;
        BaseStructure & operator=(BaseStructure && other) noexcept;
        BaseStructure & operator=(const BaseStructure & other);

    public:

        void transform(std::shared_ptr<BaseStructure> const & structure);

        virtual StructureType getStructureType() const {return StructureType::STRUCT_UNKNOWN32;};

        ByteOrder getByteOrder();

        void setByteOrder(ByteOrder const & order);

        bool needSwap() const;

        virtual std::string toString() const;
        std::string treeToString(std::string const & indent) const;

        std::shared_ptr<BaseStructureHeader> getHeader() const;

        size_t write(ByteBuffer & dest);
        size_t write(uint8_t *dest, ByteOrder const & order);

        size_t writeQuick(uint8_t *dest);
        size_t writeQuick(ByteBuffer & dest);

        uint32_t getNumberDataItems();

        uint32_t setAllHeaderLengths();
        bool isContainer() const;
        uint32_t getTotalBytes() const;

        std::vector<uint8_t> &getRawBytes();

    protected:

        void setRawBytes(uint8_t *bytes, uint32_t len);
        void setRawBytes(std::vector<uint8_t> &bytes);

    public:
        std::vector<int16_t>  &getShortData();
        std::vector<uint16_t> &getUShortData();

        std::vector<int32_t>  &getIntData();
        std::vector<uint32_t> &getUIntData();

        std::vector<int64_t>  &getLongData();
        std::vector<uint64_t> &getULongData();

        std::vector<float>  &getFloatData();
        std::vector<double> &getDoubleData();

        std::vector<signed char> & getCharData();
        std::vector<unsigned char> & getUCharData();

        std::vector<std::shared_ptr<CompositeData>> & getCompositeData();

        std::vector<std::string> & getStringData();
        uint32_t unpackRawBytesToStrings();

        static uint32_t stringToRawSize(const std::string & str);
        static uint32_t stringsToRawSize(std::vector<std::string> const & strings);

        static void stringsToRawBytes(std::vector<std::string> & strings,
                                      std::vector<uint8_t> & bytes);

        static void unpackRawBytesToStrings(std::vector<uint8_t> & bytes, size_t offset,
                                            std::vector<std::string> & strData);
        static void unpackRawBytesToStrings(std::vector<uint8_t> &  bytes,
                                            size_t offset, size_t maxLength,
                                            std::vector<std::string> & strData);
        static void unpackRawBytesToStrings(uint8_t *bytes, size_t length,
                                            std::vector<std::string> & strData);
        static void unpackRawBytesToStrings(ByteBuffer & buffer,
                                            size_t pos, size_t length,
                                            std::vector<std::string> & strData);

        void updateIntData();
        void updateUIntData();
        void updateShortData();
        void updateUShortData();
        void updateLongData();
        void updateULongData();
        void updateCharData();
        void updateUCharData();
        void updateFloatData();
        void updateDoubleData();
        void updateStringData();
        void updateCompositeData();

    private:

        static void stringBuilderToStrings(std::string const & strData, bool onlyGoodChars,
                                           std::vector<std::string> & strings);


    };


}


#endif //EVIO_6_0_BASESTRUCTURE_H
