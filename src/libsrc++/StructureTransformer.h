//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_STRUCTURETRANSFORMER_H
#define EVIO_6_0_STRUCTURETRANSFORMER_H


#include <cstring>
#include <sstream>
#include <memory>


#include "BankHeader.h"
#include "SegmentHeader.h"
#include "TagSegmentHeader.h"
#include "DataType.h"
#include "EvioBank.h"
#include "EvioSegment.h"
#include "EvioTagSegment.h"


namespace evio {

    /**
     * This class contains methods to transform structures from one type to another,
     * for example, changing an EvioSegment into an EvioBank.
     *
     * @author timmer
     * @date 6/3/2020 (10/1/2010 original java)
     */
    class StructureTransformer {

    public:

        // Segment --> Bank

        /**
         * Create an EvioBank object from an EvioSegment. The new object has all
         * data copied over, <b>except</b> that the segment's children were are added
         * (not deep cloned) to the bank. Because a segment has no num, the user
         * supplies that as an arg.
         *
         * @param segment EvioSegment object to transform.
         * @param num num of the created EvioBank.
         * @return the created EvioBank.
         */
        static std::shared_ptr<EvioBank> transform(std::shared_ptr<EvioSegment> const & segment, uint8_t num) {
            // Copy over header & create new EvioBank
            auto const & segHeader = segment->getHeader();
            auto bank = EvioBank::getInstance(segHeader->getTag(), segHeader->getDataType(), num);
            auto const & bankHeader = bank->getHeader();
            bankHeader->setLength(segHeader->getLength() + 1);
            bankHeader->setPadding(segHeader->getPadding());

            // Copy over the data (take care of padding)
            bank->transform(segment);
            return bank;
        }

        /**
         * Copy the data in an EvioSegment object into an existing EvioBank. Note, however,
         * that the segment's children were are added (not deep cloned) to the bank.
         * Because a segment has no num, the user supplies that as an arg.
         *
         * @param bank EvioBank object to copy into.
         * @param segment EvioSegment object to copy.
         * @param num num of the EvioBank.
         */
        static void copy(std::shared_ptr<EvioBank> const & bank,
                         std::shared_ptr<EvioSegment> const & segment,
                         uint8_t num) {

            // Copy over header
            auto const & segHeader  = segment->getHeader();
            auto const & bankHeader = bank->getHeader();
            bankHeader->copy(segHeader);
            bankHeader->setNumber(num);
            bankHeader->setLength(segHeader->getLength() + 1);
            bankHeader->setPadding(segHeader->getPadding());

            // Copy over the data (take care of padding)
            bank->transform(segment);
        }

        // TagSegment --> Bank

        /**
         * Create an EvioBank object from an EvioTagSegment. The new object has all
         * data copied over, <b>except</b> that the tagsegment's children were are added
         * (not deep cloned) to the bank. Because a tagsegment has no num, the user
         * supplies that as an arg.<p>
         *
         * NOTE: A tagsegment has no associated padding data. However,
         * the bank.transform() method will calculate it and set it in the bank header.
         *
         * @param tagsegment EvioTagSegment object to transform.
         * @param num num of the created EvioBank.
         * @return the created EvioBank.
         */
        static std::shared_ptr<EvioBank> transform(std::shared_ptr<EvioTagSegment> const & tagsegment, uint8_t num) {
            auto const & tagsegHeader = tagsegment->getHeader();
            auto bank = EvioBank::getInstance(tagsegHeader->getTag(), tagsegHeader->getDataType(), num);
            auto const & bankHeader = bank->getHeader();
            bankHeader->setLength(tagsegHeader->getLength() + 1);

            bank->transform(tagsegment);
            return bank;
        }

        /**
         * Copy the data in an EvioTagSegment object into an existing EvioBank. Note, however,
         * that the tagsegment's children were are added (not deep cloned) to the bank.
         * Because a tagsegment has no num, the user supplies that as an arg.
         *
         * NOTE: A tagsegment has no associated padding data. However,
         * the bank.transform() method will calculate it and set it in the bank header.
         *
         * @param bank EvioBank object to copy into.
         * @param tagsegment EvioTagSegment object to copy.
         * @param num num of the EvioBank.
         */
        static void copy(std::shared_ptr<EvioBank> const & bank,
                         std::shared_ptr<EvioTagSegment> const & tagsegment,
                         uint8_t num) {

            auto const & tagsegHeader = tagsegment->getHeader();
            auto const & bankHeader = bank->getHeader();
            bankHeader->copy(tagsegHeader);
            bankHeader->setNumber(num);
            bankHeader->setLength(tagsegHeader->getLength() + 1);

            bank->transform(tagsegment);
        }

        // Segment --> TagSegment

        /**
         * Create an EvioTagSegment object from an EvioSegment. The new object has all
         * data copied over, <b>except</b> that the segment's children were are added
         * (not deep cloned) to the tagsegment.<p>
         *
         * NOTE: No data should be lost in this transformaton since even though the
         * segment serializes 6 bits of data type when being written out while the tag segment
         * serializes 4, only 4 bits are needed to contain the equivalent type data.
         * And, the segment's tag is serialized into 8 bits while the tagsegment's tag uses 12 bits
         * so no problem there.
         *
         * @param segment EvioSegment object to transform.
         * @return the created EvioTagSegment.
         */
        static std::shared_ptr<EvioTagSegment> transform(std::shared_ptr<EvioSegment> const & segment) {
            auto const & segHeader = segment->getHeader();
            auto ts = EvioTagSegment::getInstance(segHeader->getTag(), segHeader->getDataType());
            auto const & tsHeader = ts->getHeader();
            tsHeader->setLength(segHeader->getLength());
            tsHeader->setPadding(segHeader->getPadding());

            // Change 6 bit content type to 4 bits. Do this by changing
            // BANK to ALSOBANK, SEGMENT to ALSOSEGMENT
            DataType const & segType = segHeader->getDataType();
            if (segType == DataType::BANK) {
                tsHeader->setDataType(DataType::ALSOBANK);
            }
            else if (segType == DataType::SEGMENT) {
                tsHeader->setDataType(DataType::ALSOSEGMENT);
            }

            ts->transform(segment);
            return ts;
        }

        /**
         * Copy the data in an EvioSegment object into an existing EvioTagSegment. Note, however,
         * that the segment's children were are added (not deep cloned) to the tagsegment.
         *
         * NOTE: No data should be lost in this transformaton since even though the
         * segment serializes 6 bits of data type when being written out while the tag segment
         * serializes 4, only 4 bits are needed to contain the equivalent type data.
         * And, the segment's tag is serialized into 8 bits while the tagsegment's tag uses 12 bits
         * so no problem there.
         *
         * @param tagsegment EvioTagSegment object to copy into.
         * @param segment EvioSegment object to copy.
         */
        static void copy(std::shared_ptr<EvioTagSegment> const & tagsegment,
                         std::shared_ptr<EvioSegment> const & segment) {

            auto const & segHeader = segment->getHeader();
            auto const & tsHeader  = tagsegment->getHeader();
            tsHeader->copy(segHeader);

            // Change 6 bit content type to equivalent 4 bits
            DataType const & segType = segHeader->getDataType();
            if (segType == DataType::BANK) {
                tsHeader->setDataType(DataType::ALSOBANK);
            }
            else if (segType == DataType::SEGMENT) {
                tsHeader->setDataType(DataType::ALSOSEGMENT);
            }

            tagsegment->transform(segment);
        }

        // TagSegment --> Segment

        /**
         * Create an EvioSegment object from an EvioTagSegment. The new object has all
         * data copied over, <b>except</b> that the tagsegment's children were are added
         * (not deep cloned) to the segment.<p>
         *
         * NOTE: A tagsegment has no associated padding data. However,
         * the transform() method will calculate it and set it in the segment header.
         * Tags are stored in a 16 bit int and so this transformation
         * will never lose any tag data. Only when a segment's tag is written out or
         * serialized into 8 bits will this become an issue since a tagsegment's tag is
         * serialized as 12 bits.
         *
         * @param tagsegment EvioTagSegment object to transform.
         * @return the created EvioSegment.
         */
        static std::shared_ptr<EvioSegment> transform(std::shared_ptr<EvioTagSegment> const & tagsegment) {
            auto const & tsHeader = tagsegment->getHeader();
            auto seg = EvioSegment::getInstance(tsHeader->getTag(), tsHeader->getDataType());
            auto const & segHeader = seg->getHeader();
            segHeader->setLength(tsHeader->getLength());

            seg->transform(tagsegment);
            return seg;
        }

        /**
         * Copy the data in an EvioTagSegment object into an existing EvioSegment. Note, however,
         * that the tagsegment's children were are added (not deep cloned) to the segment.
         *
         * NOTE: A tagsegment has no associated padding data. However,
         * the transform() method will calculate it and set it in the segment header.
         * Tags are stored in a 16 bit int and so this transformation
         * will never lose any tag data. Only when a segment's tag is written out or
         * serialized into 8 bits will this become an issue since a tagsegment's tag is
         * serialized as 12 bits.
         *
         * @param segment EvioSegment object to copy into.
         * @param tagsegment EvioTagSegment object to copy.
         */
        static void copy(std::shared_ptr<EvioSegment> const & segment,
                         std::shared_ptr<EvioSegment> const & tagsegment) {

            auto const & tsHeader  = tagsegment->getHeader();
            auto const & segHeader = segment->getHeader();
            segHeader->copy(tsHeader);
            tagsegment->transform(segment);
        }

        // Bank -> Segment

        /**
         * Create an EvioSegment object from an EvioBank. The new object has all
         * data copied over, <b>except</b> that the bank's children were are added
         * (not deep cloned) to the segment.<p>
         *
         * <b>TAG: </b>Tags are stored in a 16 bit int and so this transformation
         * will never lose any tag data. Only when a segment's tag is written out or
         * serialized into 8 bits will this become an issue since a bank's tag is
         * serialized as 16 bits.<p>
         *
         * <b>NUM: </b>A segment has no num data and so the bank's num is lost.
         * The bank's num is actually copied into segment header so in that sense it
         * still exists, but will never be written out or serialized.
         *
         * <b>LENGTH: </b>It is possible that the length of the bank (32 bits) is too
         * big for the segment (16 bits). This condition will cause an exception.
         *
         * @param bank EvioBank object to transform.
         * @return the created EvioSegment.
         * @throws EvioException if the bank is too long to change into a segment
         */
        static std::shared_ptr<EvioSegment> transform(std::shared_ptr<EvioBank> const & bank) {
            auto const & bankHeader = bank->getHeader();
            size_t bankLen = bankHeader->getLength();
            if (bankLen > 65535) {
                throw new EvioException("Bank is too long to transform into segment");
            }
            auto segment = EvioSegment::getInstance(bankHeader->getTag(), bankHeader->getDataType());
            auto const & segHeader = segment->getHeader();
            segHeader->setLength(bankLen - 1);
            segHeader->setPadding(bankHeader->getPadding());
            segHeader->setNumber(bankHeader->getNumber());

            segment->transform(bank);
            return segment;
        }


        /**
         * Copy the data in an EvioBank object into an existing EvioSegment. Note, however,
         * that the banks's children were are added (not deep cloned) to the segment.
         *
         * <b>TAG: </b>Tags are stored in a 16 bit int and so this transformation
         * will never lose any tag data. Only when a segment's tag is written out or
         * serialized into 8 bits will this become an issue since a bank's tag is
         * serialized as 16 bits.<p>
         *
         * <b>NUM: </b>A segment has no num data and so the bank's num is lost.
         * The bank's num is actually copied into segment header so in that sense it
         * still exists, but will never be written out or serialized.
         *
         * <b>LENGTH: </b>It is possible that the length of the bank (32 bits) is too
         * big for the segment (16 bits). This condition will cause an exception.
         *
         * @param segment EvioSegment object to copy into.
         * @param bank EvioBank object to copy.
         * @throws EvioException if the bank is too long to change into a segment
         */
        static void copy(std::shared_ptr<EvioSegment> const & segment,
                         std::shared_ptr<EvioBank> const & bank) {

            auto const & bankHeader = bank->getHeader();
            size_t bankLen = bankHeader->getLength();
            if (bankLen > 65535) {
                throw new EvioException("Bank is too long to transform into segment");
            }
            auto const & segHeader = segment->getHeader();
            segHeader->copy(bankHeader);
            segHeader->setLength(bankLen - 1);

            segment->transform(bank);
        }

        // Bank -> TagSegment

        /**
         * Create an EvioTagSegment object from an EvioBank. The new object has all
         * data copied over, <b>except</b> that the bank's children were are added
         * (not deep cloned) to the tagsegment.<p>
         *
         * <b>TAG: </b>Tags are stored in a 16 bit int and so this transformation
         * will never lose any tag data. Only when a tagsegment's tag is written out or
         * serialized into 12 bits will this become an issue since a bank's tag is
         * serialized as 16 bits.<p>
         *
         * <b>NUM: </b>A tagsegment has no num data and so the bank's num is lost.
         * The bank's num is actually copied into tagsegment header so in that sense it
         * still exists, but will never be written out or serialized.<p>
         *
         * <b>LENGTH: </b>It is possible that the length of the bank (32 bits) is too
         * big for the tagsegment (16 bits). This condition will cause an exception.<p>
         *
         * <b>TYPE: </b>No data should be lost in this transformaton since even though the
         * bank serializes 6 bits of data type when being written out while the tagsegment
         * serializes 4, only 4 bits are needed to contain the equivalent type data.<p>
         *
         * @param bank EvioBank object to transform.
         * @param dummy only used to distinguish this method from {@link #transform(std::shared_ptr<EvioBank> const &)}.
         * @return the created EvioTagSegment.
         * @throws EvioException if the bank is too long to change into a tagsegment
         */
        static std::shared_ptr<EvioTagSegment> transform(std::shared_ptr<EvioBank> const & bank,
                                                         int dummy) {
            auto const & bankHeader = bank->getHeader();
            if (bankHeader->getLength() > 65535) {
                throw new EvioException("Bank is too long to transform into segment");
            }
            auto ts = EvioTagSegment::getInstance(bankHeader->getTag(), bankHeader->getDataType());
            auto const & tsHeader = ts->getHeader();
            tsHeader->setLength(bankHeader->getLength() - 1);
            tsHeader->setPadding(bankHeader->getPadding());
            tsHeader->setNumber(bankHeader->getNumber());

            DataType const & tsType = tsHeader->getDataType();
            if (tsType == DataType::BANK) {
                tsHeader->setDataType(DataType::ALSOBANK);
            }
            else if (tsType == DataType::SEGMENT) {
                tsHeader->setDataType(DataType::ALSOSEGMENT);
            }

            ts->transform(bank);
            return ts;
        }


        /**
         * Copy the data in an EvioBank object into an existing EvioTagSegment. Note, however,
         * that the banks's children were are added (not deep cloned) to the tagsegment.
         *
         * <b>TAG: </b>Tags are stored in a 16 bit int and so this transformation
         * will never lose any tag data. Only when a tagsegment's tag is written out or
         * serialized into 12 bits will this become an issue since a bank's tag is
         * serialized as 16 bits.<p>
         *
         * <b>NUM: </b>A segment has no num data and so the bank's num is lost.
         * The bank's num is actually copied into segment header so in that sense it
         * still exists, but will never be written out or serialized.
         *
         * <b>LENGTH: </b>It is possible that the length of the bank (32 bits) is too
         * big for the segment (16 bits). This condition will cause an exception.
         *
         * @param tagsegment EvioTagSegment object to copy into.
         * @param bank EvioBank object to copy.
         * @throws EvioException if the bank is too long to change into a segment
         */
        static void copy(std::shared_ptr<EvioTagSegment> const & tagsegment,
                         std::shared_ptr<EvioBank> const & bank) {

            auto const & bankHeader = bank->getHeader();
            size_t bankLen = bankHeader->getLength();
            if (bankLen > 65535) {
                throw new EvioException("Bank is too long to transform into tagsegment");
            }
            auto const & tsHeader = tagsegment->getHeader();
            tsHeader->copy(bankHeader);
            tsHeader->setLength(bankLen - 1);

            DataType const & bankType = bankHeader->getDataType();
            if (bankType == DataType::BANK) {
                tsHeader->setDataType(DataType::ALSOBANK);
            }
            else if (bankType == DataType::SEGMENT) {
                tsHeader->setDataType(DataType::ALSOSEGMENT);
            }

            tagsegment->transform(bank);
        }


    };



}

#endif //EVIO_6_0_STRUCTURETRANSFORMER_H
