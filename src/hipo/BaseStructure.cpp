//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#include "BaseStructure.h"


namespace evio {


    /**
     * Constructor.
     * @see BaseStructureHeader
     */
    BaseStructure::BaseStructure() {
        //std::cout << "In BaseStructure default constructor" << std::endl;
        header = std::make_shared<BaseStructureHeader>();
    }


    /**
     * Constructor using a provided header.
     * @param header the header to use.
     * @see BaseStructureHeader
     */
    BaseStructure::BaseStructure(std::shared_ptr<BaseStructureHeader> head) : header(head) {
        //std::cout << "In BaseStructure head constructor" << std::endl;
    }


    /**
      * Copy constructor.
      * @param base BaseStructure to copy.
      */
    BaseStructure::BaseStructure(const BaseStructure & base) : enable_shared_from_this(base) {
        //std::cout << "In BaseStructure copy constructor" << std::endl;

        // Avoid self copy ...
        if (this != &base) {
            parent          = base.parent;
            children        = base.children;
            allowsChildren  = base.allowsChildren;
            header          = base.header;
            lengthsUpToDate = base.lengthsUpToDate;

            copyData(base);
        }
    }


    /**
     * Move constructor.
     * @param base BaseStructure to move.
     */
    BaseStructure::BaseStructure(BaseStructure && base) noexcept {
        //std::cout << "In BaseStructure move constructor" << std::endl;

        if (this != &base) {
            parent          = std::move(base.parent);
            children        = std::move(base.children);
            allowsChildren  = base.allowsChildren;
            header          = std::move(base.header);

            rawBytes        = std::move(base.rawBytes);
            shortData       = std::move(base.shortData);
            ushortData      = std::move(base.ushortData);
            intData         = std::move(base.intData);
            uintData        = std::move(base.uintData);
            longData        = std::move(base.longData);
            ulongData       = std::move(base.ulongData);
            doubleData      = std::move(base.doubleData);
            floatData       = std::move(base.floatData);
            compositeData   = std::move(base.compositeData);
            charData        = std::move(base.charData);
            ucharData       = std::move(base.ucharData);

            stringList      = std::move(base.stringList);
            stringEnd       = base.stringEnd;
            badStringFormat = base.badStringFormat;

            numberDataItems = base.numberDataItems;
            byteOrder       = base.byteOrder;
            lengthsUpToDate = base.lengthsUpToDate;
        }
    }


    /**
     * Move assignment operator.
     * @param other right side object.
     * @return left side object.
     */
    BaseStructure & BaseStructure::operator=(BaseStructure && other) noexcept {
        // Avoid self assignment ...
        if (this != &other) {
            parent          = std::move(other.parent);
            children        = std::move(other.children);
            allowsChildren  = other.allowsChildren;
            header          = std::move(other.header);

            rawBytes        = std::move(other.rawBytes);
            shortData       = std::move(other.shortData);
            ushortData      = std::move(other.ushortData);
            intData         = std::move(other.intData);
            uintData        = std::move(other.uintData);
            longData        = std::move(other.longData);
            ulongData       = std::move(other.ulongData);
            doubleData      = std::move(other.doubleData);
            floatData       = std::move(other.floatData);
            compositeData   = std::move(other.compositeData);
            charData        = std::move(other.charData);
            ucharData       = std::move(other.ucharData);

            stringList      = std::move(other.stringList);
            stringEnd       = other.stringEnd;
            badStringFormat = other.badStringFormat;

            numberDataItems = other.numberDataItems;
            byteOrder       = other.byteOrder;
            lengthsUpToDate = other.lengthsUpToDate;
        }
        return *this;
    }


    /**
     * Assignment operator.
     * @param other right side object.
     * @return left side object.
     */
    BaseStructure & BaseStructure::operator=(const BaseStructure& other) {
        // Avoid self assignment ...
        if (this != &other) {
            parent          = other.parent;
            children        = other.children;
            allowsChildren  = other.allowsChildren;
            header          = other.header;
            lengthsUpToDate = other.lengthsUpToDate;

            copyData(other);
        }
        return *this;
    }


    /**
     * This method does a partial copy and is designed to help convert
     * between banks, segments,and tagsegments in the {@link StructureTransformer}
     * class (hence the name "transform").
     * It copies all the data from another BaseStructure object.
     * Children are <b>not</b> copied in the deep clone way,
     * but their references are added to this structure.
     * It does <b>not</b> copy header data or the parent either.
     *
     * @param structure BaseStructure from which to copy data.
     */
    void BaseStructure::transform(std::shared_ptr<BaseStructure> const & structure) {
        DataType dataType = structure->getHeader()->getDataType();

        copyData(structure);
        lengthsUpToDate = structure->lengthsUpToDate;

        if (dataType.isStructure()) {
            children.clear();
            for (auto const & kid : structure->children) {
                children.push_back(kid);
            }
        }
    }


    /**
     * Copy just the data of another structure.
     * @param other structure to copy data from.
     */
    void BaseStructure::copyData(BaseStructure const & other) {
        // Copy over raw data
        rawBytes = other.rawBytes;

        // Clear out old data
        shortData.clear();
        ushortData.clear();
        intData.clear();
        uintData.clear();
        longData.clear();
        ulongData.clear();
        doubleData.clear();
        floatData.clear();
        charData.clear();
        ucharData.clear();
        compositeData.clear();
        stringList.clear();
        stringEnd = 0;
        badStringFormat = false;

        DataType const & type = other.getHeader()->getDataType();

        // Keep track of the padding and set its value in this structure's header once found.
        // This needs to be calculated since the BaseStructure arg may be a tagsegment which
        // has no associate padding data.
        // Padding is only used for the small primitive types: shorts and bytes. Strings are
        // stored in a format that takes care of its own padding and composite data is a
        // container which by definition has no padding associated with it.
        header->padding = 0;

        if (type == DataType::SHORT16) {
            shortData = other.shortData;
            if (other.shortData.size() % 2 != 0) {
                header->padding = 2;
            }
        }
        else if (type == DataType::USHORT16) {
            ushortData = other.ushortData;
            if (other.ushortData.size() % 2 != 0) {
                header->padding = 2;
            }
        }
        else if (type == DataType::INT32) {
            intData = other.intData;
        }
        else if (type == DataType::UINT32) {
            uintData = other.uintData;
        }
        else if (type == DataType::LONG64) {
            longData = other.longData;
        }
        else if (type == DataType::ULONG64) {
            ulongData = other.ulongData;
        }
        else if (type == DataType::DOUBLE64) {
            doubleData = other.doubleData;
        }
        else if (type == DataType::FLOAT32) {
            floatData = other.floatData;
        }
        else if (type == DataType::CHAR8) {
            charData = other.charData;
            header->padding = padCount[other.charData.size() % 4];
        }
        else if (type == DataType::UCHAR8) {
            ucharData = other.ucharData;
            header->padding = padCount[other.ucharData.size() % 4];
        }
        else if (type == DataType::CHARSTAR8) {
            stringList = other.stringList;
            stringEnd  = other.stringEnd;
            badStringFormat = other.badStringFormat;
        }
        else if (type == DataType::COMPOSITE) {
            // Need to copy the composite data, not just copy the shared pointers
            CompositeData::parse(rawBytes.data(), rawBytes.size(),
                                 other.byteOrder, compositeData);
        }

        numberDataItems = other.numberDataItems;
        byteOrder       = other.byteOrder;
    }


    /**
     * Copy just the data of another structure.
     * @param other structure to copy data from.
     */
    void BaseStructure::copyData(std::shared_ptr<BaseStructure> const & other) {
        // Copy over raw data
        rawBytes = other->rawBytes;

        // Clear out old data
        shortData.clear();
        ushortData.clear();
        intData.clear();
        uintData.clear();
        longData.clear();
        ulongData.clear();
        doubleData.clear();
        floatData.clear();
        charData.clear();
        ucharData.clear();
        compositeData.clear();
        stringList.clear();
        stringEnd = 0;
        badStringFormat = false;

        DataType const & type = other->getHeader()->getDataType();

        // Keep track of the padding and set its value in this structure's header once found.
        // This needs to be calculated since the BaseStructure arg may be a tagsegment which
        // has no associate padding data.
        // Padding is only used for the small primitive types: shorts and bytes. Strings are
        // stored in a format that takes care of its own padding and composite data is a
        // container which by definition has no padding associated with it.
        header->padding = 0;

        if (type == DataType::SHORT16) {
            shortData = other->shortData;
            if (other->shortData.size() % 2 != 0) {
                header->padding = 2;
            }
        }
        else if (type == DataType::USHORT16) {
            ushortData = other->ushortData;
            if (other->ushortData.size() % 2 != 0) {
                header->padding = 2;
            }
        }
        else if (type == DataType::INT32) {
            intData = other->intData;
        }
        else if (type == DataType::UINT32) {
            uintData = other->uintData;
        }
        else if (type == DataType::LONG64) {
            longData = other->longData;
        }
        else if (type == DataType::ULONG64) {
            ulongData = other->ulongData;
        }
        else if (type == DataType::DOUBLE64) {
            doubleData = other->doubleData;
        }
        else if (type == DataType::FLOAT32) {
            floatData = other->floatData;
        }
        else if (type == DataType::CHAR8) {
            charData = other->charData;
            header->padding = padCount[other->charData.size() % 4];
        }
        else if (type == DataType::UCHAR8) {
            ucharData = other->ucharData;
            header->padding = padCount[other->ucharData.size() % 4];
        }
        else if (type == DataType::CHARSTAR8) {
            stringList = other->stringList;
            stringEnd  = other->stringEnd;
            badStringFormat = other->badStringFormat;
        }
        else if (type == DataType::COMPOSITE) {
            // Need to copy the composite data, not just copy the shared pointers
            CompositeData::parse(rawBytes.data(), rawBytes.size(),
                                 other->byteOrder, compositeData);
        }

        numberDataItems = other->numberDataItems;
        byteOrder       = other->byteOrder;
    }


    //---------------------------------------------
    //-------- Tree Node structure members  -------
    //---------------------------------------------


    /**
     * Sets this node's parent to <code>newParent</code> but does not
     * change the parent's child array.  This method is called from
     * {@link #insert} and {@link #remove} to
     * reassign a child's parent, it should not be messaged from anywhere else.
     * Originally part of java's DefaultMutableBaseStructure.
     *
     * @param newParent this node's new parent.
     */
    void BaseStructure::setParent(const std::shared_ptr<BaseStructure> &newParent) {parent = newParent;}


    /**
     * Removes <code>newChild</code> from its present parent (if it has a
     * parent), sets the child's parent to this node, and then adds the child
     * to this node's child array at index <code>childIndex</code>.
     * <code>newChild</code> must not be null and must not be an ancestor of
     * this node. Originally part of java's DefaultMutableBaseStructure.
     *
     * @param   newChild        the BaseStructure to insert under this node.
     * @param   childIndex      the index in this node's child array.
     *                          where this node is to be inserted.
     * @exception  std::out_of_range  if <code>childIndex</code> is out of bounds.
     * @exception  EvioException  if <code>newChild</code> is null, is an ancestor of this node,
     *                            or this node does not allow children.
     * @see  #isNodeDescendant
     */
    void BaseStructure::insert(const std::shared_ptr<BaseStructure> newChild, size_t childIndex) {
        if (!allowsChildren) {
            throw EvioException("node does not allow children");
        }
        else if (newChild == nullptr) {
            throw EvioException("new child is null");
        }
        else if (isNodeAncestor(newChild)) {
            throw EvioException("new child is an ancestor");
        }
        else if (childIndex > children.size()) {
            throw std::out_of_range("index out of bounds");
        }

        auto oldParent = newChild->getParent();

        if (oldParent != nullptr) {
            oldParent->remove(newChild);
        }
        newChild->setParent(getThis());

        children.insert(children.begin() + childIndex, newChild);
    }


    /**
     * Removes the child at the specified index from this node's children
     * and sets that node's parent to null. The child node to remove
     * must be a <code>BaseStructure</code>. Originally part of java's DefaultMutableTreeNode.
     *
     * @param  childIndex  the index of the child to remove
     * @throws  std::out_of_range  if <code>childIndex</code> is out of bounds
     */
    void BaseStructure::remove(size_t childIndex) {
        auto child = getChildAt(childIndex);

        auto it = children.begin();
        auto end = children.end();
        size_t curIndex = 0;

        for (; it < end; it++) {
            if (curIndex == childIndex) {
                children.erase(it);
                break;
            }
            curIndex++;
        }

        child->setParent(nullptr);
    }


    /**
     * Returns this node's parent or null if this node has no parent.
     * Originally part of java's DefaultMutableTreeNode.
     * @return  this node's parent BaseStructure, or null if this node has no parent.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getParent() const { return parent; }


    /**
     * Get the children of this structure.
     * @return the children of this structure.
     */
    std::vector<std::shared_ptr<BaseStructure>> BaseStructure::getChildren() const {return children;}


    /**
     * Returns the child at the specified index in this node's child vector.
     * Originally part of java's DefaultMutableTreeNode.
     * @param   index   an index into this node's child array.
     * @throws  std::out_of_range  if <code>index</code> is out of bounds.
     * @return  the BaseStructure in this node's child array at the specified index.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getChildAt(size_t index) {
        return children.at(index);
    }


    /**
     * Returns the number of children of this node.
     * Originally part of java's DefaultMutableTreeNode.
     * @return the number of children of this node.
     */
    size_t BaseStructure::getChildCount() const { return children.size(); }


    /**
     * Returns the index of the specified child in this node's child vector.
     * If the specified node is not a child of this node, returns
     * <code>-1</code>.  This method performs a linear search and is O(n)
     * where n is the number of children.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @param   aChild  the BaseStructure to search for among this node's children.
     * @throws  EvioException  if <code>aChild</code>  is null.
     * @return  an int giving the index of the node in this node's child
     *          array, or <code>-1</code> if the specified node is a not
     *          a child of this node.
     */
    ssize_t BaseStructure::getIndex(const std::shared_ptr<BaseStructure> aChild) {
        if (aChild == nullptr) {
            throw EvioException("argument is null");
        }

        if (!isNodeChild(aChild)) {
            return -1;
        }

        auto first = children.begin();
        auto end = children.end();

        size_t index = 0;

        for (; first < end; first++) {
            if (aChild == *first) {
                return index;
            }
            index++;
        }

        return -1;
    }


    /**
     * Creates and returns a forward-order begin iterator of this node's
     * children.  Modifying this node's child array invalidates any child
     * iterators created before the modification.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @return  a begin iterator of this node's children.
     */
    std::vector<std::shared_ptr<BaseStructure>>::iterator BaseStructure::childrenBegin() { return children.begin(); }


    /**
     * Creates and returns a forward-order end iterator of this node's
     * children.  Modifying this node's child array invalidates any child
     * iterators created before the modification.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @return  an end iterator of this node's children.
     */
    std::vector<std::shared_ptr<BaseStructure>>::iterator BaseStructure::childrenEnd() { return children.end(); }


    /**
     * Determines whether or not this node is allowed to have children.
     * If <code>allows</code> is false, all of this node's children are
     * removed. Originally part of java's DefaultMutableTreeNode.
     * <p>
     * Note: By default, a node allows children.
     *
     * @param allows  true if this node is allowed to have children.
     */
    void BaseStructure::setAllowsChildren(bool allows) {
        if (allows != allowsChildren) {
            allowsChildren = allows;
            if (!allowsChildren) {
                removeAllChildren();
            }
        }
    }


    /**
     * Returns true if this node is allowed to have children.
     * Originally part of java's DefaultMutableTreeNode.
     * @return  true if this node allows children, else false.
     */
    bool BaseStructure::getAllowsChildren() const { return allowsChildren; }


    //
    //  Derived methods
    //


    /**
     * Removes the subtree rooted at this node from the tree, giving this
     * node a null parent.  Does nothing if this node is the root of its
     * tree. Originally part of java's DefaultMutableTreeNode.
     */
    void BaseStructure::removeFromParent() {
        auto p = getParent();
        if (p != nullptr) {
            p->remove(getThis());
        }
    }


    /**
     * Removes <code>aChild</code> from this node's child vector, giving it a
     * null parent. Originally part of java's DefaultMutableTreeNode.
     *
     * @param   aChild  a child of this node to remove
     * @throws  EvioException if <code>aChild</code> is not a child of this node
     */
    void BaseStructure::remove(const std::shared_ptr<BaseStructure> aChild) {
        if (!isNodeChild(aChild)) {
            throw EvioException("argument is not a child");
        }
        remove(getIndex(aChild));       // linear search
    }


    /**
     * Removes all of this node's children, setting their parents to null.
     * If this node has no children, this method does nothing.
     * Originally part of java's DefaultMutableTreeNode.
     */
    void BaseStructure::removeAllChildren() {
        for (int i = (int)getChildCount() - 1; i >= 0; i--) {
            remove(i);
        }
    }


    /**
     * Removes <code>newChild</code> from its parent and makes it a child of
     * this node by adding it to the end of this node's child array.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #insert
     * @param   newChild        node to add as a child of this node.
     * @throws  EvioException    if <code>newChild</code> is null,
     *                                  or this node does not allow children.
     */
    void BaseStructure::add(std::shared_ptr<BaseStructure> newChild) {
        if (newChild != nullptr && newChild->getParent() == getThis())
            insert(newChild, getChildCount() - 1);
        else
            insert(newChild, getChildCount());
    }


    //
    //  Tree Queries
    //


    /**
     * Returns true if <code>anotherNode</code> is an ancestor of this node
     * -- if it is this node, this node's parent, or an ancestor of this
     * node's parent.  (Note that a node is considered an ancestor of itself.)
     * If <code>anotherNode</code> is null, this method returns false.  This
     * operation is at worst O(h) where h is the distance from the root to
     * this node. Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #isNodeDescendant
     * @see     #getSharedAncestor
     * @param   anotherNode     node to test as an ancestor of this node
     * @return  true if this node is a descendant of <code>anotherNode</code>
     */
    bool BaseStructure::isNodeAncestor(const std::shared_ptr<BaseStructure> anotherNode) {
        if (anotherNode == nullptr) {
            return false;
        }

        auto ancestor = getThis();

        do {
            if (ancestor == anotherNode) {
                return true;
            }
        } while ((ancestor = ancestor->getParent()) != nullptr);

        return false;
    }


    /**
     * Returns true if <code>anotherNode</code> is a descendant of this node
     * -- if it is this node, one of this node's children, or a descendant of
     * one of this node's children.  Note that a node is considered a
     * descendant of itself.  If <code>anotherNode</code> is null, returns
     * false.  This operation is at worst O(h) where h is the distance from the
     * root to <code>anotherNode</code>. Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #isNodeAncestor
     * @see     #getSharedAncestor
     * @param   anotherNode     node to test as descendant of this node
     * @return  true if this node is an ancestor of <code>anotherNode</code>
     */
    bool BaseStructure::isNodeDescendant(std::shared_ptr<BaseStructure> anotherNode) {
        if (anotherNode == nullptr)
            return false;

        return anotherNode->isNodeAncestor(getThis());
    }


    /**
     * Returns the nearest common ancestor to this node and <code>aNode</code>.
     * Returns null if no such ancestor exists -- if this node and
     * <code>aNode</code> are in different trees or if <code>aNode</code> is
     * null.  A node is considered an ancestor of itself.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #isNodeAncestor
     * @see     #isNodeDescendant
     * @param   aNode   node to find common ancestor with.
     * @throws  EvioException if internal logic error.
     * @return  nearest ancestor common to this node and <code>aNode</code>,
     *          or null if none.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getSharedAncestor(std::shared_ptr<BaseStructure> aNode) {
        auto sharedThis = getThis();

        if (aNode == sharedThis) {
            return sharedThis;
        }
        else if (aNode == nullptr) {
            return nullptr;
        }

        uint32_t level1, level2, diff=0;
        std::shared_ptr<BaseStructure> node1, node2;

        level1 = getLevel();
        level2 = aNode->getLevel();

        if (level2 > level1) {
            diff = level2 - level1;
            node1 = aNode;
            node2 = sharedThis;
        }
        else {
            diff = level1 - level2;
            node1 = sharedThis;
            node2 = aNode;
        }

        // Go up the tree until the nodes are at the same level
        while (diff > 0) {
            node1 = node1->getParent();
            diff--;
        }

        // Move up the tree until we find a common ancestor.  Since we know
        // that both nodes are at the same level, we won't cross paths
        // unknowingly (if there is a common ancestor, both nodes hit it in
        // the same iteration).

        do {
            if (node1 == node2) {
                return node1;
            }
            node1 = node1->getParent();
            node2 = node2->getParent();
        } while (node1 != nullptr);// only need to check one -- they're at the
        // same level so if one is null, the other is

        if (node1 != nullptr || node2 != nullptr) {
            throw EvioException("nodes should be null");
        }

        return nullptr;
    }


    /**
     * Returns true if and only if <code>aNode</code> is in the same tree
     * as this node.  Returns false if <code>aNode</code> is null.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #getSharedAncestor
     * @see     #getRoot
     * @return  true if <code>aNode</code> is in the same tree as this node;
     *          false if <code>aNode</code> is null
     */
    bool BaseStructure::isNodeRelated(std::shared_ptr<BaseStructure> aNode) {
        return (aNode != nullptr) && (getRoot() == aNode->getRoot());
    }


    /**
     * Returns the depth of the tree rooted at this node -- the longest
     * distance from this node to a leaf.  If this node has no children,
     * returns 0.  This operation is much more expensive than
     * <code>getLevel()</code> because it must effectively traverse the entire
     * tree rooted at this node. Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #getLevel
     * @throws  EvioException if internal logic error.
     * @return  the depth of the tree whose root is this node.
     */
    uint32_t BaseStructure::getDepth() {
        auto iter1 = bbegin();
        auto iter2 = bend();
        auto last = iter1;

        for (; iter1 != iter2; iter1++) {
            last = iter1;
        }

        return (*last)->getLevel() - getLevel();
    }


    /**
     * Returns the number of levels above this node -- the distance from
     * the root to this node.  If this node is the root, returns 0.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #getDepth
     * @return  the number of levels above this node.
     */
    uint32_t BaseStructure::getLevel() {
        uint32_t levels = 0;

        auto ancestor = getThis();
        while ((ancestor = ancestor->getParent()) != nullptr) {
            levels++;
        }

        return levels;
    }


    /**
      * Returns the path from the root, to get to this node.  The last
      * element in the path is this node.
      * Originally part of java's DefaultMutableTreeNode.
      *
      * @return an vector of BaseStructure shared pointer giving the path, where the
      *         first element in the path is the root and the last
      *         element is this node.
      */
    std::vector<std::shared_ptr<BaseStructure>> BaseStructure::getPath() {
        return getPathToRoot(getThis(), 0);
    }


    /**
     * Builds the parents of node up to and including the root node,
     * where the original node is the last element in the returned vector.
     * The length of the returned array gives the node's depth in the
     * tree. Originally part of java's DefaultMutableTreeNode.
     *
     * @param aNode  the BaseStructure to get the path for.
     * @param depth  an int giving the number of steps already taken towards
     *        the root (on recursive calls), used to size the returned vector.
     * @return a vector of BaseStructure shared pointer giving the path from the root to the
     *         specified node.
     */
    std::vector<std::shared_ptr<BaseStructure>> BaseStructure::getPathToRoot(
            const std::shared_ptr<BaseStructure> & aNode, int depth) {

        // Check for null, in case someone passed in a null node, or
        // they passed in an element that isn't rooted at root.
        if (aNode == nullptr) {
            if (depth == 0) {
                std::vector<std::shared_ptr<BaseStructure>> retNodes;
                return retNodes;
            }
            else {
                std::vector<std::shared_ptr<BaseStructure>> retNodes;
                retNodes.reserve(depth);
                return retNodes;
            }
        }

        depth++;
        auto retNodes = getPathToRoot(aNode->getParent(), depth);
        retNodes.push_back(aNode);
        return retNodes;
    }


    /**
     * Returns the root of the tree that contains this node. The root is
     * the ancestor with a null parent. Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #isNodeAncestor
     * @return  the root of the tree that contains this node.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getRoot() {
        auto ancestor = getThis();
        std::shared_ptr<BaseStructure> previous;

        do {
            previous = ancestor;
            ancestor = ancestor->getParent();
        } while (ancestor != nullptr);

        return previous;
    }


    /**
     * Returns true if this node is the root of the tree.  The root is
     * the only node in the tree with a null parent; every tree has exactly
     * one root. Originally part of java's DefaultMutableTreeNode.
     *
     * @return  true if this node is the root of its tree.
     */
    bool BaseStructure::isRoot() const { return getParent() == nullptr; }


    /**
     * Returns the node that follows this node in a preorder traversal of this
     * node's tree (return left nodes all the way down before coming back and doing
     * siblings from bottom up). Returns null if this node is the last node of the
     * traversal.  This is an inefficient way to traverse the entire tree; use
     * an iterator instead. Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #preorderEnumeration
     * @return  the node that follows this node in a preorder traversal, or
     *          null if this node is last.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getNextNode() {
        if (getChildCount() == 0) {
            // No children, so look for nextSibling
            auto nextSibling = getNextSibling();

            if (nextSibling == nullptr) {
                auto aNode = getParent();

                do {
                    if (aNode == nullptr) {
                        return nullptr;
                    }

                    nextSibling = aNode->getNextSibling();
                    if (nextSibling != nullptr) {
                        return nextSibling;
                    }

                    aNode = aNode->getParent();
                } while (true);
            }
            else {
                return nextSibling;
            }
        }
        else {
            return getChildAt(0);
        }
    }


    /**
     * Returns the node that precedes this node in a preorder traversal of
     * this node's tree (return left nodes all the way down before coming back and doing
     * siblings from bottom up).  Returns <code>null</code> if this node is the
     * first node of the traversal -- the root of the tree.
     * This is an inefficient way to
     * traverse the entire tree; use an iterator instead.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #preorderEnumeration
     * @return  the node that precedes this node in a preorder traversal, or
     *          null if this node is the first.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getPreviousNode() {
        auto myParent = getParent();
        if (myParent == nullptr) {
            return nullptr;
        }

        auto previousSibling = getPreviousSibling();

        if (previousSibling != nullptr) {
            if (previousSibling->getChildCount() == 0)
                return previousSibling;
            else
                return previousSibling->getLastLeaf();
        }
        else {
            return myParent;
        }
    }


    //
    //  Child Queries
    //


    /**
     * Returns true if <code>aNode</code> is a child of this node.  If
     * <code>aNode</code> is null, this method returns false.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @return  true if <code>aNode</code> is a child of this node; false if
     *                  <code>aNode</code> is null.
     */
    bool BaseStructure::isNodeChild(const std::shared_ptr<BaseStructure> aNode) const {
        bool retval;

        if (aNode == nullptr) {
            retval = false;
        }
        else {
            if (getChildCount() == 0) {
                retval = false;
            }
            else {
                retval = (aNode->getParent() == getThisConst());
            }
        }

        return retval;
    }


    /**
     * Returns this node's first child.  If this node has no children,
     * throws EvioException.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @return  the first child of this node
     * @throws  EvioException  if this node has no children.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getFirstChild() {
        if (getChildCount() == 0) {
            throw EvioException("node has no children");
        }
        return getChildAt(0);
    }


    /**
     * Returns this node's last child.  If this node has no children,
     * throws EvioException.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @return  the last child of this node.
     * @exception EvioException  if this node has no children.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getLastChild() {
        if (getChildCount() == 0) {
            throw EvioException("node has no children");
        }
        return getChildAt(getChildCount() - 1);
    }


    /**
     * Returns the child in this node's child vector that immediately
     * follows <code>aChild</code>, which must be a child of this node.  If
     * <code>aChild</code> is the last child, returns null.  This method
     * performs a linear search of this node's children for
     * <code>aChild</code> and is O(n) where n is the number of children; to
     * traverse the entire vector of children, use an iterator instead.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see      #children
     * @throws   EvioException if <code>aChild</code> is null or
     *                                    is not a child of this node.
     * @return  the child of this node that immediately follows
     *          <code>aChild</code>.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getChildAfter(const std::shared_ptr<BaseStructure> aChild) {
        if (aChild == nullptr) {
            throw EvioException("argument is null");
        }

        ssize_t index = getIndex(aChild);           // linear search

        if (index == -1) {
            throw EvioException("node is not a child");
        }

        if (index < getChildCount() - 1) {
            return getChildAt(index + 1);
        }
        else {
            return nullptr;
        }
    }


    /**
     * Returns the child in this node's child vector that immediately
     * precedes <code>aChild</code>, which must be a child of this node.  If
     * <code>aChild</code> is the first child, returns null.  This method
     * performs a linear search of this node's children for <code>aChild</code>
     * and is O(n) where n is the number of children.
     *  Originally part of java's DefaultMutableTreeNode.
     *
     * @throws  EvioException if <code>aChild</code> is null or
     *                        is not a child of this node.
     * @return  the child of this node that immediately precedes <code>aChild</code>.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getChildBefore(const std::shared_ptr<BaseStructure> aChild) {
        if (aChild == nullptr) {
            throw EvioException("argument is null");
        }

        ssize_t index = getIndex(aChild);           // linear search

        if (index == -1) {
            throw EvioException("argument is not a child");
        }

        if (index > 0) {
            return getChildAt(index - 1);
        }
        else {
            return nullptr;
        }
    }


    //
    //  Sibling Queries
    //


    /**
     * Returns true if <code>anotherNode</code> is a sibling of (has the
     * same parent as) this node.  A node is its own sibling.  If
     * <code>anotherNode</code> is null, returns false.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @param   anotherNode   node to test as sibling of this node.
     * @throws  EvioException if sibling has different parent.
     * @return  true if <code>anotherNode</code> is a sibling of this node.
     */
    bool BaseStructure::isNodeSibling(const std::shared_ptr<BaseStructure> anotherNode) const {
        bool retval;

        if (anotherNode == nullptr) {
            retval = false;
        }
        else if (anotherNode == getThisConst()) {
            retval = true;
        }
        else {
            auto myParent = getParent();
            retval = (myParent != nullptr && myParent == anotherNode->getParent());

            if (retval && !(myParent->isNodeChild(anotherNode))) {
                throw EvioException("sibling has different parent");
            }
        }

        return retval;
    }


    /**
     * Returns the number of siblings of this node.  A node is its own sibling
     * (if it has no parent or no siblings, this method returns
     * <code>1</code>).
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @return  the number of siblings of this node.
     */
    size_t BaseStructure::getSiblingCount() const {
        auto myParent = getParent();

        if (myParent == nullptr) {
            return 1;
        }
        else {
            return myParent->getChildCount();
        }
    }


    /**
     * Returns the next sibling of this node in the parent's children vector.
     * Returns null if this node has no parent or is the parent's last child.
     * This method performs a linear search that is O(n) where n is the number
     * of children; to traverse the entire array, use the parent's child
     * iterator instead.
     *  Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #children.
     * @throws  EvioException if child of parent is not a sibling.
     * @return  the sibling of this node that immediately follows this node.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getNextSibling() {
        std::shared_ptr<BaseStructure> retval;

        auto myParent = getParent();

        if (myParent == nullptr) {
            retval = nullptr;
        }
        else {
            retval = myParent->getChildAfter(getThis());      // linear search
        }

        if (retval != nullptr && !isNodeSibling(retval)) {
            throw EvioException("child of parent is not a sibling");
        }

        return retval;
    }


    /**
     * Returns the previous sibling of this node in the parent's children
     * vector.  Returns null if this node has no parent or is the parent's
     * first child.  This method performs a linear search that is O(n) where n
     * is the number of children.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @throws  EvioException if child of parent is not a sibling.
     * @return  the sibling of this node that immediately precedes this node.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getPreviousSibling() {
        std::shared_ptr<BaseStructure> retval;

        auto myParent = getParent();

        if (myParent == nullptr) {
            retval = nullptr;
        }
        else {
            retval = myParent->getChildBefore(getThis());     // linear search
        }

        if (retval != nullptr && !isNodeSibling(retval)) {
            throw EvioException("child of parent is not a sibling");
        }

        return retval;
    }


    //
    //  Leaf Queries
    //


    /**
     * Returns true if this node has no children.  To distinguish between
     * nodes that have no children and nodes that <i>cannot</i> have
     * children (e.g. to distinguish files from empty directories), use this
     * method in conjunction with <code>getAllowsChildren</code>.
     * Originally part of java's DefaultMutableBaseStructure.
     *
     * @see     #getAllowsChildren
     * @return  true if this node has no children.
     */
    bool BaseStructure::isLeaf() const { return (getChildCount() == 0); }


    /**
     * Finds and returns the first leaf that is a descendant of this node --
     * either this node or its first child's first leaf.
     * Returns this node if it is a leaf.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #isLeaf
     * @see     #isNodeDescendant
     * @return  the first leaf in the subtree rooted at this node.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getFirstLeaf() {
        auto node = getThis();

        while (!node->isLeaf()) {
            node = node->getFirstChild();
        }

        return node;
    }


    /**
     * Finds and returns the last leaf that is a descendant of this node --
     * either this node or its last child's last leaf.
     * Returns this node if it is a leaf.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #isLeaf
     * @see     #isNodeDescendant
     * @return  the last leaf in the subtree rooted at this node.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getLastLeaf() {
        auto node = getThis();

        while (!node->isLeaf()) {
            node = node->getLastChild();
        }

        return node;
    }


    /**
     * Returns the leaf after this node or null if this node is the
     * last leaf in the tree.
     * <p>
     * This operation is very inefficient. In order to determine the
     * next node, this method first performs a linear search in the
     * parent's child-list in order to find the current node.
     * <p>
     * This implementation makes the operation suitable for short
     * traversals from a known position. But to traverse all of the
     * leaves in the tree, you should use A depth-first iterator
     * of the nodes in the tree and use <code>isLeaf</code>
     * on each node to determine which are leaves.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #depthFirstEnumeration
     * @see     #isLeaf
     * @return  returns the next leaf past this node.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getNextLeaf() {
        std::shared_ptr<BaseStructure> nextSibling;
        auto myParent = getParent();

        if (myParent == nullptr)
            return nullptr;

        nextSibling = getNextSibling(); // linear search

        if (nextSibling != nullptr)
            return nextSibling->getFirstLeaf();

        return myParent->getNextLeaf();  // tail recursion
    }


    /**
     * Returns the leaf before this node or null if this node is the
     * first leaf in the tree.
     * <p>
     * This operation is very inefficient. In order to determine the
     * previous node, this method first performs a linear search in the
     * parent's child-list in order to find the current node.
     * <p>
     * This implementation makes the operation suitable for short
     * traversals from a known position. But to traverse all of the
     * leaves in the tree, you should use A depth-first iterator
     * of the nodes in the tree and use <code>isLeaf</code>
     * on each node to determine which are leaves.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see             #depthFirstEnumeration
     * @see             #isLeaf
     * @return  returns the leaf before this node.
     */
    std::shared_ptr<BaseStructure> BaseStructure::getPreviousLeaf() {
        std::shared_ptr<BaseStructure> previousSibling;
        auto myParent = getParent();

        if (myParent == nullptr)
            return nullptr;

        previousSibling = getPreviousSibling(); // linear search

        if (previousSibling != nullptr)
            return previousSibling->getLastLeaf();

        return myParent->getPreviousLeaf();    // tail recursion
    }


    /**
     * Returns the total number of leaves that are descendants of this node.
     * If this node is a leaf, returns <code>1</code>.  This method is O(n)
     * where n is the number of descendants of this node.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @see     #isNodeAncestor
     * @throws  EvioException if tree has zero leaves.
     * @return  the number of leaves beneath this node.
     */
    ssize_t BaseStructure::getLeafCount() {
        ssize_t count = 0;

        auto iter1 = bbegin();
        auto iter2 = bend();

        for (; iter1 != iter2; iter1++) {
            if ((*iter1)->isLeaf()) {
                count++;
            }
        }

        if (count < 1) {
            throw EvioException("tree has zero leaves");
        }

        return count;
    }


    //
    //  Tree Traversal and Searching
    //


    /**
      * Visit all the structures in this structure (including the structure itself --
      * which is considered its own descendant).
      * This is similar to listening to the event as it is being parsed,
      * but is done to a complete (already) parsed event.
      *
      * @param listener a listener to notify as each structure is visited.
      */
    void BaseStructure::visitAllStructures(std::shared_ptr<IEvioListener> listener) {
        visitAllDescendants(getThis(), listener, nullptr);
    }


    /**
     * Visit all the structures in this structure (including the structure itself --
     * which is considered its own descendant) in a depth first manner.
     *
     * @param listener a listener to notify as each structure is visited.
     * @param filter an optional filter that must "accept" structures before
     *               they are passed to the listener. If <code>null</code>, all
     *               structures are passed. In this way, specific types of
     *               structures can be captured.
     */
    void BaseStructure::visitAllStructures(std::shared_ptr<IEvioListener> listener,
                                          std::shared_ptr<IEvioFilter> filter) {
        visitAllDescendants(getThis(), listener, filter);
    }


   /**
    * Visit all the descendants of a given structure
    * (which is considered a descendant of itself.)
    *
    * @param structure the starting structure.
    * @param listener a listener to notify as each structure is visited.
    * @param filter an optional filter that must "accept" structures before
    *               they are passed to the listener. If <code>null</code>, all
    *               structures are passed. In this way, specific types of
    *               structures can be captured.
    */
    void BaseStructure::visitAllDescendants(std::shared_ptr<BaseStructure> structure,
                                            std::shared_ptr<IEvioListener> listener,
                                            std::shared_ptr<IEvioFilter>   filter) {
        if (listener != nullptr) {
            bool accept = true;
            if (filter != nullptr) {
                accept = filter->accept(structure->getStructureType(), structure);
            }

            if (accept) {
                listener->gotStructure(getThis(), structure);
            }
        }

        if (!(structure->isLeaf())) {
            for (auto const & child : structure->getChildren()) {
                visitAllDescendants(child, listener, filter);
            }
        }
    }


    /**
     * Visit all the descendant structures, and collect those that pass a filter.
     * @param filter the filter that must be passed. If <code>null</code>,
     *               this will return all the structures.
     * @param vec    vector provided to contain all structures that are accepted by the filter.
     */
    void BaseStructure::getMatchingStructures(std::shared_ptr<IEvioFilter> filter,
                                              std::vector<std::shared_ptr<BaseStructure>> & vec) {

        class myListener : public IEvioListener {
            std::vector<std::shared_ptr<BaseStructure>> & vec;
          public:
            explicit myListener(std::vector<std::shared_ptr<BaseStructure>> & v) : vec(v) {}

            void startEventParse(std::shared_ptr<BaseStructure> structure) { }
            void endEventParse(std::shared_ptr<BaseStructure> structure) { }
            void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                              std::shared_ptr<BaseStructure> structure) {
                vec.push_back(structure);
            }
        };

        vec.clear();
        auto listener = std::make_shared<myListener>(vec);

        visitAllStructures(listener, filter);
    }


    //---------------------------------------------
    //-------- CODA evio structure elements -------
    //---------------------------------------------


    /** Clear all existing data from a non-container structure. */
    void BaseStructure::clearData() {

        if (header->getDataType().isStructure()) return;

        rawBytes.clear();
        shortData.clear();
        ushortData.clear();
        intData.clear();
        uintData.clear();
        longData.clear();
        ulongData.clear();
        doubleData.clear();
        floatData.clear();
        compositeData.clear();

        charData.clear();
        ucharData.clear();
        stringList.clear();
        stringEnd = 0;

        numberDataItems = 0;
        badStringFormat = false;
    }


    /**
     * What is the byte order of this data?
     * @return {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     */
    ByteOrder BaseStructure::getByteOrder() {return byteOrder;}


    /**
     * Set the byte order of this data. This method <b>cannot</b> be used to swap data.
     * It is only used to describe the endianness of the rawdata contained.
     * @param byteOrder {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     */
    void BaseStructure::setByteOrder(ByteOrder const & order) {byteOrder = order;}


    /**
     * Is a byte swap required (i.e. is the data endian opposite of local endian)?
     * @return <code>true</code> if byte swapping is required.
     */
    bool BaseStructure::needSwap() const {return byteOrder != ByteOrder::ENDIAN_LOCAL;}


    /**
     * Obtain a string representation of the structure.
     * @return a string representation of the structure.
     */
    std::string BaseStructure::toString() const {

        std::stringstream ss;

        // show 0x for hex
        ss << std::showbase;

        StructureType stype = getStructureType();
        DataType dtype = header->getDataType();

        std::string sb;
        sb.reserve(100);

        ss << stype.toString() << " of " << dtype.toString() << "s:  tag=" << header->getTag();
        ss << std::hex << "(" << header->getTag() << ")" << std::dec;

        if (stype == StructureType::STRUCT_BANK) {
            ss << "  num=" << ((int)(header->getNumber())) << std::hex << "(" << ((int)(header->getNumber())) << ")" << std::dec;
        }

        if (rawBytes.empty()) {
            //std::cout << "Raw Bytes is EMPTY!!!" << std::endl;
            ss << "  dataLen=" << ((header->getLength() - (header->getHeaderLength() - 1))/4);
        }
        else {
//std::cout << "Raw Bytes has size " << (rawBytes.size()/4) << std::endl;
            ss << "  dataLen=" << (rawBytes.size()/4);
        }

        if (header->getPadding() != 0) {
            ss << "  pad=" << header->getPadding();
        }

        size_t numChildren = children.size();

        if (numChildren > 0) {
            ss << "  children=" << numChildren;
        }

        return ss.str();
    }


    /**
     * Return the header for this structure.
     * @return the header for this structure.
     */
    std::shared_ptr<BaseStructureHeader> BaseStructure::getHeader() const {return header;}


    /**
     * Get the number of stored data items like number of banks, ints, floats, etc.
     * (not the size in ints or bytes). Some items may be padded such as shorts
     * and bytes. This will tell the meaningful number of such data items.
     * In the case of containers, returns number of 32-bit words not in header.
     *
     * @return number of stored data items (not size or length),
     *         or number of words if container.
     */
    uint32_t BaseStructure::getNumberDataItems() {
        if (isContainer()) {
            numberDataItems = header->getLength() + 1 - header->getHeaderLength();
        }

        // if the calculation has not already been done ...
        if (numberDataItems < 1) {
            // When parsing a file or byte array, it is not fully unpacked until data
            // is asked for specifically, for example as an int array or a float array.
            // Thus we don't know how many of a certain item (say doubles) there is.
            // But we can figure that out now based on the size of the raw data byte array.
            int divisor = 0;
            int padding = 0;
            DataType type = header->getDataType();
            int numBytes = type.getBytes();

            switch (numBytes) {
                case 2:
                    padding = header->getPadding();
                    divisor = 2; break;
                case 4:
                    divisor = 4; break;
                case 8:
                    divisor = 8; break;
                default:
                    padding = header->getPadding();
                    divisor = 1; break;
            }

            // Special cases:
            if (type == DataType::CHARSTAR8) {
                auto sd = getStringData();
                numberDataItems = sd.size();
            }
            else if (type == DataType::COMPOSITE) {
                // For this type, numberDataItems is NOT used to
                // calculate the data length so we're OK returning
                // any reasonable value here.
                numberDataItems = compositeData.size();
            }

            if (divisor > 0 && !rawBytes.empty()) {
                numberDataItems = (rawBytes.size() - padding)/divisor;
            }
        }

        return numberDataItems;
    }


    /**
     * Get the length of this structure in bytes, including the header.
     * @return the length of this structure in bytes, including the header.
     */
    uint32_t BaseStructure::getTotalBytes() const {return 4*(header->getLength() + 1);}


    /**
     * Get the raw data of the structure.
     * @return the raw data of the structure.
     */
    std::vector<uint8_t> & BaseStructure::getRawBytes() {return rawBytes;}


    /**
     * Set the data for the structure.
     * @param bytes pointer to the data to be copied.
     * @param len number of bytes to be copied.
     */
    void BaseStructure::setRawBytes(uint8_t *bytes, uint32_t len) {
        rawBytes.reserve(len);
        std::memcpy(rawBytes.data(), bytes, len);
    }


    /**
     * Set the data for the structure.
     * @param bytes vector of data to be copied.
     */
    void BaseStructure::setRawBytes(std::vector<uint8_t> & bytes) {rawBytes = bytes;}


    /**
     * Gets the raw data as an int16_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateShortData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an int16_t vector.
     * @throws EvioException if contained data type is not int16_t.
     */
    std::vector<int16_t> & BaseStructure::getShortData() {
        // If we're asking for the data type actually contained ...
        if (header->getDataType() == DataType::SHORT16) {
            // If int data has not been transformed from the raw bytes yet ...
            if (shortData.empty() && (!rawBytes.empty())) {

                // Fill int vector with transformed raw data
                auto pInt = reinterpret_cast<int16_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header->getPadding()) / sizeof(int16_t);
                shortData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    int16_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_16(dat);
                    }
                    shortData[i] = dat;
                }
            }

            return shortData;
        }

        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as an uint16_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateUShortData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an uint16_t vector.
     * @throws EvioException if contained data type is not uint16_t.
     */
    std::vector<uint16_t> & BaseStructure::getUShortData() {
        if (header->getDataType() == DataType::USHORT16) {
            if (ushortData.empty() && (!rawBytes.empty())) {

                auto pInt = reinterpret_cast<uint16_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header->getPadding()) / sizeof(uint16_t);
                ushortData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    int16_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_16(dat);
                    }
                    ushortData[i] = dat;
                }
            }
            return ushortData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as an int32_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateIntData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an int32_t vector.
     * @throws EvioException if contained data type is not int32_t.
     */
    std::vector<int32_t> & BaseStructure::getIntData() {
        if (header->getDataType() == DataType::INT32) {
            if (intData.empty() && (!rawBytes.empty())) {

                auto pInt = reinterpret_cast<int32_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header->getPadding()) / sizeof(int32_t);
                intData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    int32_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_32(dat);
                    }
                    intData[i] = dat;
                }
            }
            return intData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as an uint32_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateUIntData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an uint32_t vector.
     * @throws EvioException if contained data type is not uint32_t.
     */
    std::vector<uint32_t> & BaseStructure::getUIntData() {
        if (header->getDataType() == DataType::UINT32) {
            if (uintData.empty() && (!rawBytes.empty())) {

                auto pInt = reinterpret_cast<uint32_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header->getPadding()) / sizeof(uint32_t);
                uintData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    uint32_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_32(dat);
                    }
                    uintData[i] = dat;
                }
            }
            return uintData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as an int64_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateLongData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an int64_t vector.
     * @throws EvioException if contained data type is not int64_t.
     */
    std::vector<int64_t> & BaseStructure::getLongData() {
        if (header->getDataType() == DataType::LONG64) {
            if (longData.empty() && (!rawBytes.empty())) {

                auto pLong = reinterpret_cast<int64_t *>(rawBytes.data());
                uint32_t numLongs = (rawBytes.size() - header->getPadding()) / sizeof(int64_t);
                longData.reserve(numLongs);

                for (int i=0; i < numLongs; ++i) {
                    int64_t dat = *(pLong++);
                    if (needSwap()) {
                        dat = SWAP_64(dat);
                    }
                    longData[i] = dat;
                }
            }
            return longData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as an uint64_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateULongData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an uint64_t vector.
     * @throws EvioException if contained data type is not uint64_t.
     */
    std::vector<uint64_t> & BaseStructure::getULongData() {
        if (header->getDataType() == DataType::ULONG64) {
            if (ulongData.empty() && (!rawBytes.empty())) {

                auto pLong = reinterpret_cast<uint64_t *>(rawBytes.data());
                uint32_t numLongs = (rawBytes.size() - header->getPadding()) / sizeof(uint64_t);
                ulongData.reserve(numLongs);

                for (int i=0; i < numLongs; ++i) {
                    uint64_t dat = *(pLong++);
                    if (needSwap()) {
                        dat = SWAP_64(dat);
                    }
                    ulongData[i] = dat;
                }
            }
            return ulongData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as a float vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateFloatData} <b>MUST</b> be called!
     *
     * @return a reference to the data as a float vector.
     * @throws EvioException if contained data type is not float.
     */
    std::vector<float> & BaseStructure::getFloatData() {
        if (header->getDataType() == DataType::FLOAT32) {
            if (floatData.empty() && (!rawBytes.empty())) {

                auto pFlt = reinterpret_cast<float *>(rawBytes.data());
                uint32_t numReals = (rawBytes.size() - header->getPadding()) / sizeof(float);
                floatData.reserve(numReals);

                for (int i=0; i < numReals; ++i) {
                    float dat = *(pFlt++);
                    if (needSwap()) {
                        dat = SWAP_32(dat);
                    }
                    floatData[i] = dat;
                }
            }
            return floatData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as a double vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateDoubleData} <b>MUST</b> be called!
     *
     * @return a reference to the data as a double vector.
     * @throws EvioException if contained data type is not double.
     */
    std::vector<double> & BaseStructure::getDoubleData() {
        if (header->getDataType() == DataType::DOUBLE64) {
            if (doubleData.empty() && (!rawBytes.empty())) {

                auto pFlt = reinterpret_cast<float *>(rawBytes.data());
                uint32_t numReals = (rawBytes.size() - header->getPadding()) / sizeof(double);
                doubleData.reserve(numReals);

                for (int i=0; i < numReals; ++i) {
                    double dat = *(pFlt++);
                    if (needSwap()) {
                        dat = SWAP_64(dat);
                    }
                    doubleData[i] = dat;
                }
            }
            return doubleData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the composite data as
     * an vector of CompositeData objects, if the content type as indicated by the header
     * is appropriate.<p>
     *
     * @return the data as a vector of CompositeData objects, or <code>null</code>
     *         if this makes no sense for the given content type.
     * @throws EvioException if the data is internally inconsistent.
     */
    std::vector<std::shared_ptr<CompositeData>> & BaseStructure::getCompositeData() {

        if (header->getDataType() == DataType::COMPOSITE) {
            if (compositeData.empty() && (!rawBytes.empty())) {

                CompositeData::parse(rawBytes.data(), rawBytes.size(), byteOrder, compositeData);
            }
            return compositeData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data as an signed char vector,
     * if the contents type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateCharData} <b>MUST</b> be called!
     *
     * @return the data as an signed char vector, or <code>null</code>
     *          if this makes no sense for the given contents type.
     */
    std::vector<signed char> & BaseStructure::getCharData() {
        if (header->getDataType() == DataType::CHAR8) {
            if (charData.empty() && (!rawBytes.empty())) {

                uint32_t numBytes = (rawBytes.size() - header->getPadding());
                charData.reserve(numBytes);

                std::memcpy(reinterpret_cast<void *>(charData.data()),
                            reinterpret_cast<void *>(rawBytes.data()), numBytes);
            }
            return charData;
        }
        throw EvioException("wrong data type");
    }


    /**
     *  Gets the raw data as an unsigned char vector,
     * if the contents type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateCharData} <b>MUST</b> be called!
     *
     * @return the data as an unsigned char vector, or <code>null</code>
     *         if this makes no sense for the given contents type.
     */
    std::vector<unsigned char> & BaseStructure::getUCharData() {
        if (header->getDataType() == DataType::UCHAR8) {
            if (ucharData.empty() && (!rawBytes.empty())) {

                uint32_t numBytes = (rawBytes.size() - header->getPadding());
                ucharData.reserve(numBytes);

                std::memcpy(reinterpret_cast<void *>(ucharData.data()),
                            reinterpret_cast<void *>(rawBytes.data()), numBytes);
            }
            return ucharData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * Gets the raw data (ascii) as an
     * vector of string objects, if the contents type as indicated by the header is appropriate.
     * For any other behavior, the user should retrieve the data as a bytes and
     * manipulate it in the exact manner desired. If there are non ascii or non-printing ascii
     * chars or the bytes or not in evio format, a single string containing everything is returned.<p>
     *
     * Originally, in evio versions 1, 2 and 3, only one string was stored. Subsequent changes allowed
     * a vector of strings to be stored and retrieved. The changes are backwards compatible.
     *
     * The following is true about the string raw data format:
     * <ul>
     * <li>Each string IS immediately followed by an ending null (0).
     * <li>All string data are further padded/ended with at least one 0x4 valued ASCII
     *     char (up to 4 possible).
     * <li>The presence of 1 to 4 ending 4's distinguishes the recent string array version from
     *     the original, single string version.
     * <li>The original version string may be padded with anything after its ending null.
     * </ul>
     *
     * @return the data as a vector of string objects if DataType is CHARSTAR8, or <code>null</code>
     *         if this makes no sense for the given type.
     *
     */
    std::vector<std::string> & BaseStructure::getStringData() {
        if (header->getDataType() == DataType::CHARSTAR8) {
            if (!stringList.empty()) {
                return stringList;
            }

            if (rawBytes.empty()) {
                stringList.clear();
                return stringList;
            }

            unpackRawBytesToStrings();
            return stringList;
        }
        throw EvioException("wrong data type");
    }


    /**
     * This method returns the number of bytes in a raw
     * evio format of the given string vector, not including header.
     *
     * @param strings vector of strings to size.
     * @return the number of bytes in a raw evio format of the given strings.
     * @return 0 if vector empty.
     */
    uint32_t BaseStructure::stringsToRawSize(std::vector<std::string> const & strings) {

        if (strings.empty()) {
            return 0;
        }

        uint32_t dataLen = 0;
        for (std::string const & s : strings) {
            dataLen += s.length() + 1; // don't forget the null char after each string
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        dataLen += pads[dataLen%4];

        return dataLen;
    }


    /**
     * This method returns the number of bytes in a raw
     * evio format of the given string vector, not including header.
     *
     * @param strings vector of strings to size.
     * @return the number of bytes in a raw evio format of the given strings.
     * @return 0 if vector empty.
     */
    uint32_t BaseStructure::stringToRawSize(const std::string & str) {

        if (str.empty()) {
            return 0;
        }

        uint32_t dataLen = str.length() + 1; // don't forget the null char after each string

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        dataLen += pads[dataLen%4];

        return dataLen;
    }


    /**
     * This method transforms a vector of strings into raw evio format data,
     * not including header.
     *
     * @param strings vector of strings to transform.
     * @param bytes   vector of bytes to contain evio formatted strings.
     */
    void BaseStructure::stringsToRawBytes(std::vector<std::string> & strings,
                                          std::vector<uint8_t> & bytes) {
        if (strings.empty()) {
            bytes.clear();
            return;
        }

        // create some storage
        int dataLen = stringsToRawSize(strings);
        std::string strData;
        strData.reserve(dataLen);

        for (std::string const & s : strings) {
            // add string
            strData.append(s);
            // add ending null
            strData.append(1, '\000');
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        switch (pads[strData.length()%4]) {
            case 4:
                strData.append(4, '\004');
                break;
            case 3:
                strData.append(3, '\004');
                break;
            case 2:
                strData.append(2, '\004');
                break;
            case 1:
                strData.append(1, '\004');
        }

        // Transform to ASCII
        bytes.resize(dataLen);
        for (int i=0; i < strData.length(); i++) {
            bytes[i] = strData[i];
        }
    }


    /**
     * This method transforms the internal vector of strings into internal rawBytes vector
     * of evio format data not including header.
     */
    void BaseStructure::stringsToRawBytes() {

        if (stringList.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header->setPadding(0);
            return;
        }

        // create some storage
        int dataLen = Util::stringsToRawSize(stringList);
        std::string strData;
        strData.reserve(dataLen);

        for (std::string const & s : stringList) {
            // add string
            strData.append(s);
            // add ending null
            strData.append(1, '\000');
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        switch (pads[strData.length()%4]) {
            case 4:
                strData.append(4, '\004');
                break;
            case 3:
                strData.append(3, '\004');
                break;
            case 2:
                strData.append(2, '\004');
                break;
            case 1:
                strData.append(1, '\004');
        }

        // Transform to ASCII
        rawBytes.resize(dataLen);
        for (int i=0; i < strData.length(); i++) {
            rawBytes[i] = strData[i];
        }

        numberDataItems = stringList.size();
    }


    /**
     * This method extracts a vector of strings from bytes of raw evio string data.
     *
     * @param bytes raw evio string data.
     * @param offset offset into raw data array.
     * @param strData vector in which to place extracted strings.
     */
    void BaseStructure::unpackRawBytesToStrings(std::vector<uint8_t> & bytes, size_t offset,
                                                std::vector<std::string> & strData) {
        unpackRawBytesToStrings(bytes, offset, bytes.size(), strData);
    }


    /**
     * This method extracts a vector of strings from bytes of raw evio string data.
     * Don't go beyond the specified max character limit and stop at the first
     * non-character value.
     *
     * @param bytes       raw evio string data.
     * @param offset      offset into raw data vector.
     * @param maxLength   max length in bytes of valid data in bytes vector.
     * @param strData     vector in which to place extracted strings.
     */
    void BaseStructure::unpackRawBytesToStrings(std::vector<uint8_t> & bytes,
                                                size_t offset, size_t maxLength,
                                                std::vector<std::string> & strData) {
        size_t length = bytes.size() - offset;
        if (bytes.empty() || (length < 4)) return;

        // Don't read read more than maxLength ASCII characters
        length = length > maxLength ? maxLength : length;

        std::string sData(reinterpret_cast<const char *>(bytes.data()) + offset, length);
        return stringBuilderToStrings(sData, true, strData);
    }


    /**
     * This method extracts a vector of strings from bytes of raw evio string data.
     * Don't go beyond the specified max character limit and stop at the first
     * non-character value.
     *
     * @param bytes       raw evio string data.
     * @param length      length in bytes of valid data at bytes pointer.
     * @param strData     vector in which to place extracted strings.
     */
    void BaseStructure::unpackRawBytesToStrings(uint8_t *bytes, size_t length,
                                                std::vector<std::string> & strData) {
        if (bytes == nullptr) return;

        std::string sData(reinterpret_cast<const char *>(bytes), length);
        return stringBuilderToStrings(sData, true, strData);
    }


    /**
     * This method extracts a vector of strings from buffer containing raw evio string data.
     *
     * @param buffer  buffer containing evio string data.
     * @param pos     position of string data in buffer.
     * @param length  length of string data in buffer in bytes.
     * @param strData vector in which to place extracted strings.
     */
    void BaseStructure::unpackRawBytesToStrings(ByteBuffer & buffer,
                                                size_t pos, size_t length,
                                                std::vector<std::string> & strData) {

        if (length < 4) return;

        std::string sData(reinterpret_cast<const char *>(buffer.array() + buffer.arrayOffset()) + pos, length);
        return stringBuilderToStrings(sData, false, strData);
    }


    /**
     * This method extracts a vector of strings from a string containing evio string data.
     * If non-printable chars are found (besides those used to terminate strings),
     * then 1 string with all characters will be returned. However, if the "onlyGoodChars"
     * flag is true, 1 string is returned in truncated form without
     * the bad characters at the end.<p>
     * The name of this method is taken from the java and has little to do with C++.
     * That's done for ease of code maintenance.
     *
     * @param strData        containing string data.
     * @param onlyGoodChars  if true and non-printable chars found,
     *                       only 1 string with printable ASCII chars will be returned.
     * @param strings        vector in which to place extracted strings.
     */
    void BaseStructure::stringBuilderToStrings(std::string const & strData, bool onlyGoodChars,
                                               std::vector<std::string> & strings) {

        // Each string is terminated with a null (char val = 0)
        // and in addition, the end is padded by ASCII 4's (char val = 4).
        // However, in the legacy versions of evio, there is only one
        // null-terminated string and anything as padding. To accommodate legacy evio, if
        // there is not an ending ASCII value 4, anything past the first null is ignored.
        // After doing so, split at the nulls. Do not use the String
        // method "split" as any empty trailing strings are unfortunately discarded.

        char c;
        std::vector<int> nullIndexList(10);
        int nullCount = 0, goodChars = 0;
        bool badFormat = true;

        int length = strData.length();
        bool noEnding4 = false;
        if (strData[length - 1] != '\004') {
            noEnding4 = true;
        }

        for (int i=0; i < length; i++) {
            c = strData[i];

            // If char is a null
            if (c == 0) {
                nullCount++;
                nullIndexList.push_back(i);
                // If evio v2 or 3, only 1 null terminated string exists
                // and padding is just junk or nonexistent.
                if (noEnding4) {
                    badFormat = false;
                    break;
                }
            }
            // Look for any non-printing/control characters (not including null)
            // and end the string there. Allow tab & newline.
            else if ((c < 32 || c > 126) && c != 9 && c != 10) {
                if (nullCount < 1) {
                    badFormat = true;
                    // Getting garbage before first null.
                    break;
                }

                // Already have at least one null & therefore a String.
                // Now we have junk or non-printing ascii which is
                // possibly the ending 4.

                // If we have a 4, investigate further to see if format
                // is entirely valid.
                if (c == '\004') {
                    // How many more chars are there?
                    int charsLeft = length - (i+1);

                    // Should be no more than 3 additional 4's before the end
                    if (charsLeft > 3) {
                        badFormat = true;
                        break;
                    }
                    else {
                        // Check to see if remaining chars are all 4's. If not, bad.
                        for (int j=1; j <= charsLeft; j++) {
                            c = strData[i+j];
                            if (c != '\004') {
                                badFormat = true;
                                goto pastOuterLoop;
                            }
                        }
                        badFormat = false;
                        break;
                     }
                }
                else {
                    badFormat = true;
                    break;
                }
            }

            pastOuterLoop:

            // Number of good ASCII chars we have
            goodChars++;
        }

        strings.clear();

        if (badFormat) {
            if (onlyGoodChars) {
                // Return everything in one String WITHOUT garbage
                std::string goodStr(strData.data(), goodChars);
                strings.push_back(goodStr);
                return;
            }
            // Return everything in one String including possible garbage
            strings.push_back(strData);
            return;
        }

        // If here, raw bytes are in the proper format

        int firstIndex = 0;
        for (int nullIndex : nullIndexList) {
            std::string str(strData.data() + firstIndex, (nullIndex - firstIndex));
            strings.push_back(str);
            firstIndex = nullIndex + 1;
        }
    }


    /**
     * Extract string data from rawBytes array.
     * @return number of strings extracted from bytes.
     */
    uint32_t BaseStructure::unpackRawBytesToStrings() {

        badStringFormat = true;

        if (rawBytes.size() < 4) {
            stringList.clear();
            return 0;
        }

        // Each string is terminated with a null (char val = 0)
        // and in addition, the end is padded by ASCII 4's (char val = 4).
        // However, in the legacy versions of evio, there is only one
        // null-terminated string and anything as padding. To accommodate legacy evio, if
        // there is not an ending ASCII value 4, anything past the first null is ignored.
        // After doing so, split at the nulls.

        char c;
        int nullCount = 0;
        std::vector<int> nullIndexList(10);

        int rawLength = rawBytes.size();
        bool noEnding4 = false;
        if (rawBytes[rawLength - 1] != 4) {
            noEnding4 = true;
        }

        for (int i=0; i < rawLength; i++) {
            c = rawBytes[i];

            // If char is a null
            if (c == 0) {
                nullCount++;
                nullIndexList.push_back(i);
                // If evio v2 or 3, only 1 null terminated string exists
                // and padding is just junk or nonexistent.
                if (noEnding4) {
                    badStringFormat = false;
                    break;
                }
            }

            // Look for any non-printing/control characters (not including null)
            // and end the string there. Allow tab and newline whitespace.
            else if ((c < 32 || c > 126) && c != 9 && c != 10) {
//cout << "unpackRawBytesToStrings: found non-printing c = 0x" << hex << ((int)c) << dec << " at i = " << i << endl;
                if (nullCount < 1) {
                    // Getting garbage before first null.
//cout << "BAD FORMAT 1: garbage char before null" << endl;
                    break;
                }

                // Already have at least one null & therefore a String.
                // Now we have junk or non-printing ascii which is
                // possibly the ending 4.

                // If we have a 4, investigate further to see if format
                // is entirely valid.
                if (c == '\004') {
                    // How many more chars are there?
                    int charsLeft = rawLength - (i+1);

                    // Should be no more than 3 additional 4's before the end
                    if (charsLeft > 3) {
//std::cout << "BAD FORMAT 2: too many chars, " << charsLeft << ", after 4" << std::endl;
                        break;
                    }
                    else {
                        // Check to see if remaining chars are all 4's. If not, bad.
                        for (int j=1; j <= charsLeft; j++) {
                            c = rawBytes[i+j];
                            if (c != '\004') {
//std::cout << "BAD FORMAT 3: padding chars are not all 4's" << std::endl;
                                goto pastOuterLoop;
                            }
                        }
                        badStringFormat = false;
                        break;
                    }
                }
                else {
// std::cout << "BAD FORMAT 4: got bad char, ascii val = " << c << std::endl;
                    break;
                }
            }
        }
        pastOuterLoop:


        // What if the raw bytes are all valid ascii with no null or other non-printing chars?
        // Then format is bad so return everything as one string.

        // If error, return everything in one String including possible garbage
        if (badStringFormat) {
//cout << "unpackRawBytesToStrings: bad format, return all chars in 1 string" << endl;
            std::string everything(reinterpret_cast<char *>(rawBytes.data()), rawLength);
            stringList.push_back(everything);
            return 1;
        }

        // If here, raw bytes are in the proper format

//cout << "  split into " << nullCount << " strings" << endl;
        int firstIndex=0;
        for (int nullIndex : nullIndexList) {
            std::string subString(reinterpret_cast<char *>(rawBytes.data()) + firstIndex, (nullIndex-firstIndex));
            stringList.push_back(subString);
//cout << "    add " << subString << endl;
            firstIndex = nullIndex + 1;
        }

        // Set length of everything up to & including last null (not padding)
        stringEnd = firstIndex;
        //stringData.setLength(stringEnd);
//cout << "    good string len = " << stringEnd << endl;
        return stringList.size();
    }


    ///////////////////////////////////////////////////////////////////////


    /**
     * Checks whether this structure is a container, i.e. a structure of structures.
     * @return <code>true</code> if this structure is a container. This is the same check as
     * {@link #getAllowsChildren}.
     */
    bool BaseStructure::isContainer() const {return header->getDataType().isStructure();}


    /**
     * Compute the dataLength in 32-bit words. This is the amount of data needed
     * by a leaf of primitives. For non-leafs (structures of
     * structures) this returns 0. For data types smaller than an
     * int, e.g. a short, it computes assuming padding to an
     * integer number of ints. For example, if we are writing a byte
     * array of length 3 or 4, the it would return 1. If
     * the byte array is 5,6,7 or 8 it would return 2;
     *
     * @return the amount of 32-bit words (ints) needed by a leaf of primitives, else 0.
     */
    uint32_t BaseStructure::dataLength() {

        uint32_t datalen = 0;

        // only leafs write data
        if (isLeaf()) {

            DataType type = header->getDataType();
            int numBytes = type.getBytes();

            switch (numBytes) {
                case 8:
                    datalen = 2 * getNumberDataItems();
                    break;

                case 4:
                    datalen = getNumberDataItems();
                    break;

                case 2:
                    int items = getNumberDataItems();
                    if (items == 0) {
                        datalen = 0;
                        break;
                    }
                    datalen = 1 + (items - 1) / 2;
            }

            // Special cases:
            if (type == DataType::CHARSTAR8 || type == DataType::COMPOSITE) {
                if (!rawBytes.empty()) {
                    datalen = 1 + ((rawBytes.size() - 1) / 4);
                }
            }
            else if (type == DataType::CHAR8 || type == DataType::UCHAR8 || type == DataType::UNKNOWN32) {
                int items = getNumberDataItems();
                if (items == 0) {
                    datalen = 0;
                }
                else {
                    datalen = 1 + ((items - 1) / 4);
                }
            }
        } // isleaf

        return datalen;
    }


    /**
     * Get whether the lengths of all header fields for this structure
     * and all it descendants are up to date or not.
     *
     * @return whether the lengths of all header fields for this structure
     *         and all it descendants are up to date or not.
     */
    bool BaseStructure::getLengthsUpToDate() const {return lengthsUpToDate;}


    /**
     * Set whether the lengths of all header fields for this structure
     * and all it descendants are up to date or not.
     *
     * @param lenUpToDate are the lengths of all header fields for this structure
     *                    and all it descendants up to date or not.
     */
    void BaseStructure::setLengthsUpToDate(bool lenUpToDate) {
        lengthsUpToDate = lenUpToDate;

        // propagate back up the tree if lengths have been changed
        if (!lenUpToDate) {
            if (parent != nullptr) parent->setLengthsUpToDate(false);
        }
    }


    /**
     * Compute and set length of all header fields for this structure and all its descendants.
     * For writing events, this will be crucial for setting the values in the headers.
     *
     * @return the length that would go in the header field (for a leaf).
     * @throws EvioException if the length is too large.
     */
    uint32_t BaseStructure::setAllHeaderLengths() {
        // if length info is current, don't bother to recalculate it
        if (lengthsUpToDate) {
            return header->getLength();
        }

        uint32_t datalen, len;

        if (isLeaf()) {
            // # of 32 bit ints for leaves, 0 for empty containers (also considered leaves)
            datalen = dataLength();
        }
        else {
            datalen = 0;

            for (auto const & child : children) {
                len = child->setAllHeaderLengths();
                // Add this check to make sure structure is not being overfilled
                if (std::numeric_limits<uint32_t>::max() - datalen < len) {
                    throw EvioException("added data overflowed containing structure");
                }
                datalen += len + 1;  // + 1 for the header length word of each child
            }
        }

        len = header->getHeaderLength() - 1;  // - 1 for length header word
        if (std::numeric_limits<uint32_t>::max() - datalen < len) {
            throw EvioException("added data overflowed containing structure");
        }

        datalen += len;

        // set the datalen for the header
        header->setLength(datalen);
        setLengthsUpToDate(true);
        return datalen;
    }


   /**
    * Write myself into a ByteBuffer as evio format data
    * in the data's current byte order given by {@link #getByteOrder()}.
    * This method is much more efficient than using {@link #write(java.nio.ByteBuffer)}.<p>
    * <b>However, be warned that this method is only useful when this structure has
    * just been read from a file or buffer. Once the user adds data (and does not call
    * the appropriate update method) or children
    * to this structure, this method does NOT produce correct results.</b>
    *
    * @param dest destination ByteBuffer to contain evio format data
    *             of this bank in currently set byte order.
    */
    size_t BaseStructure::writeQuick(ByteBuffer & dest) {
        header->write(dest);
        dest.put(rawBytes.data(), rawBytes.size());
        dest.order(getByteOrder());
        return 0;
    }

    /**
     * Write myself out as evio format data
     * in the data's current byte order given by {@link #getByteOrder()} at the given pointer.
     * This method is much more efficient than using {@link #write(java.nio.ByteBuffer)}.<p>
     * <b>However, be warned that this method is only useful when this structure has
     * just been read from a file or buffer. Once the user adds data (and does not call
     * the appropriate update method) or children
     * to this structure, this method does NOT produce correct results.</b>
     *
     * @param dest pointer at which to write evio format data of this bank in currently set byte order.
     * @return byte array containing evio format data of this bank in currently set byte order.
     */
    size_t BaseStructure::writeQuick(uint8_t *dest) {
         // write the header
        header->write(dest, byteOrder);
        // write the rest
        std::memcpy(dest + 4*header->getHeaderLength(), rawBytes.data(), rawBytes.size());
        return rawBytes.size() + 4*header->getHeaderLength();
    }


    /**
     * Write myself out to a pointer.
     *
     * @param dest   pointer at which to write evio format data of this bank.
     * @param order  byte order in which to write.
     * @return the number of bytes written.
     */
    size_t BaseStructure::write(uint8_t *dest, ByteOrder const & order) {

        uint8_t *curPos = dest;

        // write the header
        header->write(curPos, order);
        curPos += 4*header->getHeaderLength();

        if (isLeaf()) {

            DataType type = header->getDataType();

            // If we have raw bytes which do NOT need swapping, this is fastest ..
            if (!rawBytes.empty() && (byteOrder == order)) {
                // write the rest
                std::memcpy(curPos, rawBytes.data(), rawBytes.size());
                curPos += rawBytes.size();
            }
            else if (type == DataType::DOUBLE64) {
                // if data sent over wire or read from file ...
                if (!rawBytes.empty()) {
                    // and need swapping ...
                    ByteOrder::byteSwap64(reinterpret_cast<uint64_t *>(rawBytes.data()),
                                          rawBytes.size()/8,
                                          reinterpret_cast<uint64_t *>(curPos));
                    curPos += rawBytes.size();
                }
                // else if user set data thru API (can't-rely-on / no rawBytes array) ...
                else {
                    ByteOrder::byteSwap64(reinterpret_cast<uint64_t *>(doubleData.data()),
                                          doubleData.size(),
                                          reinterpret_cast<uint64_t *>(curPos));
                    curPos += 8 * doubleData.size();
                }
            }
            else if (type == DataType::FLOAT32) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap32(reinterpret_cast<uint32_t *>(rawBytes.data()),
                                          rawBytes.size()/4,
                                          reinterpret_cast<uint32_t *>(curPos));
                    curPos += rawBytes.size();
                }
                else {
                    ByteOrder::byteSwap32(reinterpret_cast<uint32_t *>(floatData.data()),
                                          floatData.size(),
                                          reinterpret_cast<uint32_t *>(curPos));
                    curPos += 4 * floatData.size();
                }
            }
            else if (type == DataType::LONG64 || type == DataType::ULONG64) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap64(reinterpret_cast<uint64_t *>(rawBytes.data()),
                                          rawBytes.size()/8,
                                          reinterpret_cast<uint64_t *>(curPos));
                    curPos += rawBytes.size();
                }
                else {
                    ByteOrder::byteSwap64(reinterpret_cast<uint64_t *>(longData.data()),
                                          longData.size(),
                                          reinterpret_cast<uint64_t *>(curPos));
                    curPos += 8 * longData.size();
                }
            }
            else if (type == DataType::INT32 || type == DataType::UINT32) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap32(reinterpret_cast<uint32_t *>(rawBytes.data()),
                                          rawBytes.size()/4,
                                          reinterpret_cast<uint32_t *>(curPos));
                    curPos += rawBytes.size();
                }
                else {
                    ByteOrder::byteSwap32(reinterpret_cast<uint32_t *>(intData.data()),
                                          intData.size(),
                                          reinterpret_cast<uint32_t *>(curPos));
                    curPos += 4 * intData.size();
                }
            }
            else if (type == DataType::SHORT16 || type == DataType::USHORT16) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap16(reinterpret_cast<uint16_t *>(rawBytes.data()),
                                          rawBytes.size()/2,
                                          reinterpret_cast<uint16_t *>(curPos));
                    curPos += rawBytes.size();
                }
                else {
                    ByteOrder::byteSwap16(reinterpret_cast<uint16_t *>(shortData.data()),
                                          shortData.size(),
                                          reinterpret_cast<uint16_t *>(curPos));
                    curPos += 2 * shortData.size();

                    // might have to pad to 4 byte boundary
                    if (shortData.size() % 2 > 0) {
                        std::memcpy(curPos, padValues, 2);
                        curPos += 2;
                    }
                }
            }
            else if (type == DataType::CHAR8 || type == DataType::UCHAR8 || type == DataType::UNKNOWN32) {
                if (!rawBytes.empty()) {
                    std::memcpy(curPos, rawBytes.data(), rawBytes.size());
                    curPos += rawBytes.size();
                }
                else {
                    std::memcpy(curPos, reinterpret_cast<uint8_t*>(charData.data()), charData.size());
                    curPos += charData.size();

                    // might have to pad to 4 byte boundary
                    std::memcpy(curPos, padValues, padCount[charData.size() % 4]);
                    curPos += charData.size();
                }
            }
            else if (type == DataType::CHARSTAR8) {
                // rawbytes contains ascii, already padded
                if (!rawBytes.empty()) {
                    std::memcpy(curPos, rawBytes.data(), rawBytes.size());
                    curPos += rawBytes.size();
                }
            }
            else if (type == DataType::COMPOSITE) {
                // compositeData object always has rawBytes defined
                if (!rawBytes.empty()) {
                    // swap rawBytes
                    try {
                        CompositeData::swapAll(rawBytes.data(), curPos,
                                               rawBytes.size() / 4, byteOrder.isLocalEndian());
                        curPos += rawBytes.size();
                    }
                    catch (EvioException & e) { /* never happen */ }
                }
            }
        } // isLeaf
        else if (!children.empty()) {
            for (auto const & child : children) {
                curPos += child->write(curPos, order);
            }
        } // not leaf

        return curPos - dest;
    }


    /**
     * Write myself out into a byte buffer with fastest algorithm I could find.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written.
     * @throws overflow_error if too little space in byteBuffer.
     */
    size_t BaseStructure::write(ByteBuffer & byteBuffer) {

        if (byteBuffer.remaining() < getTotalBytes()) {
            throw std::overflow_error("byteBuffer (limit - pos) too small");
        }

        return write(byteBuffer.array() + byteBuffer.arrayOffset(), byteBuffer.order());
    }


    //----------------------------------------------------------------------
    // Methods to append to exising data if any or to set the data if none.
    //----------------------------------------------------------------------


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getIntData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateIntData() {

        // Make sure the structure is set to hold this kind of data
        DataType dataType = header->getDataType();
        if (dataType != DataType::INT32) {
            throw EvioException("cannot update int data when type = " + dataType.toString());
        }

        // if data was cleared ...
        if (intData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        // make rawBytes consistent with what's in the int vector
        else {
            numberDataItems = intData.size();
            rawBytes.resize(4 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                std::cout << "updateIntData(): swapping ..." << std::endl;
                ByteOrder::byteSwap32(reinterpret_cast<uint32_t *>(intData.data()),
                                      numberDataItems,
                                      reinterpret_cast<uint32_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), intData.data(), 4 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getUIntData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateUIntData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::UINT32) {
            throw EvioException("cannot update uint data when type = " + dataType.toString());
        }

        if (uintData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = uintData.size();
            rawBytes.resize(4 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap32(uintData.data(), numberDataItems,
                                      reinterpret_cast<uint32_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), uintData.data(), 4 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getShortData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateShortData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::SHORT16) {
            throw EvioException("cannot update short data when type = " + dataType.toString());
        }

        uint32_t pad = 0;

        if (shortData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header->setPadding(0);
        }
        else {
            numberDataItems = shortData.size();
            size_t itemBytes = 2 * numberDataItems;

            // If odd # of shorts, there are 2 bytes of padding.
            if (numberDataItems % 2 != 0) {
                pad = 2;
                rawBytes.resize(itemBytes + pad);
                // set padding bytes to val = 0
                rawBytes[itemBytes]   = 0;
                rawBytes[itemBytes+1] = 0;
            }
            else {
                rawBytes.resize(itemBytes);
            }
            header->setPadding(pad);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap16(reinterpret_cast<uint16_t *>(shortData.data()),
                                      numberDataItems,
                                      reinterpret_cast<uint16_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), shortData.data(), itemBytes);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getUShortData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateUShortData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::USHORT16) {
            throw EvioException("cannot update ushort data when type = " + dataType.toString());
        }

        uint32_t pad = 0;

        if (ushortData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header->setPadding(0);
        }
        else {
            numberDataItems = ushortData.size();
            size_t itemBytes = 2 * numberDataItems;

            if (numberDataItems % 2 != 0) {
                pad = 2;
                rawBytes.resize(itemBytes + pad);
                rawBytes[itemBytes]   = 0;
                rawBytes[itemBytes+1] = 0;
            }
            else {
                rawBytes.resize(itemBytes);
            }
            header->setPadding(pad);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap16(ushortData.data(), numberDataItems,
                                 reinterpret_cast<uint16_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), ushortData.data(), itemBytes);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getLongData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateLongData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::LONG64) {
            throw EvioException("cannot update long data when type = " + dataType.toString());
        }

        if (longData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = longData.size();
            rawBytes.resize(8 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap64(reinterpret_cast<uint64_t *>(longData.data()),
                                      numberDataItems,
                                      reinterpret_cast<uint64_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), longData.data(), 8 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getULongData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateULongData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::ULONG64) {
            throw EvioException("cannot update ulong data when type = " + dataType.toString());
        }

        if (ulongData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = ulongData.size();
            rawBytes.resize(8 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap64(ulongData.data(), numberDataItems,
                                      reinterpret_cast<uint64_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), ulongData.data(), 8 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getCharData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateCharData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::CHAR8) {
            throw EvioException("cannot update char data when type = " + dataType.toString());
        }

        if (charData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header->setPadding(0);
        }
        else {
            numberDataItems = charData.size();

            // necessary padding to 4 byte boundaries.
            uint32_t pad = padCount[numberDataItems%4];
            header->setPadding(pad);

            rawBytes.resize((numberDataItems + pad));

            std::memcpy(rawBytes.data(), charData.data(), numberDataItems);
            std::memset(rawBytes.data()+numberDataItems, 0, pad);
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getUCharData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateUCharData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::UCHAR8) {
            throw EvioException("cannot update uchar data when type = " + dataType.toString());
        }

        if (ucharData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header->setPadding(0);
        }
        else {
            numberDataItems = ucharData.size();

            uint32_t pad = padCount[numberDataItems%4];
            header->setPadding(pad);

            rawBytes.resize((numberDataItems + pad));

            std::memcpy(rawBytes.data(), ucharData.data(), numberDataItems);
            std::memset(rawBytes.data()+numberDataItems, 0, pad);
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getFloatData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateFloatData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::FLOAT32) {
            throw EvioException("cannot update float data when type = " + dataType.toString());
        }

        if (floatData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = floatData.size();
            rawBytes.resize(4 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap32(reinterpret_cast<uint32_t *>(floatData.data()),
                                      numberDataItems,
                                      reinterpret_cast<uint32_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), floatData.data(), 4 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getDoubleData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateDoubleData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::DOUBLE64) {
            throw EvioException("cannot update double data when type = " + dataType.toString());
        }

        if (doubleData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = doubleData.size();
            rawBytes.resize(8 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap64(reinterpret_cast<uint64_t *>(doubleData.data()),
                                      numberDataItems,
                                      reinterpret_cast<uint64_t *>(rawBytes.data()));
            }
            else {
                std::memcpy(rawBytes.data(), doubleData.data(), 8 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getStringData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateStringData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::CHARSTAR8) {
            throw EvioException("cannot update string data when type = " + dataType.toString());
        }

        stringsToRawBytes();

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getCompositeData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateCompositeData() {

        DataType dataType = header->getDataType();
        if (dataType != DataType::COMPOSITE) {
            throw EvioException("cannot update composite data when type = " + dataType.toString());
        }

        if (compositeData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = compositeData.size();
            rawBytes.clear();
            // Get a rough idea of the size
            size_t sz = 0;
            for (auto const & cd : compositeData) {
                sz += cd->getRawBytes().size();
            }
            rawBytes.reserve(sz);
            CompositeData::generateRawBytes(compositeData, rawBytes, byteOrder);
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

}
