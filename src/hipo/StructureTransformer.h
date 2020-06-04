//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#ifndef EVIO_6_0_STRUCTURETRANSFORMER_H
#define EVIO_6_0_STRUCTURETRANSFORMER_H

#include <cstring>
#include <sstream>
#include <memory>

#include "ByteBuffer.h"
#include "BaseStructure.h"
#include "BaseStructureHeader.h"
#include "BankHeader.h"

#include "DataType.h"
#include "EvioBank.h"
#include "EvioSegment.h"
#include "EvioTagSegment.h"


namespace evio {

    /**
     * This class contains methods to transform structures from one type to another,
     * for example changing an EvioSegment into an EvioBank.
     *
     * @author timmer
     * @date 6/3/2020 (10/1/2010 original java)
     */
    class StructureTransformer {

    public:

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
                auto segHeader = segment->getHeader();
                auto bank = EvioBank::getInstance(segHeader->getTag(), segHeader->getDataType(), num);
                auto bankHeader = bank->getHeader();
                bankHeader.setLength(segHeader->getLength() + 1);

                // Copy over the data & take care of padding
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
                auto segHeader  = segment->getHeader();
                auto bankHeader = bank->getHeader()->copy(segHeader);
                bankHeader->setNumber(num);
                bankHeader->setLength(segHeader->getLength() + 1);
                // Copy over the data & take care of padding
                bank->transform(segment);
            }


            /**
             * Create an EvioBank object from an EvioTagSegment. The new object has all
             * data copied over, <b>except</b> that the tagsegment's children were are added
             * (not deep cloned) to the bank. Because a tagsegment has no num, the user
             * supplies that as an arg.<p>
             *
             * NOTE: A tagsegment has no associated padding data. However,
             * the transform() method will calculate it and set it in the bank header.
             *
             * @param tagsegment EvioTagSegment object to transform.
             * @param num num of the created EvioBank.
             * @return the created EvioBank.
             */
            static std::shared_ptr<EvioBank> transform(std::shared_ptr<EvioTagSegment> const & tagsegment, uint8_t num) {
                auto tagsegHeader = tagsegment->getHeader();
                auto bank = EvioBank::getInstance(tagsegHeader->getTag(), tagsegHeader->getDataType(), num);
                auto bankHeader = bank->getHeader();
                bankHeader.setLength(tagsegHeader->getLength() + 1);
                bank->transform(tagsegment);
                return bank;
            }


            /**
             * Copy the data in an EvioTagSegment object into an existing EvioBank. Note, however,
             * that the tagsegment's children were are added (not deep cloned) to the bank.
             * Because a tagsegment has no num, the user supplies that as an arg.
             *
             * NOTE: A tagsegment has no associated padding data. However,
             * the transform() method will calculate it and set it in the bank header.
             *
             * @param bank EvioBank object to copy into.
             * @param segment EvioTagSegment object to copy.
             * @param num num of the EvioBank.
             */
            static void copy(std::shared_ptr<EvioBank> const & bank,
                             std::shared_ptr<EvioTagSegment> const & tagsegment,
                             uint8_t num) {

                auto tagsegHeader = tagsegment->getHeader();
                auto bankHeader = bank->getHeader()->copy(tagsegHeader);
                bankHeader->setNumber(num);
                bankHeader->setLength(tagsegHeader->getLength() + 1);
                bank->transform(tagsegment);
            }


        /**
         * Create an EvioTagSegment object from an EvioSegment. The new object has all
         * data copied over, <b>except</b> that the segment's children were are added
         * (not deep cloned) to the tagsegment.<p>
         *
         * NOTE: No data should be lost in this transformaton since even though the
         * segment has 6 bits of data type while the tag segment has only 4, only 4 bits
         * are needed to contain the type data. And, the segment's tag is 8 bits while
         * the tagsegment's tag is 12 bits so no problem there.
         *
         * @param segment EvioSegment object to transform.
         * @return the created EvioTagSegment.
         */
        static public EvioTagSegment transform(EvioSegment segment) {
            BaseStructureHeader segHeader = segment.getHeader();
            DataType type;
            DataType segType = type = segHeader.getDataType();

            // Change 6 bit content type to 4 bits. Do this by changing
            // ALSOBANK to BANK, ALSOSEGMENT to SEGMENT (ALSOTAGSEGMENT already removed)
            if (segType == DataType.ALSOBANK) {
                type = DataType.BANK;
            }
            else if (segType == DataType.ALSOSEGMENT) {
                type = DataType.SEGMENT;
            }

            // 8 bit segment tag now becomes 12 bits
            TagSegmentHeader tagsegHeader = new TagSegmentHeader(segHeader.getTag(), type);
            tagsegHeader.setLength(segHeader.getLength());

            EvioTagSegment tagseg = new EvioTagSegment(tagsegHeader);
            tagseg.transform(segment);
            return tagseg;
        }

        /**
         * Create an EvioSegment object from an EvioTagSegment. The new object has all
         * data copied over, <b>except</b> that the tagsegment's children were are added
         * (not deep cloned) to the segment.<p>
         *
         * NOTE: A tagsegment has no associated padding data. However,
         * if a tagsegment is read from a file/buffer, padding info is already lost (=0), and
         * if a tagsegment is created "by hand", the padding has already been calculated
         * and exists in the header. It is also possible that data is lost in this
         * transformaton since the segment's tag is 8 bits while the tagsegment's tag is
         * 12 bits. The user can override the truncation of the tagsegment's tag and simply
         * set the created segment's tag by calling segment.getHeader().setTag(tag).
         *
         * @param tagsegment EvioTagSegment object to transform.
         * @return the created EvioSegment.
         */
        static public EvioSegment transform(EvioTagSegment tagsegment) {
            BaseStructureHeader tagsegHeader = tagsegment.getHeader();
            DataType type = tagsegHeader.getDataType();

            // A tagseg tag is 12 bits which must be truncated to the seg's 8 bits.
            // The user can override this by setting the resultant segment's tag by hand.
            SegmentHeader segHeader = new SegmentHeader(tagsegHeader.getTag(), type);
            segHeader.setLength(tagsegHeader.getLength());
            segHeader.setPadding(tagsegHeader.getPadding());

            EvioSegment seg = new EvioSegment(segHeader);
            seg.transform(tagsegment);
            return seg;
        }

        /**
         * Create an EvioSegment object from an EvioBank. The new object has all
         * data copied over, <b>except</b> that the bank's children were are added
         * (not deep cloned) to the segment.<p>
         *
         * NOTE: It is possible that data is lost in this transformaton since the
         * segment's tag is 8 bits while the bank's tag is 16 bits. To override the
         * truncation of the tag, simply set the created segment's tag by calling
         * segment.getHeader().setTag(tag). It is also possible that the length of
         * the bank (32 bits) is too big for the segment (16 bits). This condition
         * will cause an exception.
         *
         * @param bank EvioBank object to transform.
         * @return the created EvioSegment.
         * @throws EvioException if the bank is too long to change into a segment
         */
        static public EvioSegment transform(EvioBank bank) throws EvioException {
                BaseStructureHeader header = bank.getHeader();
                if (header.getLength() > 65535) {
                    throw new EvioException("Bank is too long to transform into segment");
                }
                DataType type = header.getDataType();

                // 16 bit bank tag now becomes 8 bits
                SegmentHeader segHeader = new SegmentHeader(bank.getHeader().getTag(), type);
                segHeader.setLength(header.getLength()-1);
                segHeader.setPadding(header.getPadding());

                EvioSegment seg = new EvioSegment(segHeader);
                seg.transform(bank);
                return seg;
        }

        /**
         * Create an EvioTagSegment object from an EvioBank. The new object has all
         * data copied over, <b>except</b> that the bank's children were are added
         * (not deep cloned) to the segment.<p>
         *
         * NOTE: It is possible that data is lost in this transformaton since the
         * tagsegment's tag is 12 bits while the bank's tag is 16 bits. To override the
         * truncation of the tag, simply set the created tagsegment's tag by calling
         * tagsegment.getHeader().setTag(tag). It is also possible that the length of
         * the bank (32 bits) is too big for the tagsegment (16 bits). This condition
         * will cause an exception.
         *
         * @param bank EvioBank object to transform.
         * @param dummy only used to distinguish this method from {@link #transform(EvioBank)}.
         * @return the created EvioTagSegment.
         * @throws EvioException if the bank is too long to change into a tagsegment
         */
        static public EvioTagSegment transform(EvioBank bank, int dummy) throws EvioException {
                BaseStructureHeader header = bank.getHeader();
                if (header.getLength() > 65535) {
                    throw new EvioException("Bank is too long to transform into tagsegment");
                }
                DataType type;
                DataType bankType = type = header.getDataType();

                // Change 6 bit content type to 4 bits. Do this by changing
                // ALSOBANK to BANK, ALSOSEGMENT to SEGMENT (ALSOTAGSEGMENT already removed)
                if (bankType == DataType.ALSOBANK) {
                    type = DataType.BANK;
                }
                else if (bankType == DataType.ALSOSEGMENT) {
                    type = DataType.SEGMENT;
                }

                // 16 bit bank tag now becomes 12 bits
                TagSegmentHeader tagsegHeader = new TagSegmentHeader(bank.getHeader().getTag(), type);
                tagsegHeader.setLength(header.getLength()-1);
                tagsegHeader.setPadding(header.getPadding());

                EvioTagSegment tagseg = new EvioTagSegment(tagsegHeader);
                tagseg.transform(bank);
                return tagseg;
        }
    };



}

#endif //EVIO_6_0_STRUCTURETRANSFORMER_H
