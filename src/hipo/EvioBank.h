//
// Created by timmer on 1/28/20.
//

#ifndef EVIO_6_0_EVIOBANK_H
#define EVIO_6_0_EVIOBANK_H



#include <cstring>
#include <memory>

#include "ByteBuffer.h"
#include "DataType.h"
#include "BaseStructure.h"
#include "BankHeader.h"
#include "StructureType.h"


namespace evio {

    class EvioBank : public BaseStructure {


    protected:

        explicit EvioBank(std::shared_ptr<BankHeader> const & head) : BaseStructure(head) {
            std::cout << "EvioBank's private constructor" << std::endl;
        }

    public:

        static std::shared_ptr<EvioBank> getInstance(std::shared_ptr<BankHeader> const & head) {
            std::shared_ptr<EvioBank> pNode(new EvioBank(head));
            return pNode;
        }

        static std::shared_ptr<EvioBank> getInstance(uint32_t tag, DataType typ, uint32_t num) {
            std::shared_ptr<BankHeader> head(new BankHeader(tag, typ, num));
            std::shared_ptr<EvioBank> pNode(new EvioBank(head));
            return pNode;
        }

        /**
         * This implements the virtual method from <code>BaseStructure</code>.
         * This returns the type of this structure, not the type of data this structure holds.
         *
         * @return the <code>StructureType</code> of this structure, which is a StructureType::STRUCT_BANK.
         * @see StructureType
         */
        StructureType getStructureType() override {
            return StructureType::STRUCT_BANK;
        }

    };

}
#endif //EVIO_6_0_EVIOBANK_H




