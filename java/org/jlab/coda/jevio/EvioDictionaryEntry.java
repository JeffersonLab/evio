/*
 * Copyright (c) 2015, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio;

/**
 * Class to facilitate use of Evio XML dictionary entry data as a key or value in a hash table.
 * Objects can only be instantiated by classes in this package.
 *
 * (8/17/15).
 * @author timmer.
 */
public class EvioDictionaryEntry {

    /** Tag value or low end of a tag range of an evio container. */
    private final Integer tag;

    /** If > 0 && != tag, this is the high end of a tag range. Never null, always >= 0.
     *  @since 5.2 */
    private final Integer tagEnd;

    /** Num value of evio container which may be null if not given in xml entry. */
    private final Integer num;

    /** Type of data in evio container. */
    private DataType type;

    /** String used to identify format of data if CompositeData type. */
    private final String format;

    /** String used to describe data if CompositeData type. */
    private final String description;

    /** Does this entry specify a tag & num, only a tag, or a tag range? */
    private final EvioDictionaryEntryType entryType;


    // Track parent so identical tag/num/tagEnd can be used in another entry
    // iff the parent tag/num/tagEnd is different. For simplicity limit this
    // to one parent and not the stack/tree.
//    private Integer parentNum;
//    private Integer parentTag;
//    private Integer parentTagEnd;
//    private EvioDictionaryEntryType parentEntryType;

    private EvioDictionaryEntry parentEntry;


    /**
     * Constructor.
     * @param tag  tag value of evio container.
     */
    EvioDictionaryEntry(Integer tag) {
        this(tag, null, null, null, null);
    }


    /**
     * Constructor.
     * @param tag  tag value of evio container.
     * @param num  num value of evio container.
     */
    public EvioDictionaryEntry(Integer tag, Integer num) {
        this(tag, num, null, null, null);
    }


    /**
     * Constructor.
     * @param tag  tag value of evio container.
     * @param num  num value of evio container.
     * @param type type of data in evio container which may be (case-independent):
     *             unknown32 {@link DataType#UNKNOWN32} ...
     *             composite {@link DataType#COMPOSITE}.
     */
    EvioDictionaryEntry(Integer tag, Integer num, String type) {
        this(tag, num, type, null, null);
    }


    /**
     * Constructor.
     * @param tag    tag value of evio container.
     * @param num    num value of evio container.
     * @param tagEnd if &gt; 0, this is the high end of a tag range.
     * @param type   type of data in evio container which may be (case-independent):
     *               unknown32 {@link DataType#UNKNOWN32} ...
     *               composite {@link DataType#COMPOSITE}.
     */
    public EvioDictionaryEntry(Integer tag, Integer num, Integer tagEnd, String type) {
        this(tag, num, tagEnd, type, null, null);
    }


    /**
     * Constructor.
     *
     * @param tag tag value of evio container.
     * @param num num value of evio container.
     * @param type type of data in evio container which may be (case-independent):
     *             unknown32 {@link DataType#UNKNOWN32} ...
     *             composite {@link DataType#COMPOSITE}.
     * @param description description of CompositeData
     * @param format format of CompositeData
     */
    EvioDictionaryEntry(Integer tag, Integer num, String type,
                        String description, String format) {
        this(tag, num, 0, type, description, format);
    }


    /**
     * Constructor containing actual implementation.
     * Caller assumes responsibility of supplying correct arg values.
     * If tag &gt; tagEnd, these values are switched so tag &lt; tagEnd.
     *
     * @param tag    tag value or low end of a tag range of an evio container.
     * @param num    num value of evio container.
     * @param tagEnd if &gt; 0, this is the high end of a tag range.
     * @param type   type of data in evio container which may be (case-independent):
     *             unknown32 {@link DataType#UNKNOWN32} ...
     *             composite {@link DataType#COMPOSITE}.
     * @param description description of CompositeData
     * @param format format of CompositeData
     */
    public EvioDictionaryEntry(Integer tag, Integer num, Integer tagEnd,
                               String type, String description, String format) {

        this(tag, num, tagEnd, type, description, format, null);
    }


    /**
     * Constructor containing actual implementation.
     * Caller assumes responsibility of supplying correct arg values.
     * If tag &gt; tagEnd, these values are switched so tag &lt; tagEnd.
     *
     * @param tag    tag value or low end of a tag range of an evio container.
     * @param tagEnd if &gt; 0, this is the high end of a tag range.
     * @param num    num value of evio container.
     * @param type   type of data in evio container which may be (case-independent):
     *      unknown32 {@link DataType#UNKNOWN32},
     *      int32 {@link DataType#INT32},
     *      uint32 {@link DataType#UINT32},
     *      float32{@link DataType#FLOAT32},
     *      double64 {@link DataType#DOUBLE64},
     *      charstar8 {@link DataType#CHARSTAR8},
     *      char8 {@link DataType#CHAR8},
     *      uchar8 {@link DataType#UCHAR8},
     *      short16{@link DataType#SHORT16},
     *      ushort16 {@link DataType#USHORT16},
     *      long64 {@link DataType#LONG64},
     *      ulong64 {@link DataType#ULONG64},
     *      tagsegment {@link DataType#TAGSEGMENT},
     *      segment {@link DataType#SEGMENT}.
     *      alsosegment {@link DataType#ALSOSEGMENT},
     *      bank {@link DataType#BANK},
     *      alsobank {@link DataType#ALSOBANK}, or
     *      composite {@link DataType#COMPOSITE},
     * @param description   description of CompositeData
     * @param format        format of CompositeData
     * @param parentEntry   parent dictionary entry object
     */
    public EvioDictionaryEntry(Integer tag, Integer num, Integer tagEnd,
                        String type, String description, String format,
                        EvioDictionaryEntry parentEntry) {

        boolean isRange = true;

        if (tagEnd == null || tagEnd.equals(tag) || tagEnd <= 0) {
            // If both values equal each other or tagEnd <= 0, there is no range.
            this.tag    = tag;
            this.tagEnd = 0;
            isRange = false;
        }
        else if (tagEnd < tag) {
            // Switch things so tag < tagEnd for simplicity
            this.tag = tagEnd;
            this.tagEnd = tag;
        }
        else {
            this.tag    = tag;
            this.tagEnd = tagEnd;
        }

        this.num = num;
        this.format = format;
        this.description = description;

        if (type != null) {
            try {
                this.type = DataType.valueOf(type.toUpperCase());
            }
            catch (IllegalArgumentException e) {
            }
        }

        if (!isRange) {
            if (num != null) {
                entryType = EvioDictionaryEntryType.TAG_NUM;
            }
            else {
                entryType = EvioDictionaryEntryType.TAG_ONLY;
            }
        }
        else {
            entryType = EvioDictionaryEntryType.TAG_RANGE;
        }

        this.parentEntry = parentEntry;
    }


    /**
     * Is the given tag within the specified range (inclusive) of this dictionary entry?
     * @since 5.2
     * @param tagArg  tag to compare with range
     * @return {@code false} if tag not in range, else {@code true}.
     */
    public boolean inRange(int tagArg) {
        return tagEnd != 0 && tagArg >= tag && tagArg <= tagEnd;
    }


    /**
     * Is the given dictionary entry's tag within the specified range
     * (inclusive) of this dictionary entry?
     * @since 5.2
     * @param entry  dictionary entry to compare with range
     * @return {@code false} if tag not in range, else {@code true}.
     */
    public boolean inRange(EvioDictionaryEntry entry) {
        return entry != null && tagEnd != 0 && entry.tag >= tag && entry.tag <= tagEnd;
    }


    /** {@inheritDoc}. Algorithm suggested by Joshua Block in "Effective Java". */
    public int hashCode() {
        int hash = 17;
        hash = hash * 31 + tag;
        hash = hash * 31 + tagEnd;
        if (num != null) hash = hash * 31 + num;
        // Don't include parent tag/num/tagEnd in hash since we want
        // entries of identical tag/num/tagEnd but with different
        // parents to hash to the same value.
        // That way we can have an entry that has no parent be
        // equal to one that does which is important if no other
        // matches exist.
        return hash;
    }


    /** {@inheritDoc} */
    public final boolean equals(Object other) {

        if (other == this) return true;
        if (other == null || getClass() != other.getClass()) return false;

        // Objects equal each other if tag & num & tagEnd are the same
        EvioDictionaryEntry otherEntry  = (EvioDictionaryEntry) other;
        EvioDictionaryEntry otherParent = otherEntry.getParentEntry();

        boolean match = tag.equals(otherEntry.tag);

        if (num == null) {
            if (otherEntry.num != null) {
                return false;
            }
        }
        else {
            match = match && num.equals(otherEntry.num);
        }

        // Now check tag range if any
        match = match && tagEnd.equals(otherEntry.tagEnd);

        // Now check if same entry type
        match = match && entryType.equals(otherEntry.entryType);

        // If both parent containers are defined, use them as well
        if (parentEntry != null && otherParent != null) {
            Integer pNum = parentEntry.getNum();
            match = match && parentEntry.getTag().equals(otherParent.getTag());
            if (pNum == null) {
                if (otherParent.getNum() != null) {
                    return false;
                }
            }
            else {
                match = match && pNum.equals(otherParent.getNum());
            }
            match = match && parentEntry.getTagEnd().equals(otherParent.getTagEnd());

        }

        return match;
    }


    /** {@inheritDoc} */
    public final String toString() {
        StringBuilder builder = new StringBuilder(60);

        switch (entryType) {
            case TAG_NUM:
                builder.append("(tag=");
                builder.append(tag);
                builder.append(",num =");
                builder.append(num);
                builder.append(")");
                break;
            case TAG_ONLY:
                builder.append("(tag=");
                builder.append(tag);
                builder.append(")");
                break;
            case TAG_RANGE:
                builder.append("(tag=");
                builder.append(tag);
                builder.append("-");
                builder.append(tagEnd);
                builder.append(")");
        }

        return builder.toString();
    }


    /**
     * Get the tag value.
     * This is the low end of a tag range if tagEnd &gt; 0.
     * @return tag value.
     */
    public final Integer getTag() {return tag;}

    /**
     * Get the tagEnd value (upper end of a tag range).
     * A value of 0 means there is no range.
     * @return tagEnd value.
     */
    public final Integer getTagEnd() {return tagEnd;}

    /**
     * Get the num value.
     * @return num value, null if nonexistent.
     */
    public final Integer getNum() {return num;}

    /**
     * Get the data's type.
     * @return data type object, null if nonexistent.
     */
    public final DataType getType() {return type;}

    /**
     * Get the CompositeData's format.
     * @return CompositeData's format, null if nonexistent.
     */
    public final String getFormat() {return format;}

    /**
     * Get the CompositeData's description.
     * @return CompositeData's description, null if nonexistent.
     */
    public final String getDescription() {return description;}

    /**
     * Get this entry's type.
     * @return this entry's type.
     */
    public EvioDictionaryEntryType getEntryType() {return entryType;}

    /**
     * Get the parent container's dictionary entry.
     * @return the parent container's dictionary entry, null if nonexistent.
     */
    public EvioDictionaryEntry getParentEntry() {return parentEntry;}

}
