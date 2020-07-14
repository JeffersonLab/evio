/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/05/2019
 * @author timmer
 */


#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <memory>
#include <iterator>


#include "TNode.h"
#include "TNodeSuper1.h"
#include "TNodeSuper2.h"
#include "EventBuilder.h"
#include "DataType.h"

using namespace std;




class ReadWriteTest {


public:


};



int main(int argc, char **argv) {

    uint16_t tag = 1;
    uint8_t num = 2;
    DataType type = DataType::BANK;
    EventBuilder eb(tag, type, num);

    auto h1Sup = TNodeHeaderSuper1::getInstance(222);
    auto t2Sup = TNodeSuper1::getInstance(h1Sup, 22);

    std::cout << "Get reference to header and run whoIAm's:" << std::endl;

    auto h2 = t2Sup->getHeader();
    h2->whoAmI();
    h2->whoAmI_virt();


    std::cout << "Create default header and run whoIAm's:" << std::endl;
    auto hdSup = TNodeHeaderSuper1::getInstance();

    hdSup->whoAmI();
    hdSup->whoAmI_virt();

    auto tSup2 = TNodeSuper2::getInstance(1,2,3);
    tSup2->add(t2Sup);



//    auto t3 = TNodeSuper1::getInstance(33);
//    auto t4 = TNodeSuper2::getInstance(44);

//    t2->add(t3);
//    t2->add(t4);
//
//    // Now pull t3 out of the child vector of t2 ...
//    auto t10 = t2->getChildren()[0];
//    // Now pull t4 out of the child vector of t2 ...
//    auto t11 = t2->getChildren()[1];




//    //auto t1 = TNode::getInstance(5);
//    auto t2 = TNodeSuper1::getInstance(22);
//    auto t3 = TNodeSuper1::getInstance(33);
//    auto t4 = TNodeSuper2::getInstance(44);
//
//    t2->add(t3);
//    t2->add(t4);
//
//    // Now pull t3 out of the child vector of t2 ...
//    auto t10 = t2->getChildren()[0];
//    // Now pull t4 out of the child vector of t2 ...
//    auto t11 = t2->getChildren()[1];
//
//    std::cout << "main: call t10's myOverrideMethod " << endl;
//    t10->myOverrideMethod();
//    t10->whoAmI();
//    std::cout << "main: t3's use count = " << t3.use_count() << endl;
//    {
//        auto t5 = t10->shared_from_this();
//        std::shared_ptr<TNodeSuper1> t6 = static_pointer_cast<TNodeSuper1>(t10->shared_from_this());
//        std::cout << "main: t3's use count after copy of 2x t10->shared_from_this() = " << t3.use_count() << endl;
//
//        t6->sharedPtrBaseClassArg(t5);
//        t6->baseClassArg(t5.get());
//        std::cout << "main: t2 iterate over kids:" << endl;
//        t2->iterateKids();
//
//        std::shared_ptr<TNode> t7 = t6->shared_from_this();
//        std::cout << "main: t3's use count after calling t6->shared_from_this() = " << t3.use_count() << endl;
//    }
//    std::cout << "main: t3's use count after copyies out-of-scope = " << t3.use_count() << endl;
//
//
//    std::cout << "main: call t11's myOverrideMethod " << endl;
//    t11->myOverrideMethod();

}
