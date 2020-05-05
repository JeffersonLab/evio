//
// Created by Carl Timmer on 2020-04-16.
//

#ifndef EVIO_6_0_TNODEHEADER_H
#define EVIO_6_0_TNODEHEADER_H

#include <memory>

#include "TNode.h"
#include "TNode.h"


class TNodeHeader : public std::enable_shared_from_this<TNodeHeader> {


    friend class TNode;

protected:

    int tag = 0;
    int num = 0;
    int len = 0;


    //TNodeHeader() = default;

    /**
      * Creates a tree node with no parent, no children, initialized with
      * the specified user object, and that allows children only if
      * specified.
      *
      * @param val specifies a user object which is copied into this object.
      * @param allows if true, the node is allowed to have child
      *        nodes -- otherwise, it is always a leaf node
      */
    TNodeHeader() {
        std::cout << "TNodeHeader default constructor" << std::endl;
    }

    /**
      * Creates a tree node with no parent, no children, initialized with
      * the specified user object, and that allows children only if
      * specified.
      *
      * @param val specifies a user object which is copied into this object.
      * @param allows if true, the node is allowed to have child
      *        nodes -- otherwise, it is always a leaf node
      */
    explicit TNodeHeader(int t, int n=0) : tag(t), num(n) {
        std::cout << "TNodeHeader constructor" << std::endl;
    }


public:


    int getTag() {
        std::cout << "TNodeHeader getTag()" << std::endl;
        return tag;
    }

    void setTag(int t) {
        std::cout << "TNodeHeader setTag()" << std::endl;
        tag = t;
    }

    // TODO:  Perhaps these can throw exceptions??? PRobably not.

    void setNum(int n) {
        std::cout << "TNodeHeader setNum()" << std::endl;
        num = n;
    }

    int getNum() {
        std::cout << "TNodeHeader getNum()" << std::endl;
        return num;
    }


    ////////////////////////////

    virtual void whoAmI_virt() {
        std::cout << "TNodeHeader whoAmI virtual" << std::endl;
    }

    void whoAmI() {
        std::cout << "TNodeHeader whoAmI" << std::endl;
    }


};


#endif //EVIO_6_0_TNODEHEADER_H
