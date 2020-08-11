//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


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


    /**
     * This holds a CODA Bank structure. Mostly it has a header
     * (a <code>BankHeader</code>) and the raw data stored as a vector.
     *
     * @author heddle (original java)
     * @author timmer
     */
    class EvioBank : public BaseStructure {


    protected:

        /** Constructor. */
        EvioBank() : BaseStructure() {}

        /**
         * Constructor.
         * @param head bank header.
         */
        explicit EvioBank(std::shared_ptr<BankHeader> const & head) : BaseStructure(head) {}

    public:

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * Header is filled with default values.
         * @return shared pointer to new EvioBank.
         */
        static std::shared_ptr<EvioBank> getInstance() {
            std::shared_ptr<EvioBank> pNode(new EvioBank());
            return pNode;
        }

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param head bank header.
         * @return shared pointer to new EvioBank.
         */
        static std::shared_ptr<EvioBank> getInstance(std::shared_ptr<BankHeader> const & head) {
            std::shared_ptr<EvioBank> pNode(new EvioBank(head));
            return pNode;
        }

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param tag bank tag.
         * @param typ bank data type.
         * @param num bank num.
         * @return shared pointer to new EvioBank.
         */
        static std::shared_ptr<EvioBank> getInstance(uint16_t tag, DataType const & typ, uint8_t num) {
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
        StructureType getStructureType() const override {
            return StructureType::STRUCT_BANK;
        }

    };

}
#endif //EVIO_6_0_EVIOBANK_H




