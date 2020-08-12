//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


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


    /**
     * This holds a CODA Segment structure. Mostly it has a header (a <code>SegementHeader</code>)
     * and the raw data stored as a vector.
     *
     * @author heddle
     * @author timmer
     */
    class EvioSegment : public BaseStructure {


    private:

        /**
         * Constructor.
         * @param head segment header.
         */
        explicit EvioSegment(std::shared_ptr<SegmentHeader> const & head) : BaseStructure(head) {}

    public:

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param head segment header.
         * @return shared pointer to new EvioSegment.
         */
        static std::shared_ptr<EvioSegment> getInstance(std::shared_ptr<SegmentHeader> const & head) {
            std::shared_ptr<EvioSegment> pNode(new EvioSegment(head));
            return pNode;
        }

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param tag segment tag.
         * @param typ segment data type.
         * @return shared pointer to new EvioSegment.
         */
        static std::shared_ptr<EvioSegment> getInstance(uint16_t tag, DataType const & typ) {
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
