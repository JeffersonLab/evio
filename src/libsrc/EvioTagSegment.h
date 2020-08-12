//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOTAGSEGMENT_H
#define EVIO_6_0_EVIOTAGSEGMENT_H


#include <cstring>
#include <memory>


#include "ByteBuffer.h"
#include "DataType.h"
#include "BaseStructure.h"
#include "TagSegmentHeader.h"
#include "StructureType.h"


namespace evio {

    /**
      * This holds a CODA TagSegment structure. Mostly it has a header (a <code>TagSegementHeader</code>)
      * and the raw data stored as a vector.
      *
      * @author heddle
      * @author timmer
      */
    class EvioTagSegment : public BaseStructure {


    private:

        /**
         * Constructor.
         * @param head tagsegment header.
         */
        explicit EvioTagSegment(std::shared_ptr<TagSegmentHeader> const & head) : BaseStructure(head) {}

    public:

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param head tagsegment header.
         * @return shared pointer to new EvioTagSegment.
         */
        static std::shared_ptr<EvioTagSegment> getInstance(std::shared_ptr<TagSegmentHeader> const & head) {
            std::shared_ptr<EvioTagSegment> pNode(new EvioTagSegment(head));
            return pNode;
        }

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param tag tagsegment tag.
         * @param typ tagsegment data type.
         * @return shared pointer to new EvioTagSegment.
         */
        static std::shared_ptr<EvioTagSegment> getInstance(uint16_t tag, DataType const & typ) {
            std::shared_ptr<TagSegmentHeader> head(new TagSegmentHeader(tag, typ));
            std::shared_ptr<EvioTagSegment> pNode(new EvioTagSegment(head));
            return pNode;
        }

        /**
         * This implements the virtual method from <code>BaseStructure</code>.
         * This returns the type of this structure, not the type of data this structure holds.
         *
         * @return the <code>StructureType</code> of this structure, which is a StructureType::STRUCT_TAGSEGMENT.
         * @see StructureType
         */
        StructureType getStructureType() const override {
            return StructureType::STRUCT_TAGSEGMENT;
        }

    };

}


#endif //EVIO_6_0_EVIOTAGSEGMENT_H
