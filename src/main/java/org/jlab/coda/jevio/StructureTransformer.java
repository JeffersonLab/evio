package org.jlab.coda.jevio;

/**
 * This class contains methods to transform structures from one type to another,
 * for example changing an EvioSegment into an EvioBank.
 *
 * Date: Oct 1, 2010
 * @author timmer
 */
public class StructureTransformer {

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
    static public EvioBank transform(EvioSegment segment, int num) {
        BaseStructureHeader header = segment.getHeader();
        DataType type = header.getDataType();

        BankHeader bankHeader = new BankHeader(header.getTag(), type, num);
        bankHeader.setLength(header.getLength()+1);
        bankHeader.setPadding(header.getPadding());

        EvioBank bank = new EvioBank(bankHeader);
        // Copy over the data & take care of padding
        bank.transform(segment);
        return bank;
    }


    /**
     * Create an EvioBank object from an EvioTagSegment. The new object has all
     * data copied over, <b>except</b> that the tagsegment's children were are added
     * (not deep cloned) to the bank. Because a segment has no num, the user
     * supplies that as an arg.<p>
     *
     * NOTE: A tagsegment has no associated padding data. However,
     * the bank.transform() method will calculate it and set it in the bank header.
     *
     * @param tagsegment EvioTagSegment object to transform.
     * @param num num of the created EvioBank.
     * @return the created EvioBank.
     */
    static public EvioBank transform(EvioTagSegment tagsegment, int num) {
        BaseStructureHeader header = tagsegment.getHeader();
        DataType type = header.getDataType();

        BankHeader bankHeader = new BankHeader(header.getTag(), type, num);
        bankHeader.setLength(header.getLength()+1);

        EvioBank bank = new EvioBank(bankHeader);
        bank.transform(tagsegment);
        return bank;
    }


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
    static public EvioTagSegment transform(EvioSegment segment) {
        BaseStructureHeader segHeader = segment.getHeader();
        DataType type;
        DataType segType = type = segHeader.getDataType();
        
        // Change 6 bit content type to 4 bits. Do this by changing
        // BANK to ALSOBANK, SEGMENT to ALSOSEGMENT (ALSOTAGSEGMENT already removed)
        if (segType == DataType.BANK) {
            type = DataType.ALSOBANK;
        }
        else if (segType == DataType.SEGMENT) {
            type = DataType.ALSOSEGMENT;
        }

        TagSegmentHeader tagsegHeader = new TagSegmentHeader(segHeader.getTag(), type);
        tagsegHeader.setLength(segHeader.getLength());
        tagsegHeader.setPadding(segHeader.getPadding());

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
     * the transform() method will calculate it and set it in the segment header.
     * Tags are stored in a 16 bit int and so this transformation
     * will never lose any tag data. Only when a segment's tag is written out or
     * serialized into 8 bits will this become an issue since a tagsegment's tag is
     * serialized as 12 bits.
     *
     * @param tagsegment EvioTagSegment object to transform.
     * @return the created EvioSegment.
     */
    static public EvioSegment transform(EvioTagSegment tagsegment) {
        BaseStructureHeader tagsegHeader = tagsegment.getHeader();
        DataType type = tagsegHeader.getDataType();

        SegmentHeader segHeader = new SegmentHeader(tagsegHeader.getTag(), type);
        segHeader.setLength(tagsegHeader.getLength());

        EvioSegment seg = new EvioSegment(segHeader);
        seg.transform(tagsegment);
        return seg;
    }

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
     * still exists, but will never be written out or serialized.<p>
     *
     * <b>LENGTH: </b>It is possible that the length of the bank (32 bits) is too
     * big for the segment (16 bits). This condition will cause an exception.
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

        SegmentHeader segHeader = new SegmentHeader(bank.getHeader().getTag(), type);
        segHeader.setLength(header.getLength()-1);
        segHeader.setPadding(header.getPadding());
        segHeader.setNumber(header.getNumber());

        EvioSegment seg = new EvioSegment(segHeader);
        seg.transform(bank);
        return seg;
    }

    /**
     * Create an EvioTagSegment object from an EvioBank. The new object has all
     * data copied over, <b>except</b> that the bank's children were are added
     * (not deep cloned) to the segment.<p>
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
        // BANK to ALSOBANK, SEGMENT to ALSOSEGMENT (ALSOTAGSEGMENT already removed)
        if (bankType == DataType.BANK) {
            type = DataType.ALSOBANK;
        }
        else if (bankType == DataType.SEGMENT) {
            type = DataType.ALSOSEGMENT;
        }

        TagSegmentHeader tagsegHeader = new TagSegmentHeader(bank.getHeader().getTag(), type);
        tagsegHeader.setLength(header.getLength()-1);
        tagsegHeader.setPadding(header.getPadding());

        EvioTagSegment tagseg = new EvioTagSegment(tagsegHeader);
        tagseg.transform(bank);
        return tagseg;
    }
}
