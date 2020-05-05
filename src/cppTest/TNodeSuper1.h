//
// Created by Carl Timmer on 2020-04-16.
//

#ifndef EVIO_6_0_TNODES1_H
#define EVIO_6_0_TNODES1_H

#include <memory>
#include <vector>
#include <iostream>

#include "TNode.h"
#include "TNodeHeader.h"
#include "TNodeHeaderSuper1.h"


class TNodeSuper1 : public TNode {


public:

    TNodeSuper1(std::shared_ptr<TNodeHeaderSuper1> head, int i) : TNode(head, i) {
        std::cout << "TNodeSuper1 constructor" << std::endl;
    }

public:

    static std::shared_ptr<TNodeSuper1> getInstance(std::shared_ptr<TNodeHeaderSuper1> head, int i) {
        std::shared_ptr<TNodeSuper1> pNode(new TNodeSuper1(head, i));
        return pNode;
    }

    static std::shared_ptr<TNodeSuper1> getInstance(int tag, int num, int i) {
        std::shared_ptr<TNodeHeaderSuper1> head(new TNodeHeaderSuper1(tag, num));
        std::shared_ptr<TNodeSuper1> pNode(new TNodeSuper1(head, i));
        return pNode;
    }


public:


    virtual void myOverrideMethod() {
        std::cout << "In TNodeSuper1 myOverrideMethod" << std::endl;

    }

    void whoAmI() {
        std::cout << "In TNodeSuper1 whoAmI" << std::endl;
    }

    void sharedPtrBaseClassArg(std::shared_ptr<TNode> spn) {
        std::cout << "In TNodeSuper1 sharedPtrBaseClassArg, shared pointer count = " << spn.use_count() << std::endl;
    }


    void baseClassArg(TNode *spn) {
        std::cout << "In TNodeSuper1 baseClassArg, myInt = " << spn->getMyInt() << std::endl;
    }


    void iterateKids() {
        std::cout << "In TNodeSuper1 iterateKids:" << std::endl;
        for (auto const & child : children) {
            std::cout << "got child = " << child->getMyInt() << std::endl;
        }
    }


};


#endif //EVIO_6_0_TNODE_H
