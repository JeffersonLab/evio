//
// Created by Carl Timmer on 2020-04-26.
//

#ifndef EVIO_6_0_TNODEHEADER2_H
#define EVIO_6_0_TNODEHEADER2_H


#include "TNode.h"
#include "TNodeHeader.h"


class TNodeHeaderSuper2 : public TNodeHeader  {


    friend class TNode;

public:

    /**
      * Creates a tree node with no parent, no children, initialized with
      * the specified user object, and that allows children only if
      * specified.
      *
      * @param val specifies a user object which is copied into this object.
      * @param allows if true, the node is allowed to have child
      *        nodes -- otherwise, it is always a leaf node
      */
    TNodeHeaderSuper2() : TNodeHeader() {
        std::cout << "TNodeHeaderSuper2 default constructor" << std::endl;
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
    explicit TNodeHeaderSuper2(int tag, int num) : TNodeHeader(tag, num) {
        std::cout << "TNodeHeaderSuper2 constructor" << std::endl;
    }


public:

    static std::shared_ptr<TNodeHeaderSuper2> getInstance(int tag, int num=0) {
        std::shared_ptr<TNodeHeaderSuper2> pNode(new TNodeHeaderSuper2(tag, num));
        return pNode;
    }

    static std::shared_ptr<TNodeHeaderSuper2> getInstance() {
        std::shared_ptr<TNodeHeaderSuper2> pNode(new TNodeHeaderSuper2());
        return pNode;
    }


    void whoAmI_virt() override {
        std::cout << "TNodeHeaderSuper2 whoAmI virtual" << std::endl;
    }

    void whoAmI() {
        std::cout << "TNodeHeaderSuper2 whoAmI" << std::endl;
    }


};


#endif //EVIO_6_0_TNODEHEADER2_H
