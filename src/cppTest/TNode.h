//
// Created by Carl Timmer on 2020-04-16.
//

#ifndef EVIO_6_0_TNODE_H
#define EVIO_6_0_TNODE_H

#include <memory>
#include <vector>
#include <iostream>

#include "TNodeHeader.h"


class TNode : public std::enable_shared_from_this<TNode> {

protected:

    /** This node's parent, or null if this node has no parent.
     *  Originally part of java's DefaultMutableTreeNode. */
    std::shared_ptr<TNode> parent = nullptr;

    /** Array of children, may be null if this node has no children.
     *  Originally part of java's DefaultMutableTreeNode. */
    std::vector<std::shared_ptr<TNode>> children;

    int myInt;

    std::shared_ptr<TNodeHeader> header;


protected:


    TNode(std::shared_ptr<TNodeHeader> head, int i) : header(head), myInt(i) {
        std::cout << "In TNode head constructor" << std::endl;
    }


    /**
     * Sets this node's parent to <code>newParent</code> but does not
     * change the parent's child array.  This method is called from
     * {@link #insert} and {@link #remove} to
     * reassign a child's parent, it should not be messaged from anywhere
     * else.
     * Originally part of java's DefaultMutableTreeNode.
     *
     * @param newParent this node's new parent.
     */
    void setParent(const std::shared_ptr<TNode> &newParent) { parent = newParent; }

public:

    virtual void myOverrideMethod() = 0;



    std::shared_ptr<TNodeHeader> getHeader() {return header;}


    int getMyInt() const {return myInt;}
    void setMyInt(int i) {myInt = i;}

    std::shared_ptr<TNode> getParent() const { return parent; }
    std::vector<std::shared_ptr<TNode>> & getChildren() {return children;}
    size_t getChildCount() const { return children.size(); }


    void insert(const std::shared_ptr<TNode> newChild, size_t childIndex) {
        newChild->setParent(this->shared_from_this());
        children.insert(children.begin() + childIndex, newChild);
    }

    void add(std::shared_ptr<TNode> newChild) {
        if (newChild != nullptr && newChild->getParent() == this->shared_from_this())
            insert(newChild, getChildCount() - 1);
        else
            insert(newChild, getChildCount());
    }


    void whoAmI() {
        std::cout << "TNode whoAmI" << std::endl;
    }

};


#endif //EVIO_6_0_TNODE_H
