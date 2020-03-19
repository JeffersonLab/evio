//
// Created by timmer on 2/26/20.
//

#ifndef EVIO_6_0_TREENODE_H
#define EVIO_6_0_TREENODE_H

#include <memory>
#include <type_traits>
#include <iterator>
#include <stack>
#include <vector>
#include <utility>
#include "TreeNodeException.h"


namespace evio {


// Forward declare iterator classes
    template<typename R>   class nodeIterator;
    template<typename S>   class nodeIterator_const;
    template<typename T>   class nodeBreadthIterator;
    template<typename T>   class nodeBreadthIterator_const;


/**
 * <code>TreeNode</code> is a class, ported from Java's DefaultMutableTreeNode,
 * implementing a general-purpose node in a tree data structure.
 * <p>
 *
 * A tree node may have at most one parent and 0 or more children.
 * <code>TreeNode</code> provides operations for examining and modifying a
 * node's parent and children and also operations for examining the tree that
 * the node is a part of.  A node's tree is the set of all nodes that can be
 * reached by starting at the node and following all the possible links to
 * parents and children.  A node with no parent is the root of its tree; a
 * node with no children is a leaf.  A tree may consist of many subtrees,
 * each node acting as the root for its own subtree.
 * <p>
 * This class provides iterators for efficiently traversing a tree or
 * subtree in either depth-first, preorder traversal or breadth-first traversal.
 * A <code>TreeNode</code> may also hold a reference to a user object, the
 * use of which is left to the user.<p>
 * <b>This is not a thread safe class.</b>If you intend to use
 * a TreeNode (or a tree of TreeNodes) in more than one thread, you
 * need to do your own synchronizing. A good convention to adopt is
 * synchronizing on the root node of a tree.
 * <p>
 *
 *
 * @author Carl Timmer
 * @author Rob Davis
 */
    template<typename T>

    class TreeNode : public std::enable_shared_from_this<TreeNode<T>> {

    public:
        // For defining our own iterator
        typedef size_t size_type;
        typedef std::ptrdiff_t difference_type;
        typedef std::input_iterator_tag iterator_category;

        typedef std::shared_ptr<TreeNode> value_type;
        typedef std::shared_ptr<TreeNode> &reference;
        typedef std::shared_ptr<TreeNode> *pointer;

        typedef nodeIterator<std::shared_ptr<TreeNode>> iterator;
        typedef nodeIterator_const<std::shared_ptr<TreeNode>> const_iterator;
        typedef nodeBreadthIterator<std::shared_ptr<TreeNode>> breadth_iterator;
//        typedef nodeBreadthIterator_const<std::shared_ptr<const TreeNode>> breadth_iterator_const;

        friend class nodeIterator<std::shared_ptr<TreeNode>>;
        friend class nodeIterator_const<std::shared_ptr<TreeNode>>;
        friend class nodeBreadthIterator<std::shared_ptr<TreeNode>>;
//        friend class nodeBreadthIterator_const<std::shared_ptr<const TreeNode>>;

    public:

        iterator begin() { return iterator(this->shared_from_this(), false); }
        iterator end()   { return iterator(this->shared_from_this(), true); }

        iterator begin() const { return const_iterator(this->shared_from_this(), false); }
        iterator end()   const { return const_iterator(this->shared_from_this(), true); }

        breadth_iterator bbegin() { return breadth_iterator(this->shared_from_this(), false); }
        breadth_iterator bend()   { return breadth_iterator(this->shared_from_this(), true); }

//        breadth_iterator_const bbegin() const { return breadth_iterator_const(this->shared_from_this(), false); }
//        breadth_iterator_const bend()   const { return breadth_iterator_const(this->shared_from_this(), true); }



    protected:

        /** this node's parent, or null if this node has no parent */
        std::shared_ptr<TreeNode> parent = nullptr;

        /** array of children, may be null if this node has no children */
        std::vector<std::shared_ptr<TreeNode>> children;

        /** optional user object */
        std::shared_ptr<T> userObject = nullptr;

        /** true if the node is able to have children */
        bool allowsChildren = true;

    private:

        /**
         * Creates a tree node that has no parent and no children, but which
         * allows children.
         */
        TreeNode() = default;

        /**
         * Creates a tree node with no parent, no children, but which allows
         * children, and initializes it with the specified user object.
         *
         * @param userObject an Object provided by the user that constitutes
         *                   the node's data
         */
        explicit TreeNode(std::shared_ptr<T> userObject) : TreeNode(userObject, true) {}

        /**
         * Creates a tree node with no parent, no children, initialized with
         * the specified user object, and that allows children only if
         * specified.
         *
         * @param userObject an Object provided by the user that constitutes
         *        the node's data
         * @param allowsChildren if true, the node is allowed to have child
         *        nodes -- otherwise, it is always a leaf node
         */
        TreeNode(std::shared_ptr<T> userObject, bool allowsChildren) {
            this->allowsChildren = allowsChildren;
            this->userObject = userObject;
        }

    public:

        static std::shared_ptr<TreeNode> getInstance() {
            // Unfortunately, make_shared chokes on private constructors when constructing objects
            //return std::make_shared<TreeNode>(nullptr, true);
            std::shared_ptr<TreeNode> pNode(new TreeNode(nullptr, true));
            return pNode;
        }

        static std::shared_ptr<TreeNode> getInstance(std::shared_ptr<T> userObject) {
            std::shared_ptr<TreeNode> pNode(new TreeNode(userObject, true));
            return pNode;
        }

        static std::shared_ptr<TreeNode> getInstance(std::shared_ptr<T> userObject, bool allowsChildren) {
            std::shared_ptr<TreeNode> pNode(new TreeNode(userObject, allowsChildren));
            return pNode;
        }

//
//  Primitives
//

        /**
         * Removes <code>newChild</code> from its present parent (if it has a
         * parent), sets the child's parent to this node, and then adds the child
         * to this node's child array at index <code>childIndex</code>.
         * <code>newChild</code> must not be null and must not be an ancestor of
         * this node.
         *
         * @param   newChild        the TreeNode to insert under this node.
         * @param   childIndex      the index in this node's child array.
         *                          where this node is to be inserted.
         * @exception  TreeNodeException  if
         *             <code>childIndex</code> is out of bounds,
         *             <code>newChild</code> is null or is an ancestor of this node,
         *            this node does not allow children.
         * @see  #isNodeDescendant
         */
        void insert(const std::shared_ptr<TreeNode> &newChild, size_t childIndex) {
            if (!allowsChildren) {
                throw TreeNodeException("node does not allow children");
            } else if (newChild == nullptr) {
                throw TreeNodeException("new child is null");
            } else if (isNodeAncestor(newChild)) {
                throw TreeNodeException("new child is an ancestor");
            }

            auto oldParent = newChild->getParent();

            if (oldParent != nullptr) {
                oldParent->remove(newChild);
            }
            newChild->setParent(this->shared_from_this());

            children.insert(children.begin() + childIndex, newChild);
        }

        /**
         * Removes the child at the specified index from this node's children
         * and sets that node's parent to null. The child node to remove
         * must be a <code>TreeNode</code>.
         *
         * @param   childIndex      the index in this node's child array
         *                          of the child to remove
         * @exception       ArrayIndexOutOfBoundsException  if
         *                          <code>childIndex</code> is out of bounds
         */
        void remove(size_t childIndex) {
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
         * Sets this node's parent to <code>newParent</code> but does not
         * change the parent's child array.  This method is called from
         * <code>insert()</code> and <code>remove()</code> to
         * reassign a child's parent, it should not be messaged from anywhere
         * else.
         *
         * @param   newParent       this node's new parent
         */
        void setParent(const std::shared_ptr<TreeNode> &newParent) { parent = newParent; }

        /**
         * Returns this node's parent or null if this node has no parent.
         *
         * @return  this node's parent TreeNode, or null if this node has no parent
         */
        std::shared_ptr<TreeNode> getParent() { return parent; }

        /**
         * Returns the child at the specified index in this node's child array.
         *
         * @param   index   an index into this node's child array
         * @exception   TreeNodeException  if <code>index</code> is out of bounds
         * @return  the TreeNode in this node's child array at  the specified index
         */
        std::shared_ptr<TreeNode> getChildAt(size_t index) {
            if (children.size() < index + 1) {
                throw TreeNodeException("index too large");
            }
            return children[index];
        }

        /**
         * Returns the number of children of this node.
         *
         * @return  an int giving the number of children of this node
         */
        size_t getChildCount() const { return children.size(); }

        /**
         * Returns the index of the specified child in this node's child array.
         * If the specified node is not a child of this node, returns
         * <code>-1</code>.  This method performs a linear search and is O(n)
         * where n is the number of children.
         *
         * @param   aChild  the TreeNode to search for among this node's children.
         * @exception       TreeNodeException  if <code>aChild</code>  is null
         * @return  an int giving the index of the node in this node's child
         *          array, or <code>-1</code> if the specified node is a not
         *          a child of this node
         */
        ssize_t getIndex(const std::shared_ptr<TreeNode> &aChild) {
            if (aChild == nullptr) {
                throw TreeNodeException("argument is null");
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
         * Creates and returns a forward-order iterator of this node's
         * children.  Modifying this node's child array invalidates any child
         * iterators created before the modification.
         *
         * @return  an iterator of this node's children
         */
        auto childrenIter() { return children.begin(); }

        /**
         * Determines whether or not this node is allowed to have children.
         * If <code>allows</code> is false, all of this node's children are
         * removed.
         * <p>
         * Note: By default, a node allows children.
         *
         * @param   allows  true if this node is allowed to have children
         */
        void setAllowsChildren(bool allows) {
            if (allows != allowsChildren) {
                allowsChildren = allows;
                if (!allowsChildren) {
                    removeAllChildren();
                }
            }
        }

        /**
         * Returns true if this node is allowed to have children.
         * @return  true if this node allows children, else false
         */
        bool getAllowsChildren() { return allowsChildren; }

        /**
         * Sets the user object for this node to <code>userObject</code>.
         * @param   userObject      the Object that constitutes this node's
         *                          user-specified data
         * @see     #getUserObject
         */
        void setUserObject(std::shared_ptr<T> &usrObject) { this->shared_from_this()->userObject = usrObject; }

        /**
         * Returns this node's user object.
         * @return  the Object stored at this node by the user
         * @see     #setUserObject
         */
        std::shared_ptr<T> getUserObject() { return userObject; }


//
//  Derived methods
//

        /**
         * Removes the subtree rooted at this node from the tree, giving this
         * node a null parent.  Does nothing if this node is the root of its
         * tree.
         */
        void removeFromParent() {
            auto p = getParent();
            if (p != nullptr) {
// TODO: need to override remove to take this
                p.remove(this);
            }
        }

        /**
         * Removes <code>aChild</code> from this node's child array, giving it a
         * null parent.
         *
         * @param   aChild  a child of this node to remove
         * @exception  TreeNodeException if <code>aChild</code> is not a child of this node
         */
        void remove(const TreeNode *aChild) {
            if (!isNodeChild(aChild)) {
                throw TreeNodeException("argument is not a child");
            }
            remove(getIndex(aChild));       // linear search
        }

        /**
         * Removes <code>aChild</code> from this node's child array, giving it a
         * null parent.
         *
         * @param   aChild  a child of this node to remove
         * @exception  TreeNodeException if <code>aChild</code> is not a child of this node
         */
        void remove(const std::shared_ptr<TreeNode> &aChild) {
            if (!isNodeChild(aChild)) {
                throw TreeNodeException("argument is not a child");
            }
            remove(getIndex(aChild));       // linear search
        }

        /**
         * Removes all of this node's children, setting their parents to null.
         * If this node has no children, this method does nothing.
         */
        void removeAllChildren() {
            for (int i = getChildCount() - 1; i >= 0; i--) {
                remove(i);
            }
        }

        /**
         * Removes <code>newChild</code> from its parent and makes it a child of
         * this node by adding it to the end of this node's child array.
         *
         * @see             #insert
         * @param   newChild        node to add as a child of this node
         * @exception  TreeNodeException    if <code>newChild</code> is null,
         *                                  or this node does not allow children.
         */
        void add(std::shared_ptr<TreeNode<T>> &newChild) {
            if (newChild != nullptr && newChild->getParent() == this->shared_from_this())
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
         * this node.
         *
         * @see             #isNodeDescendant
         * @see             #getSharedAncestor
         * @param   anotherNode     node to test as an ancestor of this node
         * @return  true if this node is a descendant of <code>anotherNode</code>
         */
        bool isNodeAncestor(const TreeNode *anotherNode) {
            if (anotherNode == nullptr) {
                return false;
            }

            TreeNode *ancestor = this;

            do {
                if (ancestor == anotherNode) {
                    return true;
                }
            } while ((ancestor = ancestor->getParent()) != nullptr);

            return false;
        }

        /**
         * Returns true if <code>anotherNode</code> is an ancestor of this node
         * -- if it is this node, this node's parent, or an ancestor of this
         * node's parent.  (Note that a node is considered an ancestor of itself.)
         * If <code>anotherNode</code> is null, this method returns false.  This
         * operation is at worst O(h) where h is the distance from the root to
         * this node.
         *
         * @see             #isNodeDescendant
         * @see             #getSharedAncestor
         * @param   anotherNode     node to test as an ancestor of this node
         * @return  true if this node is a descendant of <code>anotherNode</code>
         */
        bool isNodeAncestor(const std::shared_ptr<TreeNode> &anotherNode) {
            if (anotherNode == nullptr) {
                return false;
            }

            auto ancestor = this->shared_from_this();

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
         * root to <code>anotherNode</code>.
         *
         * @see     #isNodeAncestor
         * @see     #getSharedAncestor
         * @param   anotherNode     node to test as descendant of this node
         * @return  true if this node is an ancestor of <code>anotherNode</code>
         */
        bool isNodeDescendant(std::shared_ptr<TreeNode> &anotherNode) {
            if (anotherNode == nullptr)
                return false;

            return anotherNode->isNodeAncestor(this->shared_from_this());
        }

        /**
         * Returns the nearest common ancestor to this node and <code>aNode</code>.
         * Returns null, if no such ancestor exists -- if this node and
         * <code>aNode</code> are in different trees or if <code>aNode</code> is
         * null.  A node is considered an ancestor of itself.
         *
         * @see     #isNodeAncestor
         * @see     #isNodeDescendant
         * @param   aNode   node to find common ancestor with
         * @throws  TreeNodeException if internal logic error.
         * @return  nearest ancestor common to this node and <code>aNode</code>,
         *          or null if none
         */
        std::shared_ptr<TreeNode> getSharedAncestor(std::shared_ptr<TreeNode> &aNode) {
            auto sharedThis = this->shared_from_this();

            if (aNode == sharedThis) {
                return sharedThis;
            } else if (aNode == nullptr) {
                return nullptr;
            }

            int level1, level2, diff;
            std::shared_ptr<TreeNode> node1, node2;

            level1 = getLevel();
            level2 = aNode->getLevel();

            if (level2 > level1) {
                diff = level2 - level1;
                node1 = aNode;
                node2 = sharedThis;
            } else {
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
                throw TreeNodeException("nodes should be null");
            }

            return nullptr;
        }


//    /**
//     * Returns the nearest common ancestor to this node and <code>aNode</code>.
//     * Returns null, if no such ancestor exists -- if this node and
//     * <code>aNode</code> are in different trees or if <code>aNode</code> is
//     * null.  A node is considered an ancestor of itself.
//     *
//     * @see     #isNodeAncestor
//     * @see     #isNodeDescendant
//     * @param   aNode   node to find common ancestor with
//     * @return  nearest ancestor common to this node and <code>aNode</code>,
//     *          or null if none
//     */
//    TreeNode getSharedAncestorOrig(TreeNode aNode) {
//        if (aNode == this) {
//            return this;
//        } else if (aNode == null) {
//            return null;
//        }
//
//        int             level1, level2, diff;
//        TreeNode        node1, node2;
//
//        level1 = getLevel();
//        level2 = aNode.getLevel();
//
//        if (level2 > level1) {
//            diff = level2 - level1;
//            node1 = aNode;
//            node2 = this;
//        } else {
//            diff = level1 - level2;
//            node1 = this;
//            node2 = aNode;
//        }
//
//        // Go up the tree until the nodes are at the same level
//        while (diff > 0) {
//            node1 = node1->getParent();
//            diff--;
//        }
//
//        // Move up the tree until we find a common ancestor.  Since we know
//        // that both nodes are at the same level, we won't cross paths
//        // unknowingly (if there is a common ancestor, both nodes hit it in
//        // the same iteration).
//
//        do {
//            if (node1 == node2) {
//                return node1;
//            }
//            node1 = node1->getParent();
//            node2 = node2->getParent();
//        } while (node1 != null);// only need to check one -- they're at the
//        // same level so if one is null, the other is
//
//        if (node1 != null || node2 != null) {
//            throw new Error ("nodes should be null");
//        }
//
//        return nullptr;
//    }
//


        /**
         * Returns true if and only if <code>aNode</code> is in the same tree
         * as this node.  Returns false if <code>aNode</code> is null.
         *
         * @see     #getSharedAncestor
         * @see     #getRoot
         * @return  true if <code>aNode</code> is in the same tree as this node;
         *          false if <code>aNode</code> is null
         */
        bool isNodeRelated(std::shared_ptr<TreeNode> &aNode) {
            return (aNode != nullptr) && (getRoot() == aNode.getRoot());
        }


        /**
         * Returns the depth of the tree rooted at this node -- the longest
         * distance from this node to a leaf.  If this node has no children,
         * returns 0.  This operation is much more expensive than
         * <code>getLevel()</code> because it must effectively traverse the entire
         * tree rooted at this node.
         *
         * @see     #getLevel
         * @throws  TreeNodeException if internal logic error.
         * @return  the depth of the tree whose root is this node
         */
        uint32_t getDepth() {
            auto iter1 = bbegin();
            auto iter2 = bend();
            auto last = iter1;

            for (; iter1 < iter2; iter1++) {
                last = iter1;
            }

            return (*last)->getLevel() - getLevel();
        }


        /**
         * Returns the number of levels above this node -- the distance from
         * the root to this node.  If this node is the root, returns 0.
         *
         * @see     #getDepth
         * @return  the number of levels above this node
         */
        uint32_t getLevel() {
            uint32_t levels = 0;

            auto ancestor = this->shared_from_this();
            while ((ancestor = ancestor->getParent()) != nullptr) {
                levels++;
            }

            return levels;
        }


        /**
          * Returns the path from the root, to get to this node.  The last
          * element in the path is this node.
          *
          * @return an array of TreeNode objects giving the path, where the
          *         first element in the path is the root and the last
          *         element is this node.
          */
        std::vector<std::shared_ptr<TreeNode>> getPath() {
            return getPathToRoot(this->shared_from_this(), 0);
        }

    protected:

        /**
         * Builds the parents of node up to and including the root node,
         * where the original node is the last element in the returned array.
         * The length of the returned array gives the node's depth in the
         * tree.
         *
         * @param aNode  the TreeNode to get the path for
         * @param depth  an int giving the number of steps already taken towards
         *        the root (on recursive calls), used to size the returned array
         * @return an array of TreeNodes giving the path from the root to the
         *         specified node
         */
        std::vector<std::shared_ptr<TreeNode>> getPathToRoot(std::shared_ptr<TreeNode> &aNode, int depth) {

            /* Check for null, in case someone passed in a null node, or
               they passed in an element that isn't rooted at root. */
            if (aNode == nullptr) {
                if (depth == 0) {
                    std::vector<std::shared_ptr<TreeNode>> retNodes;
                    return retNodes;
                } else {
                    std::vector<std::shared_ptr<TreeNode>> retNodes[depth] = {};
                    return retNodes;
                }
            }

            depth++;
            auto retNodes = getPathToRoot(aNode->getParent(), depth);
            retNodes[retNodes.length - depth] = aNode;
            return retNodes;
        }

    public:

        /**
          * Returns the user object path, from the root, to get to this node.
          * If some of the TreeNode in the path have null user objects, the
          * returned path will contain nulls.
          */
        std::vector<std::shared_ptr<T>> getUserObjectPath() {
            auto realPath = getPath();
            std::vector<std::shared_ptr<T>> retPath(realPath.length);

            for (int counter = 0; counter < realPath.length; counter++)
                retPath[counter] = realPath[counter]->getUserObject();
            return retPath;
        }


        /**
         * Returns the root of the tree that contains this node.  The root is
         * the ancestor with a null parent.
         *
         * @see     #isNodeAncestor
         * @return  the root of the tree that contains this node
         */
        std::shared_ptr<TreeNode> getRoot() {
            auto ancestor = this->shared_from_this();
            std::shared_ptr<TreeNode> previous;

            do {
                previous = ancestor;
                ancestor = ancestor->getParent();
            } while (ancestor != nullptr);

            return previous;
        }


        /**
         * Returns true if this node is the root of the tree.  The root is
         * the only node in the tree with a null parent; every tree has exactly
         * one root.
         *
         * @return  true if this node is the root of its tree
         */
        bool isRoot() { return getParent() == nullptr; }


        /**
         * Returns the node that follows this node in a preorder traversal of this
         * node's tree.  Returns null if this node is the last node of the
         * traversal.  This is an inefficient way to traverse the entire tree; use
         * an enumeration, instead.
         *
         * @see     #preorderEnumeration
         * @return  the node that follows this node in a preorder traversal, or
         *          null if this node is last
         */
        std::shared_ptr<TreeNode> getNextNode() {
            if (getChildCount() == 0) {
                // No children, so look for nextSibling
                auto nextSibling = getNextSibling();

                if (nextSibling == nullptr) {
                    auto aNode = getParent();

                    do {
                        if (aNode == nullptr) {
                            return nullptr;
                        }

                        nextSibling = aNode.getNextSibling();
                        if (nextSibling != nullptr) {
                            return nextSibling;
                        }

                        aNode = aNode.getParent();
                    } while (true);
                } else {
                    return nextSibling;
                }
            } else {
                return getChildAt(0);
            }
        }


        /**
         * Returns the node that precedes this node in a preorder traversal of
         * this node's tree.  Returns <code>null</code> if this node is the
         * first node of the traversal -- the root of the tree.
         * This is an inefficient way to
         * traverse the entire tree; use an enumeration, instead.
         *
         * @see     #preorderEnumeration
         * @return  the node that precedes this node in a preorder traversal, or
         *          null if this node is the first
         */
        std::shared_ptr<TreeNode> getPreviousNode() {
            std::shared_ptr<TreeNode> previousSibling;
            auto myParent = getParent();

            if (myParent == nullptr) {
                return nullptr;
            }

            previousSibling = getPreviousSibling();

            if (previousSibling != nullptr) {
                if (previousSibling.getChildCount() == 0)
                    return previousSibling;
                else
                    return previousSibling.getLastLeaf();
            } else {
                return myParent;
            }
        }


//
//  Child Queries
//

        /**
         * Returns true if <code>aNode</code> is a child of this node.  If
         * <code>aNode</code> is null, this method returns false.
         *
         * @return  true if <code>aNode</code> is a child of this node; false if
         *                  <code>aNode</code> is null
         */
        bool isNodeChild(const std::shared_ptr<TreeNode> &aNode) const {
            bool retval;

            if (aNode == nullptr) {
                retval = false;
            } else {
                if (getChildCount() == 0) {
                    retval = false;
                } else {
                    retval = (aNode->getParent() == this->shared_from_this());
                }
            }

            return retval;
        }


        /**
         * Returns this node's first child.  If this node has no children,
         * throws NoSuchElementException.
         *
         * @return  the first child of this node
         * @exception       TreeNodeException  if this node has no children
         */
        std::shared_ptr<TreeNode> getFirstChild() {
            if (getChildCount() == 0) {
                throw TreeNodeException("node has no children");
            }
            return getChildAt(0);
        }


        /**
         * Returns this node's last child.  If this node has no children,
         * throws NoSuchElementException.
         *
         * @return  the last child of this node
         * @exception       TreeNodeException  if this node has no children
         */
        std::shared_ptr<TreeNode> getLastChild() {
            if (getChildCount() == 0) {
                throw TreeNodeException("node has no children");
            }
            return getChildAt(getChildCount() - 1);
        }


        /**
         * Returns the child in this node's child array that immediately
         * follows <code>aChild</code>, which must be a child of this node.  If
         * <code>aChild</code> is the last child, returns null.  This method
         * performs a linear search of this node's children for
         * <code>aChild</code> and is O(n) where n is the number of children; to
         * traverse the entire array of children, use an enumeration instead.
         *
         * @see             #children
         * @throws      TreeNodeException if <code>aChild</code> is null or
         *                                    is not a child of this node.
         * @return  the child of this node that immediately follows
         *          <code>aChild</code>
         */
        std::shared_ptr<TreeNode> getChildAfter(const std::shared_ptr<TreeNode> &aChild) {
            if (aChild == nullptr) {
                throw TreeNodeException("argument is null");
            }

            ssize_t index = getIndex(aChild);           // linear search

            if (index == -1) {
                throw TreeNodeException("node is not a child");
            }

            if (index < getChildCount() - 1) {
                return getChildAt(index + 1);
            } else {
                return nullptr;
            }
        }


        /**
         * Returns the child in this node's child array that immediately
         * precedes <code>aChild</code>, which must be a child of this node.  If
         * <code>aChild</code> is the first child, returns null.  This method
         * performs a linear search of this node's children for <code>aChild</code>
         * and is O(n) where n is the number of children.
         *
         * @throws  TreeNodeException if <code>aChild</code> is null or
         *                                    is not a child of this node.
         * @return  the child of this node that immediately precedes <code>aChild</code>.
         */
        std::shared_ptr<TreeNode> getChildBefore(const std::shared_ptr<TreeNode> &aChild) {
            if (aChild == nullptr) {
                throw TreeNodeException("argument is null");
            }

            ssize_t index = getIndex(aChild);           // linear search

            if (index == -1) {
                throw TreeNodeException("argument is not a child");
            }

            if (index > 0) {
                return getChildAt(index - 1);
            } else {
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
         *
         * @param   anotherNode     node to test as sibling of this node.
         * @throws TreeNodeExceptin if sibling has different parent.
         * @return  true if <code>anotherNode</code> is a sibling of this node
         */
        bool isNodeSibling(const std::shared_ptr<TreeNode> &anotherNode) const {
            bool retval;

            if (anotherNode == nullptr) {
                retval = false;
            } else if (anotherNode == this->shared_from_this()) {
                retval = true;
            } else {
                auto myParent = getParent();
                retval = (myParent != nullptr && myParent == anotherNode->getParent());

                if (retval && !(getParent().isNodeChild(anotherNode))) {
                    throw TreeNodeException("sibling has different parent");
                }
            }

            return retval;
        }


        /**
         * Returns the number of siblings of this node.  A node is its own sibling
         * (if it has no parent or no siblings, this method returns
         * <code>1</code>).
         *
         * @return  the number of siblings of this node
         */
        size_t getSiblingCount() const {
            auto myParent = getParent();

            if (myParent == nullptr) {
                return 1;
            } else {
                return myParent.getChildCount();
            }
        }


        /**
         * Returns the next sibling of this node in the parent's children array.
         * Returns null if this node has no parent or is the parent's last child.
         * This method performs a linear search that is O(n) where n is the number
         * of children; to traverse the entire array, use the parent's child
         * enumeration instead.
         *
         * @see     #children
         * @throws  TreeNodeException if child of parent is not a sibling.
         * @return  the sibling of this node that immediately follows this node
         */
        std::shared_ptr<TreeNode> getNextSibling() {
            std::shared_ptr<TreeNode> retval;

            auto myParent = getParent();

            if (myParent == nullptr) {
                retval = nullptr;
            } else {
                retval = myParent.getChildAfter(this->shared_from_this());      // linear search
            }

            if (retval != nullptr && !isNodeSibling(retval)) {
                throw TreeNodeException("child of parent is not a sibling");
            }

            return retval;
        }


        /**
         * Returns the previous sibling of this node in the parent's children
         * array.  Returns null if this node has no parent or is the parent's
         * first child.  This method performs a linear search that is O(n) where n
         * is the number of children.
         *
         * @throws  TreeNodeException if child of parent is not a sibling.
         * @return  the sibling of this node that immediately precedes this node
         */
        std::shared_ptr<TreeNode> getPreviousSibling() {
            std::shared_ptr<TreeNode> retval;

            auto myParent = getParent();

            if (myParent == nullptr) {
                retval = nullptr;
            } else {
                retval = myParent.getChildBefore(this->shared_from_this());     // linear search
            }

            if (retval != nullptr && !isNodeSibling(retval)) {
                throw TreeNodeException("child of parent is not a sibling");
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
         * method in conjunction with <code>getAllowsChildren</code>
         *
         * @see     #getAllowsChildren
         * @return  true if this node has no children
         */
        bool isLeaf() const { return (getChildCount() == 0); }


        /**
         * Finds and returns the first leaf that is a descendant of this node --
         * either this node or its first child's first leaf.
         * Returns this node if it is a leaf.
         *
         * @see     #isLeaf
         * @see     #isNodeDescendant
         * @return  the first leaf in the subtree rooted at this node
         */
        std::shared_ptr<TreeNode> getFirstLeaf() {
            auto node = this->shared_from_this();

            while (!node.isLeaf()) {
                node = node.getFirstChild();
            }

            return node;
        }


        /**
         * Finds and returns the last leaf that is a descendant of this node --
         * either this node or its last child's last leaf.
         * Returns this node if it is a leaf.
         *
         * @see     #isLeaf
         * @see     #isNodeDescendant
         * @return  the last leaf in the subtree rooted at this node
         */
        std::shared_ptr<TreeNode> getLastLeaf() {
            auto node = this->shared_from_this();

            while (!node.isLeaf()) {
                node = node.getLastChild();
            }

            return node;
        }


        /**
         * Returns the leaf after this node or null if this node is the
         * last leaf in the tree.
         * <p>
         * In this implementation of the <code>MutableNode</code> interface,
         * this operation is very inefficient. In order to determine the
         * next node, this method first performs a linear search in the
         * parent's child-list in order to find the current node.
         * <p>
         * That implementation makes the operation suitable for short
         * traversals from a known position. But to traverse all of the
         * leaves in the tree, you should use <code>depthFirstEnumeration</code>
         * to enumerate the nodes in the tree and use <code>isLeaf</code>
         * on each node to determine which are leaves.
         *
         * @see     #depthFirstEnumeration
         * @see     #isLeaf
         * @return  returns the next leaf past this node
         */
        std::shared_ptr<TreeNode> getNextLeaf() {
            std::shared_ptr<TreeNode> nextSibling;
            auto myParent = getParent();

            if (myParent == nullptr)
                return nullptr;

            nextSibling = getNextSibling(); // linear search

            if (nextSibling != nullptr)
                return nextSibling.getFirstLeaf();

            return myParent.getNextLeaf();  // tail recursion
        }


        /**
         * Returns the leaf before this node or null if this node is the
         * first leaf in the tree.
         * <p>
         * In this implementation of the <code>MutableNode</code> interface,
         * this operation is very inefficient. In order to determine the
         * previous node, this method first performs a linear search in the
         * parent's child-list in order to find the current node.
         * <p>
         * That implementation makes the operation suitable for short
         * traversals from a known position. But to traverse all of the
         * leaves in the tree, you should use <code>depthFirstEnumeration</code>
         * to enumerate the nodes in the tree and use <code>isLeaf</code>
         * on each node to determine which are leaves.
         *
         * @see             #depthFirstEnumeration
         * @see             #isLeaf
         * @return  returns the leaf before this node
         */
        std::shared_ptr<TreeNode> getPreviousLeaf() {
            std::shared_ptr<TreeNode> previousSibling;
            auto myParent = getParent();

            if (myParent == nullptr)
                return nullptr;

            previousSibling = getPreviousSibling(); // linear search

            if (previousSibling != nullptr)
                return previousSibling.getLastLeaf();

            return myParent.getPreviousLeaf();              // tail recursion
        }


        /**
         * Returns the total number of leaves that are descendants of this node.
         * If this node is a leaf, returns <code>1</code>.  This method is O(n)
         * where n is the number of descendants of this node.
         *
         * @see     #isNodeAncestor
         * @throws  TreeNodeException if tree has zero leaves.
         * @return  the number of leaves beneath this node
         */
        ssize_t getLeafCount() const {
            ssize_t count = 0;

            auto iter1 = bbegin();
            auto iter2 = bend();

            for (; iter1 != iter2; iter1++) {
                if ((*iter1)->isLeaf()) {
                    count++;
                }
            }

            if (count < 1) {
                throw TreeNodeException("tree has zero leaves");
            }

            return count;
        }

    }; // End of class DefaultMutableTreeNode




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
            std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
            stack.push_back(p);
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

            // Look at node's children
            auto &kidIterBegin = node->children.begin();
            auto &kidIterEnd = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
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

            // Look at node's children
            auto &kidIterBegin = node->children.begin();
            auto &kidIterEnd = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
                stack.push(p);
            }

            currentNode = node;
            return *this;
        }
    };


// TODO: not sure what to make const here ........................

    template<typename S>

    class nodeIterator_const {

        // iterator of vector contained shared pointers to node's children
        typedef typename std::vector<S>::iterator KidIter;

    protected:

        // Stack of pair containing 2 iterators, each iterating over vector
        // of node's children (shared pts).
        // In each pair, first is current iterator, second is end iterator.
        std::stack<std::pair<KidIter, KidIter>> stack;

        // Where we are now in the tree
        S currentNode;

        // Is this the end iterator?
        bool isEnd;

    public:

        // Copy shared pointer arg
        explicit nodeIterator_const(S &node, bool isEnd) : currentNode(node), isEnd(isEnd) {
            // store current-element and end of vector in pair
            std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
            stack.push_back(p);
        }

        S operator*() const { return currentNode; }

        bool operator==(const nodeIterator_const &other) const {
            // Identify end iterator
            if (isEnd && other.isEnd) {
                return true;
            }
            return this == &other;
        }

        bool operator!=(const nodeIterator_const &other) const {
            // Identify end iterator
            if (isEnd && other.isEnd) {
                return false;
            }
            return this != &other;
        }

        // post increment gets ignored arg of 0 to distinguish from pre, A++
        const nodeIterator_const operator++(int) {

            if (isEnd) return *this;

            // copy this iterator here
            nodeIterator_const niter = *this;

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

            // Look at node's children
            auto &kidIterBegin = node->children.begin();
            auto &kidIterEnd = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
                stack.push(p);
            }

            currentNode = node;
            // return copy of this iterator before changes
            return niter;
        }


        // pre increment, ++A
        const nodeIterator_const &operator++() {

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

            // Look at node's children
            auto &kidIterBegin = node->children.begin();
            auto &kidIterEnd = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
                stack.push(p);
            }

            currentNode = node;
            return *this;
        }
    };

/////////////////////////////////// BREADTH FIRST ITERATOR

    template<typename T>

    class nodeBreadthIterator {

    protected:

        // iterator of vector contained shared pointers to node's children
        typedef typename std::vector<T>::iterator KidIter;

        // Stack of iterators of vector of shared pointers.
        // Each vector contains the children of a node,
        // thus each iterator gives all a node's kids.

        // Stack of iterators over node's children.
        // In each pair, first is current iterator, second is end
        std::queue<std::pair<KidIter, KidIter>> que;

        // Where we are now in the tree
        T currentNode;

        // Is this the end iterator?
        bool isEnd;

    public:

        // Copy shared pointer arg
        explicit nodeBreadthIterator(T & node, bool isEnd) : currentNode(node), isEnd(isEnd) {
            // store current-element and end of vector in pair
            std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
            que.push(p);
        }

        T operator*() const { return currentNode; }

        bool operator==(const nodeBreadthIterator &other) const { return this == &other; }

        bool operator!=(const nodeBreadthIterator &other) const { return this != &other; }

        // TODO: How does one handle going too far???

        // post increment gets ignored arg of 0 to distinguish from pre, A++
        nodeBreadthIterator operator++(int) {

            if (isEnd) return *this;

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

            // Look at node's children
            auto &kidIterBegin = node->children.begin();
            auto &kidIterEnd = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
                que.push(p);
            }

            currentNode = node;
            // return copy of this iterator before changes
            return niter;
        }


        // pre increment, ++A
        nodeBreadthIterator operator++() {

            if (isEnd) return *this;

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

            // Look at node's children
            auto kidIterBegin = node->children.begin();
            auto kidIterEnd = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
                que.push(p);
            }

            currentNode = node;
            return *this;
        }
    };

/////////////////////////////////// CONST BREADTH FIRST ITERATOR

    template<typename T>

    class nodeBreadthIterator_const {

    protected:

        // iterator of vector contained shared pointers to node's children
        typedef typename std::vector<T>::const_iterator KidIter;

        // Stack of iterators of vector of shared pointers.
        // Each vector contains the children of a node,
        // thus each iterator gives all a node's kids.

        // Stack of iterators over node's children.
        // In each pair, first is current iterator, second is end
        std::queue<std::pair<KidIter, KidIter>> que;

        // Where we are now in the tree
        const T currentNode;

        // Is this the end iterator?
        bool isEnd;

    public:

        // Copy shared pointer arg
        explicit nodeBreadthIterator_const(T const & node, bool isEnd) : currentNode(node), isEnd(isEnd) {
            // store current-element and end of vector in pair
            std::pair<KidIter, KidIter> p(node->children.begin(), node->children.end());
            que.push(p);
        }

        const T operator*() const { return currentNode; }

        bool operator==(const nodeBreadthIterator_const &other) const { return this == &other; }

        bool operator!=(const nodeBreadthIterator_const &other) const { return this != &other; }

        // TODO: How does one handle going too far???

        // post increment gets ignored arg of 0 to distinguish from pre, A++
        nodeBreadthIterator_const operator++(int) {

            if (isEnd) return *this;

            // copy this iterator here
            nodeBreadthIterator_const niter = *this;

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

            // Look at node's children
            auto kidIterBegin = node->children.begin();
            auto kidIterEnd   = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
                que.push(p);
            }

            currentNode = node;
            // return copy of this iterator before changes
            return niter;
        }


        // pre increment, ++A
        nodeBreadthIterator_const operator++() {

            if (isEnd) return *this;

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

            // Look at node's children
            auto &kidIterBegin = node->children.begin();
            auto &kidIterEnd = node->children.end();

            // If it has children, put pair of iterators on stack
            if ((kidIterEnd - 1) - kidIterBegin > 0) {
                std::pair<KidIter, KidIter> p(kidIterBegin, kidIterEnd);
                que.push(p);
            }

            currentNode = node;
            return *this;
        }
    };

}

#endif //EVIO_6_0_TREENODE_H
