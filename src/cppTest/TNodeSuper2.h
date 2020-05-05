//
// Created by Carl Timmer on 2020-04-16.
//

#ifndef EVIO_6_0_TNODES2_H
#define EVIO_6_0_TNODES2_H

#include <memory>
#include <vector>
#include <iostream>

#include "TNode.h"
#include "TNodeHeader.h"
#include "TNodeHeaderSuper2.h"


class TNodeSuper2 : public TNode {


public:

    TNodeSuper2(std::shared_ptr<TNodeHeaderSuper2> head, int i) : TNode(head, i) {
        std::cout << "TNodeSuper2 constructor" << std::endl;
    }

public:

    static std::shared_ptr<TNodeSuper2> getInstance(std::shared_ptr<TNodeHeaderSuper2> head, int i) {
        std::shared_ptr<TNodeSuper2> pNode(new TNodeSuper2(head, i));
        return pNode;
    }

    static std::shared_ptr<TNodeSuper2> getInstance(int tag, int num, int i) {
        std::shared_ptr<TNodeHeaderSuper2> head(new TNodeHeaderSuper2(tag, num));
        std::shared_ptr<TNodeSuper2> pNode(new TNodeSuper2(head, i));
        return pNode;
    }


public:


    virtual void myOverrideMethod() {
        std::cout << "In TNodeSuper2 myOverrideMethod" << std::endl;

    }

    void whoAmI() {
        std::cout << "In TNodeSuper2 whoAmI" << std::endl;
    }

    void sharedPtrBaseClassArg(std::shared_ptr<TNode> spn) {
        std::cout << "In TNodeSuper2 sharedPtrBaseClassArg, shared pointer count = " << spn.use_count() << std::endl;
    }


    void baseClassArg(TNode *spn) {
        std::cout << "In TNodeSuper2 baseClassArg, myInt = " << spn->getMyInt() << std::endl;
    }


    void iterateKids() {
        std::cout << "In TNodeSuper2 iterateKids:" << std::endl;
        for (auto const & child : children) {
            std::cout << "got child = " << child->getMyInt() << std::endl;
        }
    }



};


#endif //EVIO_6_0_TNODE_H
