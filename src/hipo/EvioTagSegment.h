//
// Created by Carl Timmer on 2020-05-15.
//

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

    class EvioTagSegment : public BaseStructure {


    private:

        explicit EvioTagSegment(std::shared_ptr<TagSegmentHeader> const & head) : BaseStructure(head) {
            std::cout << "EvioTagSegment's private constructor" << std::endl;
        }

    public:

        static std::shared_ptr<EvioTagSegment> getInstance(std::shared_ptr<TagSegmentHeader> const & head) {
            std::shared_ptr<EvioTagSegment> pNode(new EvioTagSegment(head));
            return pNode;
        }

        static std::shared_ptr<EvioTagSegment> getInstance(uint32_t tag, DataType typ) {
            std::shared_ptr<TagSegmentHeader> head(new TagSegmentHeader(tag, typ));
            std::shared_ptr<EvioTagSegment> pNode(new EvioTagSegment(head));
            return pNode;
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
         * @return the <code>StructureType</code> of this structure, which is a StructureType::STRUCT_TAGSEGMENT.
         * @see StructureType
         */
        StructureType getStructureType() override {
            return StructureType::STRUCT_TAGSEGMENT;
        }

    };

}


#endif //EVIO_6_0_EVIOTAGSEGMENT_H
