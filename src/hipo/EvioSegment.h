//
// Created by Carl Timmer on 2020-05-15.
//

#ifndef EVIO_6_0_EVIOSEGMENT_H
#define EVIO_6_0_EVIOSEGMENT_H


#include <cstring>
#include <memory>

#include "ByteBuffer.h"
#include "DataType.h"
#include "BaseStructure.h"
#include "SegmentHeader.h"
#include "StructureType.h"


namespace evio {

    class EvioSegment : public BaseStructure {


    private:

        explicit EvioSegment(std::shared_ptr<SegmentHeader> const & head) : BaseStructure(head) {
            std::cout << "EvioSegment's private constructor" << std::endl;
        }

    public:

        static std::shared_ptr<EvioSegment> getInstance(std::shared_ptr<SegmentHeader> const & head) {
            std::shared_ptr<EvioSegment> pNode(new EvioSegment(head));
            return pNode;
        }

        static std::shared_ptr<EvioSegment> getInstance(uint16_t tag, DataType typ) {
            std::shared_ptr<SegmentHeader> head(new SegmentHeader(tag, typ));
            std::shared_ptr<EvioSegment> pNode(new EvioSegment(head));
            return pNode;
        }

        /**
         * This implements the virtual method from <code>BaseStructure</code>.
         * This returns the type of this structure, not the type of data this structure holds.
         *
         * @return the <code>StructureType</code> of this structure, which is a StructureType::STRUCT_SEGMENT.
         * @see StructureType
         */
        StructureType getStructureType() const override {
            return StructureType::STRUCT_SEGMENT;
        }

    };

}


#endif //EVIO_6_0_EVIOSEGMENT_H
