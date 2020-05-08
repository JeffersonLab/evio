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


    private:

        explicit EvioBank(std::shared_ptr<BankHeader> const & head) : BaseStructure(head) {
            std::cout << "EvioBank's private constructor" << std::endl;
        }

    public:

        static std::shared_ptr<EvioBank> getInstance(std::shared_ptr<BankHeader> const & head) {
            std::shared_ptr<EvioBank> pNode(new EvioBank(head));
            return pNode;
        }

        static std::shared_ptr<EvioBank> getInstance(int tag, DataType typ, int num) {
            std::shared_ptr<BankHeader> head(new BankHeader(tag, typ, num));
            std::shared_ptr<EvioBank> pNode(new EvioBank(head));
            return pNode;
        }


        size_t write(ByteBuffer & dest) override {
            header->write(dest);
            dest.put(rawBytes.data(), rawBytes.size());
            return 0;
        }

        size_t write(uint8_t *dest, uint32_t destMaxSize, ByteOrder & order) override {
            // write the header
            header->write(dest, destMaxSize, byteOrder);
            // write the rest
            std::memcpy(dest + 4*header->getHeaderLength(), rawBytes.data(), rawBytes.size());

            return rawBytes.size() + 4*header->getHeaderLength();
        }


//    /**
//     * Get the attached object.
//     * @return the attached object
//     */
//    R getAttachment() {
//        return attachment;
//    }
//
//    /**
//     * Set the attached object.
//     * @param attachment object to attach to this bank
//     */
//    void setAttachment(R attachment) {
//        this.attachment = attachment;
//    }

        /**
         * This implements the virtual method from <code>BaseStructure</code>.
         * This returns the type of this structure, not the type of data this structure holds.
         *
         * @return the <code>StructureType</code> of this structure, which is a StructureType.BANK.
         * @see StructureType
         */
        StructureType getStructureType() override {
            return StructureType::STRUCT_BANK;
        }

    };

}
#endif //EVIO_6_0_EVIOBANK_H




