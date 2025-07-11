package org.jlab.coda.jevio;

import java.math.BigInteger;
import java.nio.*;
import java.nio.charset.*;
import java.util.*;
import java.io.UnsupportedEncodingException;
import java.io.StringWriter;

import javax.swing.tree.MutableTreeNode;
import javax.swing.tree.TreeNode;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import javax.xml.stream.XMLOutputFactory;

import static org.jlab.coda.jevio.DataType.COMPOSITE;

/**
 * This is the base class for all evio structures: Banks, Segments, and TagSegments.
 * It implements <code>MutableTreeNode</code> because a tree representation of
 * events is created when a new event is parsed.<p>
 * Note that using an EventBuilder for the same event in more than one thread
 * can cause problems. For example the boolean lengthsUpToDate in this class
 * would need to be volatile.
 * 
 * @author heddle
 * @author timmer - add byte order tracking, make setAllHeaderLengths more efficient
 *
 */
public abstract class BaseStructure implements Cloneable, IEvioStructure, MutableTreeNode, IEvioWriter  {

	/** Holds the header of the bank. */
	protected BaseStructureHeader header;

    /**
     * The raw data of the structure.
     * Storing data as raw bytes limits the # of data elements to
     * Integer.MAX_VALUE/size_of_data_type_in_bytes.
     */
    protected byte rawBytes[];

	/** Used if raw data should be interpreted as ints. */
	protected int intData[];

	/** Used if raw data should be interpreted as longs. */
	protected long longData[];

	/** Used if raw data should be interpreted as shorts. */
	protected short shortData[];

	/** Used if raw data should be interpreted as doubles. */
	protected double doubleData[];

    /** Used if raw data should be interpreted as floats. */
    protected float floatData[];

    /** Used if raw data should be interpreted as composite type. */
    protected CompositeData[] compositeData;

    /**
     * Used if raw data should be interpreted as chars.
     * The reason rawBytes is not used directly is because
     * it may be padded and it may not, depending on whether
     * this object was created by EvioReader or by EventBuilder, etc., etc.
     * We don't want to return rawBytes when a user calls getByteData() if there
     * are padding bytes in it.
     */
    protected byte charData[];

    //------------------- STRING STUFF -------------------

    /** Used if raw data should be interpreted as a string. */
    protected StringBuilder stringData;

    /** Used if raw data should be interpreted as a string. */
    protected ArrayList<String> stringsList;

    /**
     * Keep track of end of the last string added to stringData
     * (including null but not padding).
     */
    protected int stringEnd;

    /**
     * True if char data has non-ascii or non-printable characters,
     * or has too little data to be in proper format.
     */
    protected boolean badStringFormat;

    //----------------------------------------------------

    /**
     * The number of stored data items like number of banks, ints, floats, etc.
     * (not the size in ints or bytes). Some items may be padded such as shorts
     * and bytes. This will tell the meaningful number of such data items.
     * In the case of containers, returns number of bytes not in header.
     */
    protected int numberDataItems;

    /** Bytes with which to pad short and byte data. */
    final private static byte[] padValues = {0,0,0};

    /** Number of bytes to pad short and byte data. */
    final private static int[]  padCount  = {0,3,2,1};

    /** Give the XML output proper indentation. */
    protected String xmlIndent = "";

    /** Name of this object as an XML element. */
    protected String xmlElementName = "unknown";   //TODO: get rid of this init val

    /** Name of this object's contents as an XML attribute if it is a structure type. */
    protected String xmlContentAttributeName = "unknown32";  //TODO: get rid of this init val

    /**
     * Endianness of the raw data if appropriate,
     * either {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}.
     */
    protected ByteOrder byteOrder;

	/**
	 * The parent of the structure. If it is an "event", the parent is null.
     * This is used for creating trees for a variety of purposes
     * (not necessarily graphical.)
	 */
	private BaseStructure parent;

	/**
	 * Holds the children of this structure. This is used for creating trees
     * for a variety of purposes (not necessarily graphical.)
	 */
	protected ArrayList<BaseStructure> children;

    /** Keep track of whether header length data is up-to-date or not. */
    protected boolean lengthsUpToDate;

    /** Is this structure a leaf? Leaves are structures with no children. */
    protected boolean isLeaf = true;


    /**
	 * Constructor using a provided header
	 * 
	 * @param header the header to use.
	 * @see BaseStructureHeader
	 */
	public BaseStructure(BaseStructureHeader header) {
		this.header = header;
        // by default we're big endian since this is java
        byteOrder = ByteOrder.BIG_ENDIAN;
        setXmlNames();
	}

    /**
     * This method does a partial copy and is designed to help convert
     * between banks, segments,and tagsegments in the {@link StructureTransformer}
     * class (hence the name "transform").
     * It copies all the data from another BaseStructure object.
     * Children are <b>not</b> copied in the deep clone way,
     * but their references are added to this structure.
     * It does <b>not</b> copy header data or the parent either.
     * Keeps track of the padding and set its value in this structure's header once found.
     * This needs to be calculated since the BaseStructure arg may be a tagsegment which
     * has no associate padding data.
     *
     * @param structure BaseStructure from which to copy data.
     */
    public void transform(BaseStructure structure) {

        if (structure == null) return;

        // reinitialize this base structure first
        rawBytes      = null;
        charData      = null;
        intData       = null;
        longData      = null;
        shortData     = null;
        doubleData    = null;
        floatData     = null;
        compositeData = null;
        stringData    = null;
        stringsList   = null;
        stringEnd     = 0;

        if (children != null) {
            children.clear();
        }
        else {
            children = new ArrayList<BaseStructure>(10);
        }

        // copy over some stuff from other structure
        isLeaf = structure.isLeaf;
        lengthsUpToDate = structure.lengthsUpToDate;
        byteOrder = structure.byteOrder;
        numberDataItems = structure.numberDataItems;

        xmlIndent = structure.xmlIndent;
        xmlElementName = structure.xmlElementName;
        xmlContentAttributeName =  structure.xmlContentAttributeName;

        if (structure.getRawBytes() != null) {
            setRawBytes(structure.getRawBytes().clone());
        }

        DataType type = structure.getHeader().getDataType();

        // Keep track of the padding and set its value in this structure's header once found.
        // This needs to be calculated since the BaseStructure arg may be a tagsegment which
        // has no associate padding data.
        // Padding is only used for the small primitive types: shorts and bytes. Strings are
        // stored in a format that takes care of its own padding and composite data is a
        // container which by definition has no padding associated with it.
        header.padding = 0;

        switch (type)  {
            case SHORT16:
            case USHORT16:
                if (structure.shortData != null) {
                    shortData = structure.shortData.clone();
                    if (structure.shortData.length % 2 != 0) {
                        header.padding = 2;
                    }
                }
                break;

            case INT32:
            case UINT32:
                if (structure.intData != null) {
                    intData = structure.intData.clone();
                }
                break;

            case LONG64:
            case ULONG64:
                if (structure.longData != null) {
                    longData = structure.longData.clone();
                }
                break;

            case FLOAT32:
                if (structure.floatData != null) {
                    floatData = structure.floatData.clone();
                }
                break;

            case DOUBLE64:
                if (structure.doubleData != null) {
                    doubleData = structure.doubleData.clone();
                }
                break;

            case UNKNOWN32:
            case CHAR8:
            case UCHAR8:
                if (structure.charData != null) {
                    charData = structure.charData.clone();
                    header.padding = padCount[structure.charData.length % 4];
                }
                break;

            case CHARSTAR8:
                if (structure.stringsList != null) {
                    stringsList = new ArrayList<String>(structure.stringsList.size());
                    stringsList.addAll(structure.stringsList);
                    stringData = new StringBuilder(structure.stringData);
                    stringEnd = structure.stringEnd;
                }
                break;

            case COMPOSITE:
                if (structure.compositeData != null) {
                    int len = structure.compositeData.length;
                    compositeData = new CompositeData[len];
                    for (int i=0; i < len; i++) {
                        compositeData[i] = (CompositeData) structure.compositeData[i].clone();
                    }
                }
                break;

            case ALSOBANK:
            case ALSOSEGMENT:
            case BANK:
            case SEGMENT:
            case TAGSEGMENT:
                for (BaseStructure kid : structure.children) {
                    children.add(kid);
                }
                break;

            default:
        }
    }

//    static public BaseStructure copy(BaseStructure structure) {
//        return (BaseStructure)structure.clone();
//    }

    /**
     * Clone this object. First call the Object class's clone() method
     * which creates a bitwise copy. Then clone all the mutable objects
     * so that this method does a safe (not deep) clone. This means all
     * children get cloned as well.
     */
    public Object clone() {
        try {
            // "bs" is a bitwise copy. Do a deep clone of all object references.
            BaseStructure bs = (BaseStructure)super.clone();

            // Clone the header
            bs.header = (BaseStructureHeader) header.clone();

            // Clone raw bytes
            if (rawBytes != null) {
                bs.rawBytes = rawBytes.clone();
            }

            // Clone data
            switch (header.getDataType())  {
                case SHORT16:
                case USHORT16:
                    if (shortData != null) {
                        bs.shortData = shortData.clone();
                    }
                    break;

                case INT32:
                case UINT32:
                    if (intData != null) {
                        bs.intData = intData.clone();
                    }
                    break;

                case LONG64:
                case ULONG64:
                    if (longData != null) {
                        bs.longData = longData.clone();
                    }
                    break;

                case FLOAT32:
                    if (floatData != null) {
                        bs.floatData = floatData.clone();
                    }
                    break;

                case DOUBLE64:
                    if (doubleData != null) {
                        bs.doubleData = doubleData.clone();
                    }
                    break;

                case UNKNOWN32:
                case CHAR8:
                case UCHAR8:
                    if (charData != null) {
                        bs.charData = charData.clone();
                    }
                    break;

                case CHARSTAR8:
                    if (stringsList != null) {
                        bs.stringsList = new ArrayList<String>(stringsList.size());
                        bs.stringsList.addAll(stringsList);
                        bs.stringData = new StringBuilder(stringData);
                        bs.stringEnd  = stringEnd;
                    }
                    break;

                case COMPOSITE:
                    if (compositeData != null) {
                        int len = compositeData.length;
                        bs.compositeData = new CompositeData[len];
                        for (int i=0; i < len; i++) {
                            bs.compositeData[i] = (CompositeData) compositeData[i].clone();
                        }
                    }
                    break;

                case ALSOBANK:
                case ALSOSEGMENT:
                case BANK:
                case SEGMENT:
                case TAGSEGMENT:
                    // Create kid storage since we're a container type
                    bs.children = new ArrayList<BaseStructure>(10);

                    // Clone kids
                    for (BaseStructure kid : children) {
                        bs.children.add((BaseStructure)kid.clone());
                    }
                    break;

                default:
            }

            // Do NOT clone the parent, just keep it as a reference.

            return bs;
        }
        catch (CloneNotSupportedException e) {
            return null;
        }
    }

    /** Clear all existing data from a non-container structure. */
    private void clearData() {

        if (header.getDataType().isStructure()) return;

        rawBytes        = null;
        charData        = null;
        intData         = null;
        longData        = null;
        shortData       = null;
        doubleData      = null;
        floatData       = null;
        compositeData   = null;
        stringData      = null;
        stringsList     = null;
        stringEnd       = 0;
        numberDataItems = 0;
    }

	/**
	 * A convenience method use instead of "instanceof" to see what type
     * of structure we have. Note: this returns the type of this structure,
     * not the type of data (the content type) this structure holds.
	 * 
	 * @return the <code>StructureType</code> of this structure.
	 * @see StructureType
	 */
	public abstract StructureType getStructureType();

    /**
     * This is a method that must be filled in by subclasses.
     * Write this structure out as an XML record.
     *
     * @param xmlWriter the writer used to write the events.
     *                  It is tied to an open file.
     */
    public abstract void toXML(XMLStreamWriter xmlWriter);

    /**
     * This is a method that must be filled in by subclasses.
     * Write this structure out as an XML record.
     *
     * @param xmlWriter the writer used to write the events.
     *                  It is tied to an open file.
     * @param hex       if true, ints get displayed in hexadecimal
     */
    public abstract void toXML(XMLStreamWriter xmlWriter, boolean hex);

	/**
	 * Get the element name for the structure for writing to XML.
	 * @return the element name for the structure for writing to XML.
	 */
	public abstract String getXMLElementName();

    /**
     * Write this structure out as an XML format string.
     * @return this structure as an XML format string.
     */
    public String toXML() {
        return toXML(false);
    }

    /**
     * Write this structure out as an XML format string.
     * @param hex if true, ints get displayed in hexadecimal
     * @return this structure as an XML format string.
     */
    public String toXML(boolean hex) {
        try {
            StringWriter sWriter = new StringWriter();
            XMLStreamWriter xmlWriter = XMLOutputFactory.newInstance().createXMLStreamWriter(sWriter);

            toXML(xmlWriter, hex);
            return sWriter.toString();
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * Set the name of this object's XML element and also content type attribute if content is structure type.
     */
    protected void setXmlNames() {

        // is this structure holding a structure type container?
        boolean holdingStructureType = false;

        // type of data held by this container type
        DataType type = header.getDataType();
        if (type == null) return;

        switch (type) {
            case CHAR8:
                xmlElementName = "int8"; break;
            case UCHAR8:
                xmlElementName = "uint8"; break;
            case SHORT16:
                xmlElementName = "int16"; break;
            case USHORT16:
                xmlElementName = "uint16"; break;
            case INT32:
                xmlElementName = "int32"; break;
            case UINT32:
                xmlElementName = "uint32"; break;
            case LONG64:
                xmlElementName = "int64"; break;
            case ULONG64:
                xmlElementName = "uint64"; break;
            case FLOAT32:
                xmlElementName = "float32"; break;
            case DOUBLE64:
                xmlElementName = "float64"; break;
            case CHARSTAR8:
                xmlElementName = "string"; break;
            case COMPOSITE:
                xmlElementName = "composite"; break;
            case UNKNOWN32:
                xmlElementName = "unknown32";
                break;

            case TAGSEGMENT:
//            case ALSOTAGSEGMENT:
                holdingStructureType = true;
                xmlContentAttributeName = "tagsegment";
                break;

            case SEGMENT:
            case ALSOSEGMENT:
                holdingStructureType = true;
                xmlContentAttributeName = "segment";
                break;

            case BANK:
            case ALSOBANK:
                holdingStructureType = true;
                xmlContentAttributeName = "bank";
                break;

            default:
                xmlElementName = "unknown"; break;
        }

        if (holdingStructureType) {
            // Which container type are we (NOT what are we holding)?
            // Because that determines our element name.
            StructureType structType = getStructureType();
            if (structType == StructureType.UNKNOWN32) {
                xmlElementName = "unknown32";
            }
            else {
                xmlElementName = getXMLElementName();
            }
        }

    }

    /**
     * Set the indentation (string of spaces) for more pleasing XML output.
     * @param s the indentation (string of spaces) for more pleasing XML output
     */
    protected void setXmlIndent(String s) {
        xmlIndent = s;
    }

    /**
     * Increase the indentation (string of spaces) of the XML output.
     */
    protected void increaseXmlIndent() {
        xmlIndent += "   ";
    }

    /**
     * Decrease the indentation (string of spaces) of the XML output.
     */
    protected void decreaseXmlIndent() {
        xmlIndent = xmlIndent.substring(0, xmlIndent.length() - 3);
    }

    /**
     * What is the byte order of this data?
     * @return {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     */
    public ByteOrder getByteOrder() {
        return byteOrder;
    }

    /**
     * Set the byte order of this data. This method <b>cannot</b> be used to swap data.
     * It is only used to describe the endianness of the rawdata contained.
     * @param byteOrder {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
     */
    public void setByteOrder(ByteOrder byteOrder) {
        this.byteOrder = byteOrder;
    }

	/**
	 * Is a byte swap required? This is java and therefore big endian. If data is
     * little endian, then a swap is required.
	 * 
	 * @return <code>true</code> if byte swapping is required (data is little endian).
	 */
	public boolean isSwap() {
		return byteOrder == ByteOrder.LITTLE_ENDIAN;
	}

	/**
	 * Get the description from the name provider (dictionary), if there is one.
	 * 
	 * @return the description from the name provider (dictionary), if there is one. If not, return
	 *         NameProvider.NO_NAME_STRING.
	 */
	public String getDescription() {
		return NameProvider.getName(this);
	}


    /**
     * Obtain a string representation of the structure.
     * This string is used when displaying the object as a node in JTree widget.
     *
     * @return a string representation of the structure.
     */
    public String toString() {
        StructureType stype = getStructureType();
        DataType dtype = header.getDataType();

        String description = getDescription();
        if (INameProvider.NO_NAME_STRING.equals(description)) {
            description = null;
        }

        StringBuilder sb = new StringBuilder(100);
        if (description != null) {
            sb.append("<html><b>");
            sb.append(description);
            sb.append("</b>");
        }
        else {
            sb.append(stype);
            sb.append(" of ");
            sb.append(dtype);
            sb.append("s:  tag=");
            sb.append(header.tag);
            sb.append("(0x");
            sb.append(Integer.toHexString(header.tag));

            sb.append(")");
            if (this instanceof EvioBank) {
                sb.append("  num=");
                sb.append(header.number);
                sb.append("(0x");
                sb.append(Integer.toHexString(header.number));
                sb.append(")");
            }
        }


        if (rawBytes == null) {
            sb.append("  dataLen=");
            sb.append(header.getDataLength());
        }
        else {
            sb.append("  dataLen=");
            sb.append(rawBytes.length/4);
        }

        if (header.padding != 0) {
            sb.append("  pad=");
            sb.append(header.padding);
        }

        int numChildren = (children == null) ? 0 : children.size();

        if (numChildren > 0) {
            sb.append("  children=");
            sb.append(numChildren);
        }

        if (description != null) {
            sb.append("</html>");
        }

        return sb.toString();
    }


    /**
     * Recursive method to obtain a string representation of the entire tree structure
     * rooted at this structure.
     * @param indent string containing indentation for this structure. Generally called with "".
     * @return a string representation of the entire tree structure rooted at this structure.
     */
    public String treeToString(String indent) {
        StringBuilder sb = new StringBuilder(100);
        sb.append(indent);
        sb.append(toString());

        if (!(isLeaf())) {
            sb.append("\n");
            String myIndent = indent + "  ";
            int childCount = getChildCount();
            for (int i=0; i < childCount; i++) {
                sb.append(children.get(i).treeToString(myIndent));
                if (i < childCount - 1) {
                    sb.append("\n");
                }
            }
        }
        return sb.toString();
    }


    /**
	 * This is a method from the IEvioStructure Interface. Return the header for this structure.
	 * 
	 * @return the header for this structure.
	 */
    public BaseStructureHeader getHeader() {
		return header;
	}

    /**
     * Get the number of stored data items like number of banks, ints, floats, etc.
     * (not the size in ints or bytes). Some items may be padded such as shorts
     * and bytes. This will tell the meaningful number of such data items.
     * In the case of containers, returns number of 32-bit words not in header.
     *
     * @return number of stored data items (not size or length),
     *         or number of bytes if container
     */
    public int getNumberDataItems() {
        if (isContainer()) {
            numberDataItems = header.getLength() + 1 - header.getHeaderLength();
        }

        // if the calculation has not already been done ...
        if (numberDataItems < 1) {
            // When parsing a file or byte array, it is not fully unpacked until data
            // is asked for specifically, for example as an int array or a float array.
            // Thus we don't know how many of a certain item (say doubles) there is.
            // But we can figure that out now based on the size of the raw data byte array.
            int divisor = 0;
            int padding = 0;
            DataType type = header.getDataType();

            switch (type) {
                case UNKNOWN32:
                case CHAR8:
                case UCHAR8:
                    padding = header.getPadding();
                    divisor = 1; break;
                case SHORT16:
                case USHORT16:
                    padding = header.getPadding();
                    divisor = 2; break;
                case INT32:
                case UINT32:
                case FLOAT32:
                    divisor = 4; break;
                case LONG64:
                case ULONG64:
                case DOUBLE64:
                    divisor = 8; break;
                case CHARSTAR8:
                    String[] s = getStringData();
                    if (s ==  null) {
                        numberDataItems = 0;
                        break;
                    }
                    numberDataItems = s.length;
                    break;
                case COMPOSITE:
                    // For this type, numberDataItems is NOT used to
                    // calculate the data length so we're OK returning
                    // any reasonable value here.
                    numberDataItems = 1;
                    if (compositeData != null) {
                        numberDataItems = compositeData.length;
                    }
                    else {
                        try {
                            CompositeData[] d = getCompositeData();
                            if (d != null) {
                                numberDataItems = d.length;
                            }
                        }
                        catch (EvioException e) {/* nothing more can be done */}
                    }
                    break;
                default:
            }

            if (divisor > 0 && rawBytes != null) {
                numberDataItems = (rawBytes.length - padding)/divisor;
            }
        }

        return numberDataItems;
    }

    /**
     * Get the length of this structure in bytes, including the header.
     * @return the length of this structure in bytes, including the header.
     */
    public int getTotalBytes() {
        return 4*(header.getLength() + 1);
    }

	/**
	 * Get the raw data of the structure.
	 * Warning, there are circumstances in which rawBytes is null.
	 * For example if this structure contains other structures.
	 * @return the raw data of the structure.
	 */
	public byte[] getRawBytes() {
		return rawBytes;
	}

	/**
	 * Set the data for the structure.
	 * 
	 * @param rawBytes the structure raw data.
	 */
	public void setRawBytes(byte[] rawBytes) {
		this.rawBytes = rawBytes;
	}

	/**
	 * This is a method from the IEvioStructure Interface. Gets the raw data as an integer array,
     * if the content type as indicated by the header is appropriate.<p>
     * NOTE: since Java does not have unsigned primitives, both INT32 and UINT32
	 * data types will be returned as int arrays. The application will have to deal
     * with reinterpreting signed ints that are negative as unsigned ints.
	 * 
	 * @return the data as an int array, or <code>null</code> if this makes no sense for the given content type.
	 */
	public int[] getIntData() {

        switch (header.getDataType()) {
            case INT32:
            case UINT32:
                if (intData == null) {
                    if (rawBytes == null) {
                        return null;
                    }
                    try {
                        intData = ByteDataTransformer.toIntArray(rawBytes, byteOrder);
                    }
                    catch (EvioException e) {/* should not happen */}
                }
                return intData;
            default:
                return null;
        }
	}

	/**
	 * This is a method from the IEvioStructure Interface. Gets the raw data as a long array,
     * if the content type as indicated by the header is appropriate.<p>
     * NOTE: since Java does not have unsigned primitives, both LONG64 and ULONG64
     * data types will be returned as long arrays. The application will have to deal
     * with reinterpreting signed longs that are negative as unsigned longs.
	 * 
	 * @return the data as a long array, or <code>null</code> if this makes no sense for the given content type.
	 */
	public long[] getLongData() {

        switch (header.getDataType()) {
            case LONG64:
            case ULONG64:
                if (longData == null) {
                    if (rawBytes == null) {
                        return null;
                    }
                    try {
                        longData = ByteDataTransformer.toLongArray(rawBytes, byteOrder);
                    }
                    catch (EvioException e) {/* should not happen */}
                }
                return longData;
            default:
                return null;
        }
	}

	/**
	 * This is a method from the IEvioStructure Interface. Gets the raw data as a float array,
     * if the content type as indicated by the header is appropriate.
	 * 
	 * @return the data as a double array, or <code>null</code> if this makes no sense for the given contents type.
	 */
	public float[] getFloatData() {

        switch (header.getDataType()) {
            case FLOAT32:
                if (floatData == null) {
                    if (rawBytes == null) {
                        return null;
                    }
                    try {
                        floatData = ByteDataTransformer.toFloatArray(rawBytes, byteOrder);
                    }
                    catch (EvioException e) {/* should not happen */}
                }
                return floatData;
            default:
                return null;
        }
	}

	/**
	 * This is a method from the IEvioStructure Interface. Gets the raw data as a double array,
     * if the content type as indicated by the header is appropriate.
	 * 
	 * @return the data as a double array, or <code>null</code> if this makes no sense for the given content type.
	 */
	public double[] getDoubleData() {

        switch (header.getDataType()) {
            case DOUBLE64:
                if (doubleData == null) {
                    if (rawBytes == null) {
                        return null;
                    }
                    try {
                        doubleData = ByteDataTransformer.toDoubleArray(rawBytes, byteOrder);
                    }
                    catch (EvioException e) {/* should not happen */}
                }
                return doubleData;
            default:
                return null;
        }
	}

	/**
	 * This is a method from the IEvioStructure Interface. Gets the raw data as a short array,
     * if the contents type as indicated by the header is appropriate.<p>
     * NOTE: since Java does not have unsigned primitives, both SHORT16 and USHORT16
     * data types will be returned as short arrays. The application will have to deal
     * with reinterpreting signed shorts that are negative as unsigned shorts.
	 *
	 * @return the data as a short array, or <code>null</code> if this makes no sense for the given contents type.
	 */
	public short[] getShortData() {

        switch (header.getDataType()) {
            case SHORT16:
            case USHORT16:
                if (shortData == null) {
                    if (rawBytes == null) {
                        return null;
                    }
                    try {
                        shortData = ByteDataTransformer.toShortArray(rawBytes, header.getPadding(), byteOrder);
                    }
                    catch (EvioException e) {/* should not happen */}
                }
                return shortData;
            default:
                return null;
        }
	}

    /**
     * This is a method from the IEvioStructure Interface. Gets the composite data as
     * an array of CompositeData objects, if the content type as indicated by the header
     * is appropriate.<p>
     *
     * @return the data as an array of CompositeData objects, or <code>null</code>
     *         if this makes no sense for the given content type.
     * @throws EvioException if the data is internally inconsistent
     */
    public CompositeData[] getCompositeData() throws EvioException {

        switch (header.getDataType()) {
            case COMPOSITE:
                if (compositeData == null) {
                    if (rawBytes == null) {
                        return null;
                    }
                    compositeData = CompositeData.parse(rawBytes, byteOrder);
                }
                return compositeData;
            default:
                return null;
        }
    }

	/**
	 * This is a method from the IEvioStructure Interface. Gets the raw data as an byte array,
     * if the contents type as indicated by the header is appropriate.<p>
     * NOTE: since Java does not have unsigned primitives, CHAR8 and UCHAR8
	 * data types will be returned as byte arrays. The application will have to deal
     * with reinterpreting bytes as characters, if necessary.
	 * 
	 * @return the data as an byte array, or <code>null</code> if this makes no sense for the given contents type.
	 */
	public byte[] getByteData() {

        switch (header.getDataType()) {
            case CHAR8:
            case UCHAR8:
                if (charData == null) {
                    if (rawBytes == null) {
                        return null;
                    }
                    // Get rid of padding on end, if any.
                    charData = new byte[rawBytes.length - header.getPadding()];
                    System.arraycopy(rawBytes, 0, charData, 0, rawBytes.length - header.getPadding());
                }
                return charData;
            default:
                return null;
        }
	}

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data (ascii) as an
     * array of String objects, if the contents type as indicated by the header is appropriate.
     * For any other behavior, the user should retrieve the data as a byte array and
     * manipulate it in the exact manner desired. If there are non ascii or non-printing ascii
     * chars or the bytes or not in evio format, a single String containing everything is returned.<p>
     *
     * Originally, in evio versions 1, 2 and 3, only one string was stored. Recent changes allow
     * an array of strings to be stored and retrieved. The changes are backwards compatible.
     *
     * The following is true about the string raw data format:
     * <ul>
     * <li>All strings are immediately followed by an ending null (0).
     * <li>All string arrays are further padded/ended with at least one 0x4 valued ASCII
     *     char (up to 4 possible).
     * <li>The presence of 1 to 4 ending 4's distinguishes the recent string array version from
     *     the original, single string version.
     * <li>The original version string may be padded with anything after its ending null.
     * </ul>
     *
     * @return the data as an array of String objects if DataType is CHARSTAR8, or <code>null</code>
     *         if this makes no sense for the given type.
     *
     */
    public String[] getStringData() {

        switch (header.getDataType()) {
        case CHARSTAR8:

            if (stringsList != null) {
                return stringsList.toArray(new String[stringsList.size()]);
            }

            if (stringData == null && rawBytes == null) {
                return null;
            }

            int stringCount = unpackRawBytesToStrings();
            if (stringCount < 1) return null;

            return stringsList.toArray(new String[stringsList.size()]);

        default:
            return null;
        }
    }


    /**
     * This method returns the number of bytes in a raw
     * evio format of the given string array, not including header.
     *
     * @param strings array of String objects to size
     * @return the number of bytes in a raw evio format of the given string array,
     *         or 0 if arg is null or has zero length.
     */
    static public int stringsToRawSize(String[] strings) {

        if (strings == null || strings.length < 1) {
            return 0;
        }

        int dataLen = 0;
        for (String s : strings) {
            dataLen += s.length() + 1; // don't forget the null
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int[] pads = {4,3,2,1};
        dataLen += pads[dataLen%4];

        return dataLen;
    }


    /**
     * This method transforms an array of Strings into raw evio format data,
     * not including header.
     *
     * @param strings array of String objects to transform
     * @return byte array containing evio format string array,
     *         or null if arg is null or has zero length.
     */
    static public byte[] stringsToRawBytes(String[] strings) {

        if (strings == null || strings.length < 1) {
            return null;
        }

        // create some storage
        int dataLen = 0;
        for (String s : strings) {
            dataLen += s.length() + 1; // don't forget the null
        }
        dataLen += 4; // allow room for maximum padding of 4's
        StringBuilder stringData = new StringBuilder(dataLen);

        for (String s : strings) {
            // add string
            stringData.append(s);
            // add ending null
            stringData.append('\000');
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int[] pads = {4,3,2,1};
        switch (pads[stringData.length()%4]) {
            case 4:
                stringData.append("\004\004\004\004");
                break;
            case 3:
                stringData.append("\004\004\004");
                break;
            case 2:
                stringData.append("\004\004");
                break;
            case 1:
                stringData.append('\004');
            default:
        }

        // set raw data
        try {
            return stringData.toString().getBytes("US-ASCII");
        }
        catch (UnsupportedEncodingException e) { /* never happen */ }

        return null;
    }


    /**
     * This method extracts an array of strings from byte array of raw evio string data.
     *
     * @param rawBytes raw evio string data
     * @param offset offset into raw data array
     * @return array of Strings or null if bad arg or too little data
     */
    static public String[] unpackRawBytesToStrings(byte[] rawBytes, int offset) {
        return unpackRawBytesToStrings(rawBytes, offset, rawBytes.length);
    }


    /**
     * This method extracts an array of strings from byte array of raw evio string data.
     * Don't go beyond the specified max character limit and stop at the first
     * non-character value.
     *
     * @param rawBytes    raw evio string data
     * @param offset      offset into raw data array
     * @param maxLength   max length in bytes of valid data in rawBytes array
     * @return array of Strings or null if bad arg or too little data
     */
    static public String[] unpackRawBytesToStrings(byte[] rawBytes, int offset, int maxLength) {

        if (rawBytes == null) return null;

        int length = rawBytes.length - offset;
        if (offset < 0 || length < 4) return null;

        // Don't read read more than maxLength ASCII characters
        length = length > maxLength ? maxLength : length;

        StringBuilder stringData = null;
        try {
            stringData = new StringBuilder(new String(rawBytes, offset, length, "US-ASCII"));
        }
        catch (UnsupportedEncodingException e) { /* will never happen */ }

        return stringBuilderToStrings(stringData, true);
    }


    /**
     * This method extracts an array of strings from buffer containing raw evio string data.
     *
     * @param buffer  buffer containing evio string data
     * @param pos     position of string data in buffer
     * @param length  length of string data in buffer in bytes
     * @return array of Strings or null if bad arg or too little data
     */
    static public String[] unpackRawBytesToStrings(ByteBuffer buffer, int pos, int length) {

        if (buffer == null || pos < 0 || length < 4) return null;

        ByteBuffer stringBuf = buffer.duplicate();
        stringBuf.limit(pos+length).position(pos);

        StringBuilder stringData = null;
        try {
            CharsetDecoder decoder = Charset.forName("US-ASCII").newDecoder();
            stringData = new StringBuilder(decoder.decode(stringBuf));
        }
        catch (CharacterCodingException e) {/* will not happen */}

        return stringBuilderToStrings(stringData, false);
    }


    /**
     * This method extracts an array of strings from StringBuilder containing string data.
     * If non-printable chars are found (besides those used to terminate strings),
     * then 1 string with all characters will be returned. However, if the "onlyGoodChars"
     * flag is true, 1 string is returned in truncated form without
     * the bad characters at the end.
     *
     * @param stringData     buffer containing string data
     * @param onlyGoodChars  if true and non-printable chars found,
     *                       only 1 string with printable ASCII chars will be returned.
     * @return array of Strings or null if processing error
     */
    static private String[] stringBuilderToStrings(StringBuilder stringData,
                                                   boolean onlyGoodChars) {

        // Each string is terminated with a null (char val = 0)
        // and in addition, the end is padded by ASCII 4's (char val = 4).
        // However, in the legacy versions of evio, there is only one
        // null-terminated string and anything as padding. To accommodate legacy evio, if
        // there is not an ending ASCII value 4, anything past the first null is ignored.
        // After doing so, split at the nulls. Do not use the String
        // method "split" as any empty trailing strings are unfortunately discarded.

        char c;
        ArrayList<Integer> nullIndexList = new ArrayList<Integer>(10);
        int nullCount = 0, goodChars = 0;
        boolean badFormat = true;

        int length = stringData.length();
        boolean noEnding4 = false;
        if (stringData.charAt(length - 1) != '\004') {
            noEnding4 = true;
        }

        outerLoop:
        for (int i=0; i < length; i++) {
            c = stringData.charAt(i);

            // If char is a null
            if (c == 0) {
                nullCount++;
                nullIndexList.add(i);
                // If evio v2 or 3, only 1 null terminated string exists
                // and padding is just junk or nonexistent.
                if (noEnding4) {
                    badFormat = false;
                    break;
                }
            }
            // Look for any non-printing/control characters (not including null)
            // and end the string there. Allow tab & newline.
            else if ((c < 32 || c > 126) && c != 9 && c != 10) {
                if (nullCount < 1) {
                    badFormat = true;
                    // Getting garbage before first null.
                    break;
                }

                // Already have at least one null & therefore a String.
                // Now we have junk or non-printing ascii which is
                // possibly the ending 4.

                // If we have a 4, investigate further to see if format
                // is entirely valid.
                if (c == '\004') {
                    // How many more chars are there?
                    int charsLeft = length - (i+1);

                    // Should be no more than 3 additional 4's before the end
                    if (charsLeft > 3) {
                        badFormat = true;
                        break;
                    }
                    else {
                        // Check to see if remaining chars are all 4's. If not, bad.
                        for (int j=1; j <= charsLeft; j++) {
                            c = stringData.charAt(i+j);
                            if (c != '\004') {
                                badFormat = true;
                                break outerLoop;
                            }
                        }
                        badFormat = false;
                        break;
                    }
                }
                else {
                    badFormat = true;
                    break;
                }
            }

            // Number of good ASCII chars we have
            goodChars++;
        }

        if (badFormat) {
            if (onlyGoodChars) {
                // Return everything in one String WITHOUT garbage
                return new String[] {stringData.substring(0, goodChars)};
            }
            // Return everything in one String including possible garbage
            return new String[] {stringData.toString()};
        }

        // If here, raw bytes are in the proper format

        int firstIndex=0, count=0;
        String[] strings = new String[nullCount];
        for (int nullIndex : nullIndexList) {
            strings[count++] = stringData.substring(firstIndex, nullIndex);
            firstIndex = nullIndex + 1;
        }

        return strings;
    }

    /**
     * Extract string data from rawBytes array.
     * @return number of strings extracted from bytes
     */
    private int unpackRawBytesToStrings() {

        badStringFormat = true;

        if (rawBytes == null || rawBytes.length < 4) {
            stringsList = null;
            stringData = null;
            return 0;
        }

        // Each string is terminated with a null (char val = 0)
        // and in addition, the end is padded by ASCII 4's (char val = 4).
        // However, in the legacy versions of evio, there is only one
        // null-terminated string and anything as padding. To accommodate legacy evio, if
        // there is not an ending ASCII value 4, anything past the first null is ignored.
        // After doing so, split at the nulls. Do not use the String
        // method "split" as any empty trailing strings are unfortunately discarded.

        try {
            // stringData contains all elements of rawBytes.
            // Most efficient way to convert bytes to chars!
            stringData = new StringBuilder(new String(rawBytes, "US-ASCII"));
        }
        catch (UnsupportedEncodingException e) {/* will never happen */}

        char c;
        int nullCount = 0;
        stringsList = new ArrayList<String>(10);
        ArrayList<Integer> nullIndexList = new ArrayList<Integer>(10);

        int rawLength = rawBytes.length;
        boolean noEnding4 = false;
        if (rawBytes[rawLength - 1] != 4) {
            noEnding4 = true;
        }

        outerLoop:
        for (int i=0; i < rawLength; i++) {
            c = stringData.charAt(i);

            // If char is a null
            if (c == 0) {
                nullCount++;
                nullIndexList.add(i);
                // If evio v2 or 3, only 1 null terminated string exists
                // and padding is just junk or nonexistent.
                if (noEnding4) {
                    badStringFormat = false;
                    break;
                }
            }
            // Look for any non-printing/control characters (not including null)
            // and end the string there. Allow tab and newline whitespace.
            else if ((c < 32 || c > 126) && c != 9 && c != 10) {
//System.out.println("unpackRawBytesToStrings: found non-printing c = 0x" +
//                           Integer.toHexString((int)c) + " at i = " + i);
                if (nullCount < 1) {
                    // Getting garbage before first null.
//System.out.println("BAD FORMAT 1: garbage char before null");
                    break;
                }

                // Already have at least one null & therefore a String.
                // Now we have junk or non-printing ascii which is
                // possibly the ending 4.

                // If we have a 4, investigate further to see if format
                // is entirely valid.
                if (c == '\004') {
                    // How many more chars are there?
                    int charsLeft = rawLength - (i+1);

                    // Should be no more than 3 additional 4's before the end
                    if (charsLeft > 3) {
//System.out.println("BAD FORMAT 2: too many chars, " + charsLeft + ", after 4");
                        break;
                    }
                    else {
                        // Check to see if remaining chars are all 4's. If not, bad.
                        for (int j=1; j <= charsLeft; j++) {
                            c = stringData.charAt(i+j);
                            if (c != '\004') {
//System.out.println("BAD FORMAT 3: padding chars are not all 4's");
                                break outerLoop;
                            }
                        }
                        badStringFormat = false;
                        break;
                    }
                }
                else {
//System.out.println("BAD FORMAT 4: got bad char, ascii val = " + c);
                    break;
                }
            }
        }

        // What if the raw bytes are all valid ascii with no null or other non-printing chars?
        // Then format is bad so return everything as one string.

        // If error, return everything in one String including possible garbage
        if (badStringFormat) {
//System.out.println("unpackRawBytesToStrings: bad format, return all chars in 1 string");
            stringsList.add(stringData.toString());
            return 1;
        }

        // If here, raw bytes are in the proper format

//System.out.println("  split into " + nullCount + " strings");
        int firstIndex=0;
        for (int nullIndex : nullIndexList) {
            stringsList.add(stringData.substring(firstIndex, nullIndex));
//System.out.println("    add " + stringData.substring(firstIndex, nullIndex));
            firstIndex = nullIndex + 1;
        }

        // Set length of everything up to & including last null (not padding)
        stringEnd = firstIndex;
        stringData.setLength(stringEnd);
//System.out.println("    good string len = " +stringEnd);
        return stringsList.size();
    }

    /**
	 * Get the parent of this structure. The outer event bank is the only structure with a <code>null</code>
     * parent (the only orphan). All other structures have non-null parent giving the container in which they
     * were embedded. Part of the <code>MutableTreeNode</code> interface.
	 * 
	 * @return the parent of this structure.
	 */
	public BaseStructure getParent() {
		return parent;
	}

	/**
	 * Get an enumeration of all the children of this structure. If none, it returns a constant,
     * empty Enumeration. Part of the <code>MutableTreeNode</code> interface.
     * @return enumeration of children nodes
     */
	public Enumeration<? extends TreeNode> children() {
		if (children == null) {
			return Collections.emptyEnumeration();
		}
        return Collections.enumeration(children);
	}

    /**
	 * Add a child at the given index.
	 * 
	 * @param child the child to add.
	 * @param index the target index. Part of the <code>MutableTreeNode</code> interface.
     * @throws IndexOutOfBoundsException if index is too large.
	 */
	public void insert(MutableTreeNode child, int index) {
		if (children == null) {
			children = new ArrayList<BaseStructure>(10);
		}

        MutableTreeNode oldParent = (MutableTreeNode)child.getParent();
        if (oldParent != null) {
            oldParent.remove(child);
        }

        children.add(index, (BaseStructure) child);
        child.setParent(this);
        isLeaf = false;
        lengthsUpToDate(false);
	}

	/**
	 * Convenience method to add a child at the end of the child list. In this application
	 * where are not concerned with the ordering of the children.
     * Each child can only be added once to a parent!
	 * 
	 * @param child the child to add. It will be added to the end of child list.
	 */
	public void insert(MutableTreeNode child) {
        if (children == null) {
            children = new ArrayList<BaseStructure>(10);
        }

        if (child == null) {
            return;
        }

        if (isNodeAncestor(child)) {
            return;
        }

        // Add to end
        if (child.getParent() == this) {
            // If we are adding this potential child to same parent again, the insert(child, index) method will
            // remove it first so each child only has 1 parent! Thus reduce index by 1.
            insert(child, getChildCount() - 1);
        }
        else {
            insert(child, getChildCount());
        }
	}

	/**
	 * Removes the child at index from the receiver. Part of the <code>MutableTreeNode</code> interface.
	 * 
	 * @param index the target index for removal.
	 */
	public void remove(int index) {
        if (children == null) return;
        BaseStructure bs = children.remove(index);
        bs.setParent(null);
        if (children.size() < 1) isLeaf = true;
        lengthsUpToDate(false);
	}

	/**
	 * Removes the child. Part of the <code>MutableTreeNode</code> interface.
	 * 
	 * @param child the child node being removed.
	 */
	public void remove(MutableTreeNode child) {
        if (children == null || child == null) return;
		children.remove(child);
        child.setParent(null);
        if (children.size() < 1) isLeaf = true;
        lengthsUpToDate(false);
	}


    /**
     * Removes all of this node's children, setting their parents to null.
     * If this node has no children, this method does nothing.
     */
    public void removeAllChildren() {
        for (int i = getChildCount()-1; i >= 0; i--) {
            remove(i);
        }
    }

	/**
	 * Remove this node from its parent. Part of the <code>MutableTreeNode</code> interface.
	 */
	public void removeFromParent() {
		if (parent != null) parent.remove(this);
	}

	/**
	 * Set the parent for this node. Part of the <code>MutableTreeNode</code> interface.
	 */
	public void setParent(MutableTreeNode parent) {
		this.parent = (BaseStructure) parent;
	}

    //------------------------------------------------------------------------------------------
    // Stole many of the following methods (some slightly modified) from DefaultMutableTreeNode
    //------------------------------------------------------------------------------------------

    //
    //  Tree Queries
    //

    /**
     * Returns true if <code>anotherNode</code> is an ancestor of this node
     * -- if it is this node, this node's parent, or an ancestor of this
     * node's parent.  (Note that a node is considered an ancestor of itself.)
     * If <code>anotherNode</code> is null, this method returns false.  This
     * operation is at worst O(h) where h is the distance from the root to
     * this node.
     *
     * @see             #isNodeDescendant
     * @see             #getSharedAncestor
     * @param   anotherNode     node to test as an ancestor of this node
     * @return  true if this node is a descendant of <code>anotherNode</code>
     */
    public boolean isNodeAncestor(TreeNode anotherNode) {
        if (anotherNode == null) {
            return false;
        }

        TreeNode ancestor = this;

        do {
            if (ancestor == anotherNode) {
                return true;
            }
        } while((ancestor = ancestor.getParent()) != null);

        return false;
    }

    /**
     * Returns true if <code>anotherNode</code> is a descendant of this node
     * -- if it is this node, one of this node's children, or a descendant of
     * one of this node's children.  Note that a node is considered a
     * descendant of itself.  If <code>anotherNode</code> is null, returns
     * false.  This operation is at worst O(h) where h is the distance from the
     * root to <code>anotherNode</code>.
     *
     * @see     #isNodeAncestor
     * @see     #getSharedAncestor
     * @param   anotherNode     node to test as descendant of this node
     * @return  true if this node is an ancestor of <code>anotherNode</code>
     */
    public boolean isNodeDescendant(BaseStructure anotherNode) {
        if (anotherNode == null)
            return false;

        return anotherNode.isNodeAncestor(this);
    }

    /**
     * Returns the nearest common ancestor to this node and <code>aNode</code>.
     * Returns null, if no such ancestor exists -- if this node and
     * <code>aNode</code> are in different trees or if <code>aNode</code> is
     * null.  A node is considered an ancestor of itself.
     *
     * @see     #isNodeAncestor
     * @see     #isNodeDescendant
     * @param   aNode   node to find common ancestor with
     * @return  nearest ancestor common to this node and <code>aNode</code>,
     *          or null if none
     */
    public BaseStructure getSharedAncestor(BaseStructure aNode) {
        if (aNode == this) {
            return this;
        } else if (aNode == null) {
            return null;
        }

        int             level1, level2, diff;
        BaseStructure   node1, node2;

        level1 = getLevel();
        level2 = aNode.getLevel();

        if (level2 > level1) {
            diff = level2 - level1;
            node1 = aNode;
            node2 = this;
        } else {
            diff = level1 - level2;
            node1 = this;
            node2 = aNode;
        }

        // Go up the tree until the nodes are at the same level
        while (diff > 0) {
            node1 = node1.getParent();
            diff--;
        }

        // Move up the tree until we find a common ancestor.  Since we know
        // that both nodes are at the same level, we won't cross paths
        // unknowingly (if there is a common ancestor, both nodes hit it in
        // the same iteration).

        do {
            if (node1 == node2) {
                return node1;
            }
            node1 = node1.getParent();
            node2 = node2.getParent();
        } while (node1 != null);// only need to check one -- they're at the
        // same level so if one is null, the other is

        if (node1 != null || node2 != null) {
            throw new Error ("nodes should be null");
        }

        return null;
    }


    /**
     * Returns true if and only if <code>aNode</code> is in the same tree
     * as this node.  Returns false if <code>aNode</code> is null.
     *
     * @param   aNode node to find common ancestor with
     * @see     #getSharedAncestor
     * @see     #getRoot
     * @return  true if <code>aNode</code> is in the same tree as this node;
     *          false if <code>aNode</code> is null
     */
    public boolean isNodeRelated(BaseStructure aNode) {
        return (aNode != null) && (getRoot() == aNode.getRoot());
    }


    /**
     * Returns the depth of the tree rooted at this node -- the longest
     * distance from this node to a leaf.  If this node has no children,
     * returns 0.  This operation is much more expensive than
     * <code>getLevel()</code> because it must effectively traverse the entire
     * tree rooted at this node.
     *
     * @see     #getLevel
     * @return  the depth of the tree whose root is this node
     */
    public int getDepth() {
        Object  last = null;
        Enumeration<TreeNode> enum_ =  new BreadthFirstEnumeration(this);

        while (enum_.hasMoreElements()) {
            last = enum_.nextElement();
        }

        if (last == null) {
            throw new Error ("nodes should be null");
        }

        return ((BaseStructure)last).getLevel() - getLevel();
    }



    /**
     * Returns the number of levels above this node -- the distance from
     * the root to this node.  If this node is the root, returns 0.
     *
     * @see     #getDepth
     * @return  the number of levels above this node
     */
    public int getLevel() {
        TreeNode ancestor;
        int levels = 0;

        ancestor = this;
        while((ancestor = ancestor.getParent()) != null){
            levels++;
        }

        return levels;
    }


    /**
     * Returns the path from the root, to get to this node.  The last
     * element in the path is this node.
     *
     * @return an array of TreeNode objects giving the path, where the
     *         first element in the path is the root and the last
     *         element is this node.
     */
    public BaseStructure[] getPath() {
        return getPathToRoot(this, 0);
    }

    /**
     * Builds the parents of node up to and including the root node,
     * where the original node is the last element in the returned array.
     * The length of the returned array gives the node's depth in the
     * tree.
     *
     * @param aNode  the TreeNode to get the path for
     * @param depth  an int giving the number of steps already taken towards
     *        the root (on recursive calls), used to size the returned array
     * @return an array of TreeNodes giving the path from the root to the
     *         specified node
     */
    protected BaseStructure[] getPathToRoot(BaseStructure aNode, int depth) {
        BaseStructure[]              retNodes;

        /* Check for null, in case someone passed in a null node, or
           they passed in an element that isn't rooted at root. */
        if(aNode == null) {
            if(depth == 0)
                return null;
            else
                retNodes = new BaseStructure[depth];
        }
        else {
            depth++;
            retNodes = getPathToRoot(aNode.getParent(), depth);
            retNodes[retNodes.length - depth] = aNode;
        }
        return retNodes;
    }


    /**
     * Returns the root of the tree that contains this node.  The root is
     * the ancestor with a null parent.
     *
     * @see     #isNodeAncestor
     * @return  the root of the tree that contains this node
     */
    public BaseStructure getRoot() {
        BaseStructure ancestor = this;
        BaseStructure previous;

        do {
            previous = ancestor;
            ancestor = ancestor.getParent();
        } while (ancestor != null);

        return previous;
    }


    /**
     * Returns true if this node is the root of the tree.  The root is
     * the only node in the tree with a null parent; every tree has exactly
     * one root.
     *
     * @return  true if this node is the root of its tree
     */
    public boolean isRoot() {
        return getParent() == null;
    }


    /**
     * Returns the node that follows this node in a preorder traversal of this
     * node's tree.  Returns null if this node is the last node of the
     * traversal.  This is an inefficient way to traverse the entire tree; use
     * an enumeration, instead.
     *
     * @return  the node that follows this node in a preorder traversal, or
     *          null if this node is last
     */
    public BaseStructure getNextNode() {
        if (getChildCount() == 0) {
            // No children, so look for nextSibling
            BaseStructure nextSibling = getNextSibling();

            if (nextSibling == null) {
                BaseStructure aNode = getParent();

                do {
                    if (aNode == null) {
                        return null;
                    }

                    nextSibling = aNode.getNextSibling();
                    if (nextSibling != null) {
                        return nextSibling;
                    }

                    aNode = aNode.getParent();
                } while(true);
            } else {
                return nextSibling;
            }
        } else {
            return getChildAt(0);
        }
    }


    /**
     * Returns the node that precedes this node in a preorder traversal of
     * this node's tree.  Returns <code>null</code> if this node is the
     * first node of the traversal -- the root of the tree.
     * This is an inefficient way to
     * traverse the entire tree; use an enumeration, instead.
     *
     * @return  the node that precedes this node in a preorder traversal, or
     *          null if this node is the first
     */
    public BaseStructure getPreviousNode() {
        BaseStructure previousSibling;
        BaseStructure myParent = getParent();

        if (myParent == null) {
            return null;
        }

        previousSibling = getPreviousSibling();

        if (previousSibling != null) {
            if (previousSibling.getChildCount() == 0)
                return previousSibling;
            else
                return previousSibling.getLastLeaf();
        } else {
            return myParent;
        }
    }


    //
    //  Child Queries
    //

    /**
     * Returns true if <code>aNode</code> is a child of this node.  If
     * <code>aNode</code> is null, this method returns false.
     *
     * @param   aNode the node to determinate whether it is a child
     * @return  true if <code>aNode</code> is a child of this node; false if
     *                  <code>aNode</code> is null
     */
    public boolean isNodeChild(TreeNode aNode) {
        boolean retval;

        if (aNode == null) {
            retval = false;
        } else {
            if (getChildCount() == 0) {
                retval = false;
            } else {
                retval = (aNode.getParent() == this);
            }
        }

        return retval;
    }


    /**
     * Returns this node's first child.  If this node has no children,
     * throws NoSuchElementException.
     *
     * @return  the first child of this node
     * @exception       NoSuchElementException  if this node has no children
     */
    public BaseStructure getFirstChild() {
        if (getChildCount() == 0) {
            throw new NoSuchElementException("node has no children");
        }
        return getChildAt(0);
    }


    /**
     * Returns this node's last child.  If this node has no children,
     * throws NoSuchElementException.
     *
     * @return  the last child of this node
     * @exception       NoSuchElementException  if this node has no children
     */
    public BaseStructure getLastChild() {
        if (getChildCount() == 0) {
            throw new NoSuchElementException("node has no children");
        }
        return getChildAt(getChildCount()-1);
    }


    /**
     * Returns the child in this node's child array that immediately
     * follows <code>aChild</code>, which must be a child of this node.  If
     * <code>aChild</code> is the last child, returns null.  This method
     * performs a linear search of this node's children for
     * <code>aChild</code> and is O(n) where n is the number of children; to
     * traverse the entire array of children, use an enumeration instead.
     *
     * @param           aChild the child node to look for next child after it
     * @see             #children
     * @exception       IllegalArgumentException if <code>aChild</code> is
     *                                  null or is not a child of this node
     * @return  the child of this node that immediately follows
     *          <code>aChild</code>
     */
    public BaseStructure getChildAfter(TreeNode aChild) {
        if (aChild == null) {
            throw new IllegalArgumentException("argument is null");
        }

        int index = getIndex(aChild);           // linear search

        if (index == -1) {
            throw new IllegalArgumentException("node is not a child");
        }

        if (index < getChildCount() - 1) {
            return getChildAt(index + 1);
        } else {
            return null;
        }
    }


    /**
     * Returns the child in this node's child array that immediately
     * precedes <code>aChild</code>, which must be a child of this node.  If
     * <code>aChild</code> is the first child, returns null.  This method
     * performs a linear search of this node's children for <code>aChild</code>
     * and is O(n) where n is the number of children.
     *
     * @param           aChild the child node to look for previous child before it
     * @exception       IllegalArgumentException if <code>aChild</code> is null
     *                                          or is not a child of this node
     * @return  the child of this node that immediately precedes
     *          <code>aChild</code>
     */
    public BaseStructure getChildBefore(TreeNode aChild) {
        if (aChild == null) {
            throw new IllegalArgumentException("argument is null");
        }

        int index = getIndex(aChild);           // linear search

        if (index == -1) {
            throw new IllegalArgumentException("argument is not a child");
        }

        if (index > 0) {
            return getChildAt(index - 1);
        } else {
            return null;
        }
    }


    //
    //  Sibling Queries
    //


    /**
     * Returns true if <code>anotherNode</code> is a sibling of (has the
     * same parent as) this node.  A node is its own sibling.  If
     * <code>anotherNode</code> is null, returns false.
     *
     * @param   anotherNode     node to test as sibling of this node
     * @return  true if <code>anotherNode</code> is a sibling of this node
     */
    public boolean isNodeSibling(TreeNode anotherNode) {
        boolean retval;

        if (anotherNode == null) {
            retval = false;
        } else if (anotherNode == this) {
            retval = true;
        } else {
            TreeNode  myParent = getParent();
            retval = (myParent != null && myParent == anotherNode.getParent());

            if (retval && !((BaseStructure)getParent())
                    .isNodeChild(anotherNode)) {
                throw new Error("sibling has different parent");
            }
        }

        return retval;
    }


    /**
     * Returns the number of siblings of this node.  A node is its own sibling
     * (if it has no parent or no siblings, this method returns
     * <code>1</code>).
     *
     * @return  the number of siblings of this node
     */
    public int getSiblingCount() {
        TreeNode myParent = getParent();

        if (myParent == null) {
            return 1;
        } else {
            return myParent.getChildCount();
        }
    }


    /**
     * Returns the next sibling of this node in the parent's children array.
     * Returns null if this node has no parent or is the parent's last child.
     * This method performs a linear search that is O(n) where n is the number
     * of children; to traverse the entire array, use the parent's child
     * enumeration instead.
     *
     * @see     #children
     * @return  the sibling of this node that immediately follows this node
     */
    public BaseStructure getNextSibling() {
        BaseStructure retval;

        BaseStructure myParent = getParent();

        if (myParent == null) {
            retval = null;
        } else {
            retval = (BaseStructure)myParent.getChildAfter(this);      // linear search
        }

        if (retval != null && !isNodeSibling(retval)) {
            throw new Error("child of parent is not a sibling");
        }

        return retval;
    }


    /**
     * Returns the previous sibling of this node in the parent's children
     * array.  Returns null if this node has no parent or is the parent's
     * first child.  This method performs a linear search that is O(n) where n
     * is the number of children.
     *
     * @return  the sibling of this node that immediately precedes this node
     */
    public BaseStructure getPreviousSibling() {
        BaseStructure retval;

        BaseStructure myParent = getParent();

        if (myParent == null) {
            retval = null;
        } else {
            retval = myParent.getChildBefore(this);     // linear search
        }

        if (retval != null && !isNodeSibling(retval)) {
            throw new Error("child of parent is not a sibling");
        }

        return retval;
    }



    //
    //  Leaf Queries
    //

//    /**
//     * Returns true if this node has no children.  To distinguish between
//     * nodes that have no children and nodes that <i>cannot</i> have
//     * children (e.g. to distinguish files from empty directories), use this
//     * method in conjunction with <code>getAllowsChildren</code>
//     *
//     * @see     #getAllowsChildren
//     * @return  true if this node has no children
//     */
//    public boolean isLeaf() {
//        return (getChildCount() == 0);
//    }


    /**
     * Finds and returns the first leaf that is a descendant of this node --
     * either this node or its first child's first leaf.
     * Returns this node if it is a leaf.
     *
     * @see     #isLeaf
     * @see     #isNodeDescendant
     * @return  the first leaf in the subtree rooted at this node
     */
    public BaseStructure getFirstLeaf() {
        BaseStructure node = this;

        while (!node.isLeaf()) {
            node = node.getFirstChild();
        }

        return node;
    }


    /**
     * Finds and returns the last leaf that is a descendant of this node --
     * either this node or its last child's last leaf.
     * Returns this node if it is a leaf.
     *
     * @see     #isLeaf
     * @see     #isNodeDescendant
     * @return  the last leaf in the subtree rooted at this node
     */
    public BaseStructure getLastLeaf() {
        BaseStructure node = this;

        while (!node.isLeaf()) {
            node = node.getLastChild();
        }

        return node;
    }


    /**
     * Returns the leaf after this node or null if this node is the
     * last leaf in the tree.
     * <p>
     * In this implementation of the <code>MutableNode</code> interface,
     * this operation is very inefficient. In order to determine the
     * next node, this method first performs a linear search in the
     * parent's child-list in order to find the current node.
     * <p>
     * That implementation makes the operation suitable for short
     * traversals from a known position. But to traverse all of the
     * leaves in the tree, you should use <code>depthFirstEnumeration</code>
     * to enumerate the nodes in the tree and use <code>isLeaf</code>
     * on each node to determine which are leaves.
     *
     * @see     #isLeaf
     * @return  returns the next leaf past this node
     */
    public BaseStructure getNextLeaf() {
        BaseStructure nextSibling;
        BaseStructure myParent = getParent();

        if (myParent == null)
            return null;

        nextSibling = getNextSibling(); // linear search

        if (nextSibling != null)
            return nextSibling.getFirstLeaf();

        return myParent.getNextLeaf();  // tail recursion
    }


    /**
     * Returns the leaf before this node or null if this node is the
     * first leaf in the tree.
     * <p>
     * In this implementation of the <code>MutableNode</code> interface,
     * this operation is very inefficient. In order to determine the
     * previous node, this method first performs a linear search in the
     * parent's child-list in order to find the current node.
     * <p>
     * That implementation makes the operation suitable for short
     * traversals from a known position. But to traverse all of the
     * leaves in the tree, you should use <code>depthFirstEnumeration</code>
     * to enumerate the nodes in the tree and use <code>isLeaf</code>
     * on each node to determine which are leaves.
     *
     * @see             #isLeaf
     * @return  returns the leaf before this node
     */
    public BaseStructure getPreviousLeaf() {
        BaseStructure previousSibling;
        BaseStructure myParent = getParent();

        if (myParent == null)
            return null;

        previousSibling = getPreviousSibling(); // linear search

        if (previousSibling != null)
            return previousSibling.getLastLeaf();

        return myParent.getPreviousLeaf();              // tail recursion
    }


    /**
     * Returns the total number of leaves that are descendants of this node.
     * If this node is a leaf, returns <code>1</code>.  This method is O(n)
     * where n is the number of descendants of this node.
     *
     * @see     #isNodeAncestor
     * @return  the number of leaves beneath this node
     */
    public int getLeafCount() {
        int count = 0;

        TreeNode node;
        Enumeration<TreeNode> enum_ =  new BreadthFirstEnumeration(this); // order matters not

        while (enum_.hasMoreElements()) {
            node = enum_.nextElement();
            if (node.isLeaf()) {
                count++;
            }
        }

        if (count < 1) {
            throw new Error("tree has zero leaves");
        }

        return count;
    }

    /**
     * Creates and returns an enumeration that traverses the subtree rooted at
     * this node in breadth-first order.  The first node returned by the
     * enumeration's <code>nextElement()</code> method is this node.<P>
     *
     * Modifying the tree by inserting, removing, or moving a node invalidates
     * any enumerations created before the modification.
     *
     * @see     #depthFirstEnumeration
     * @return  an enumeration for traversing the tree in breadth-first order
     */
    public Enumeration<TreeNode> breadthFirstEnumeration() {
        return new BreadthFirstEnumeration(this);
    }

    /**
     * Creates and returns an enumeration that traverses the subtree rooted at
     * this node in depth-first order.  The first node returned by the
     * enumeration's <code>nextElement()</code> method is the leftmost leaf.
     * This is the same as a postorder traversal.<P>
     *
     * Modifying the tree by inserting, removing, or moving a node invalidates
     * any enumerations created before the modification.
     *
     * @see     #breadthFirstEnumeration
     * @return  an enumeration for traversing the tree in depth-first order
     */
    public Enumeration<TreeNode> depthFirstEnumeration() {
        return new PostorderEnumeration(this);
    }


    final class BreadthFirstEnumeration implements Enumeration<TreeNode> {
        Queue queue;

        public BreadthFirstEnumeration(TreeNode rootNode) {
            super();
            Vector<TreeNode> v = new Vector<TreeNode>(1);
            v.addElement(rootNode);     // PENDING: don't really need a vector
            queue = new Queue();
            queue.enqueue(v.elements());
        }

        public boolean hasMoreElements() {
            return (!queue.isEmpty() &&
                    ((Enumeration)queue.firstObject()).hasMoreElements());
        }

        public BaseStructure nextElement() {
            Enumeration<?> enumer = (Enumeration)queue.firstObject();
            BaseStructure    node = (BaseStructure)enumer.nextElement();
            Enumeration<?> children = node.children();

            if (!enumer.hasMoreElements()) {
                queue.dequeue();
            }
            if (children.hasMoreElements()) {
                queue.enqueue(children);
            }
            return node;
        }


        // A simple queue with a linked list data structure.
        final class Queue {
            Queue.QNode head; // null if empty
            Queue.QNode tail;

            final class QNode {
                public Object   object;
                public Queue.QNode next;   // null if end
                public QNode(Object object, Queue.QNode next) {
                    this.object = object;
                    this.next = next;
                }
            }

            public void enqueue(Object anObject) {
                if (head == null) {
                    head = tail = new Queue.QNode(anObject, null);
                } else {
                    tail.next = new Queue.QNode(anObject, null);
                    tail = tail.next;
                }
            }

            public Object dequeue() {
                if (head == null) {
                    throw new NoSuchElementException("No more elements");
                }

                Object retval = head.object;
                Queue.QNode oldHead = head;
                head = head.next;
                if (head == null) {
                    tail = null;
                } else {
                    oldHead.next = null;
                }
                return retval;
            }

            public Object firstObject() {
                if (head == null) {
                    throw new NoSuchElementException("No more elements");
                }

                return head.object;
            }

            public boolean isEmpty() {
                return head == null;
            }

        } // End of class Queue

    }  // End of class BreadthFirstEnumeration


    /** Same as a depth-first tranversal. */
    final class PostorderEnumeration implements Enumeration<TreeNode> {
        protected TreeNode root;
        protected Enumeration<? extends TreeNode> children;
        protected Enumeration<TreeNode> subtree;

        public PostorderEnumeration(TreeNode rootNode) {
            super();
            root = rootNode;
            children = root.children();
            subtree =  Collections.emptyEnumeration();
        }

        public boolean hasMoreElements() {
            return root != null;
        }

        public TreeNode nextElement() {
            BaseStructure retval;

            if (subtree.hasMoreElements()) {
                retval = (BaseStructure)subtree.nextElement();
            } else if (children.hasMoreElements()) {
                subtree = new PostorderEnumeration(children.nextElement());
                retval = (BaseStructure)subtree.nextElement();
            } else {
                retval = (BaseStructure)root;
                root = null;
            }

            return retval;
        }

    }  // End of class PostorderEnumeration




    /**
     * Visit all the structures in this structure (including the structure itself --
     * which is considered its own descendant).
     * This is similar to listening to the event as it is being parsed,
     * but is done to a complete (already) parsed event.
     *
     * @param listener a listener to notify as each structure is visited.
     */
    public void visitAllStructures(IEvioListener listener) {
        visitAllDescendants(this, listener, null);
    }

    /**
     * Visit all the structures in this structure (including the structure itself --
     * which is considered its own descendant) in a depth first manner.
     *
     * @param listener a listener to notify as each structure is visited.
     * @param filter an optional filter that must "accept" structures before
     *               they are passed to the listener. If <code>null</code>, all
     *               structures are passed. In this way, specific types of
     *               structures can be captured.
     */
    public void visitAllStructures(IEvioListener listener, IEvioFilter filter) {
        visitAllDescendants(this, listener, filter);
    }

    /**
     * Visit all the descendants of a given structure
     * (which is considered a descendant of itself.)
     *
     * @param structure the starting structure.
     * @param listener a listener to notify as each structure is visited.
     * @param filter an optional filter that must "accept" structures before
     *               they are passed to the listener. If <code>null</code>, all
     *               structures are passed. In this way, specific types of
     *               structures can be captured.
     */
    private void visitAllDescendants(BaseStructure structure, IEvioListener listener, IEvioFilter filter) {
        if (listener != null) {
            boolean accept = true;
            if (filter != null) {
                accept = filter.accept(structure.getStructureType(), structure);
            }

            if (accept) {
                listener.gotStructure(this, structure);
            }
        }

        if (!(structure.isLeaf())) {
            for (BaseStructure child : structure.getChildren()) {
                visitAllDescendants(child, listener, filter);
            }
        }
    }

    /**
     * Visit all the descendant structures, and collect those that pass a filter.
     * @param filter the filter that must be passed. If <code>null</code>,
     *               this will return all the structures.
     * @return a collection of all structures that are accepted by a filter.
     */
    public List<BaseStructure> getMatchingStructures(IEvioFilter filter) {
        final Vector<BaseStructure> structures = new Vector<BaseStructure>(25, 10);

        IEvioListener listener = new IEvioListener() {
            public void startEventParse(BaseStructure structure) { }

            public void endEventParse(BaseStructure structure) { }

            public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                structures.add((BaseStructure)structure);
            }
        };

        visitAllStructures(listener, filter);

        if (structures.size() == 0) {
            return null;
        }
        return structures;
    }

	/**
	 * This method is not relevant for this implementation.
     * An empty implementation is provided to satisfy the interface.
	 * Part of the <code>MutableTreeNode</code> interface.
	 */
	public void setUserObject(Object arg0) {
	}

	/**
	 * Checks whether children are allowed. Structures of structures can have children.
     * Structures of primitive data can not. Thus is is entirely determined by the content type.
     * Part of the <code>MutableTreeNode</code> interface.
	 * 
	 * @return <code>true</code> if this node does not hold primitive data,
     *         i.e., if it is a structure of structures (a container).
	 */
	public boolean getAllowsChildren() {
		return header.getDataType().isStructure();
	}

	/**
	 * Obtain the child at the given index. Part of the <code>MutableTreeNode</code> interface.
	 * 
	 * @param index the target index.
	 * @return the child at the given index or null if none
	 */
	public BaseStructure getChildAt(int index) {
        if (children == null) return null;

        BaseStructure b = null;
        try {
            b = children.get(index);
        }
        catch (ArrayIndexOutOfBoundsException e) { }
        return b;
	}

	/**
	 * Get the count of the number of children. Part of the <code>MutableTreeNode</code> interface.
	 * 
	 * @return the number of children.
	 */
	public int getChildCount() {
		if (children == null) {
			return 0;
		}
		return children.size();
	}

	/**
	 * Get the index of a node. Part of the <code>MutableTreeNode</code> interface.
	 * 
	 * @return the index of the target node or -1 if no such node in tree
	 */
	public int getIndex(TreeNode node) {
        if (children == null || node == null) return -1;
		return children.indexOf(node);
	}

	/**
	 * Checks whether this is a leaf. Leaves are structures with no children.
     * All structures that contain primitive data are leaf structures.
	 * Part of the <code>MutableTreeNode</code> interface.<br>
	 * Note: this means that an empty container, say a Bank of Segments that have no segments, is a leaf.
	 * 
	 * @return <code>true</code> if this is a structure with a primitive data type, i.e., it is not a container
	 *         structure that contains other structures.
	 */
	public boolean isLeaf() {
        return isLeaf;
	}
	
	/**
	 * Checks whether this structure is a container, i.e. a structure of structures.
	 * @return <code>true</code> if this structure is a container. This is the same check as 
	 * <code>getAllowsChildren()</code>.
	 */
	public boolean isContainer() {
		return header.getDataType().isStructure();
	}

    /**
   	 * This calls the xml write for all of a structure's children.
     * It is used for a depth-first traversal through the tree
   	 * when writing an event to xml.
   	 *
   	 * @param xmlWriter the writer used to write the events. It is tied to an open file.
     * @param hex if true, ints get displayed in hexadecimal
   	 */
    protected void childrenToXML(XMLStreamWriter xmlWriter, boolean hex) {
        if (children == null) {
            return;
        }

        increaseXmlIndent();

        for (BaseStructure structure : children) {
            structure.setXmlIndent(xmlIndent);
            structure.toXML(xmlWriter, hex);
        }

        decreaseXmlIndent();
    }

    /**
	 * All structures have a common start to their xml writing
	 * 
	 * @param xmlWriter the writer used to write the events. It is tied to an open file.
	 */
	protected void commonXMLStart(XMLStreamWriter xmlWriter) {
		try {
			xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(xmlIndent);
			xmlWriter.writeStartElement(xmlElementName);
		}
		catch (XMLStreamException e) {
			e.printStackTrace();
		}
	}

    /**
     * All structures close their xml writing the same way, by drilling down to embedded children.
     *
     * @param xmlWriter the writer used to write the events. It is tied to an open file.
     * @param hex if true, ints get displayed in hexadecimal
     */
    protected void commonXMLClose(XMLStreamWriter xmlWriter, boolean hex) {
        childrenToXML(xmlWriter, hex);
        try {
            xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(xmlIndent);
            xmlWriter.writeEndElement();
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }
    }

	/**
	 * All structures have a common data write for their xml writing.<p>
     *
     * Warning: this is essentially duplicated in
     * {@link Utilities#writeXmlData(EvioNode, int, String, boolean, XMLStreamWriter)}
     * so any change here needs to be made there too.
	 *
	 * @param xmlWriter the writer used to write the events. It is tied to an open file.
     * @param hex       if true, ints get displayed in hexadecimal
	 */
	protected void commonXMLDataWrite(XMLStreamWriter xmlWriter, boolean hex) {

		// only leafs write data
		if (!isLeaf()) {
			return;
		}

		try {
            int count;
            boolean isLast;
            String s;
            String indent = String.format("\n%s", xmlIndent);

			//BaseStructureHeader header = getHeader();
			switch (header.getDataType()) {
			case DOUBLE64:
				double doubledata[] = getDoubleData();
                count = doubledata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%25.17g", doubledata[i]);
                    xmlWriter.writeCharacters(s);
                    if (!isLast && (i%2 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
				break;

			case FLOAT32:
				float floatdata[] = getFloatData();
                count = floatdata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%4 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%15.8g", floatdata[i]);
                    xmlWriter.writeCharacters(s);
                    if (!isLast && (i%4 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
				break;

			case LONG64:
				long[] longdata = getLongData();
                count = longdata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%016x", longdata[i]);
                    }
                    else {
                        s = String.format("%20d", longdata[i]);
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%2 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
				break;

            case ULONG64:
                longdata = getLongData();
                count = longdata.length;
                BigInteger bg;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%016x", longdata[i]);
                    }
                    else {
                        bg = new BigInteger(1, ByteDataTransformer.toBytes(longdata[i], ByteOrder.BIG_ENDIAN));
                        s = String.format("%20s", bg.toString());
                        // For java version 8+, no BigIntegers necessary:
                        //s = String.format("%20s  ", Long.toUnsignedString(longdata[i]));
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%2 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
                break;

			case INT32:
                int[] intdata = getIntData();
                count = intdata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%4 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%08x", intdata[i]);
                    }
                    else {
                        s = String.format("%11d", intdata[i]);
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%4 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
                break;

			case UINT32:
				intdata = getIntData();
                count = intdata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%4 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%08x", intdata[i]);
                    }
                    else {
                        s = String.format("%11d", ((long) intdata[i]) & 0xffffffffL);
                        //s = String.format("%11s  ", Integer.toUnsignedString(intdata[i]));
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%4 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
				break;

            case SHORT16:
                short[] shortdata = getShortData();
                count = shortdata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%04x", shortdata[i]);
                    }
                    else {
                        s = String.format("%6d", shortdata[i]);
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%8 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
                break;

            case USHORT16:
				shortdata = getShortData();
                count = shortdata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%04x", shortdata[i]);
                    }
                    else {
                        s = String.format("%6d", ((int) shortdata[i]) & 0xffff);
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%8 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
				break;

			case CHAR8:
                byte[] bytedata = getByteData();
                count = bytedata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%02x", bytedata[i]);
                    }
                    else {
                        s = String.format("%4d", bytedata[i]);
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%8 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
                break;

            case UNKNOWN32:
			case UCHAR8:
                bytedata = getByteData();
                count = bytedata.length;
                for (int i=0; i < count; i++) {
                    isLast = (i == count - 1);
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%02x", bytedata[i]);
                    }
                    else {
                        s = String.format("%4d", ((short) bytedata[i]) & 0xff);
                    }
                    xmlWriter.writeCharacters(s);

                    if (!isLast && (i%8 == 0)) {
                        xmlWriter.writeCharacters("  ");
                    }
                }
				break;

			case CHARSTAR8:
                String stringdata[] = getStringData();
                for (String str : stringdata) {
                    xmlWriter.writeCharacters(indent);
                    xmlWriter.writeCData(str);
                }
				break;

            case COMPOSITE:
                if (compositeData == null) {
                    try {
                        getCompositeData();
                    }
                    catch (EvioException e) {
                        return;
                    }
                }

                for (CompositeData cd : compositeData) {
                    cd.toXML(xmlWriter, this, hex);
                }
                break;

            default:
            }
		}
		catch (XMLStreamException e) {
			e.printStackTrace();
		}

	}

	/**
	 * Compute the dataLength. This is the amount of data needed
     * by a leaf of primitives. For non-leafs (structures of
	 * structures) this returns 0. For data types smaller than an
     * int, e.g. a short, it computes assuming padding to an
	 * integer number of ints. For example, if we are writing a byte
     * array of length 3 or 4, then it would return 1. If
	 * the byte array is 5,6,7 or 8 it would return 2;
     *
     * The problem with the original method was that if the data was
     * still packed (only rawBytes was defined), then the data
     * was unpacked - very inefficient. No need to unpack data
     * just to calculate a length. This method doesn't do that.
	 *
	 * @return the amount of ints needed by a leaf of primitives, else 0
	 */
	protected int dataLength() {

		int datalen = 0;

		// only leafs write data
		if (isLeaf()) {
			//BaseStructureHeader header = getHeader();

			switch (header.getDataType()) {
			case DOUBLE64:
            case LONG64:
            case ULONG64:
			    datalen = 2 * getNumberDataItems();
				break;

            case INT32:
            case UINT32:
			case FLOAT32:
			    datalen = getNumberDataItems();
				break;

			case SHORT16:
			case USHORT16:
                int items = getNumberDataItems();
                if (items == 0) {
                    datalen = 0;
                    break;
                }
                datalen = 1 + (items - 1) / 2;
				break;

            case UNKNOWN32:
            case CHAR8:
            case UCHAR8:
                items = getNumberDataItems();
                if (items == 0) {
                    datalen = 0;
                    break;
                }
                datalen = 1 + (items - 1) / 4;
                break;

            case CHARSTAR8: // rawbytes contain ascii, already padded
            case COMPOSITE:
                if (rawBytes != null) {
                    datalen = 1 + (rawBytes.length - 1) / 4;
                }
                break;

            default:
            } // switch
		} // isleaf

		return datalen;
	}

    /**
   	 * Get the children of this structure.
     * Use {@link #getChildrenList()} instead.
   	 *
     * @deprecated child structures are no longer keep in a Vector
   	 * @return the children of this structure.
   	 */
    @Deprecated
   	public Vector<BaseStructure> getChildren() {
        if (children == null) return null;
        return new Vector<BaseStructure>(children);
    }

    /**
   	 * Get the children of this structure.
   	 *
   	 * @return the children of this structure.
   	 */
   	public List<BaseStructure> getChildrenList() {return children;}

    /**
     * Get whether the lengths of all header fields for this structure
     * and all it descendants are up to date or not.
     *
     * @return whether the lengths of all header fields for this structure
     *         and all it descendants are up to date or not.
     */
    protected boolean lengthsUpToDate() {
        return lengthsUpToDate;
    }

    /**
     * Set whether the lengths of all header fields for this structure
     * and all it descendants are up to date or not.
     *
     * @param lengthsUpToDate are the lengths of all header fields for this structure
     *                        and all it descendants up to date or not.
     */
    protected void lengthsUpToDate(boolean lengthsUpToDate) {
        this.lengthsUpToDate = lengthsUpToDate;
        // propagate back up the tree if lengths have been changed
        if (!lengthsUpToDate) {
            if (parent != null) {
                parent.lengthsUpToDate(false);
            }
        }
    }

    /**
     * Compute and set length of all header fields for this structure and all its descendants.
     * For writing events, this will be crucial for setting the values in the headers.
     *
     * @return the length that would go in the header field (for a leaf).
     * @throws EvioException if the length is too large (&gt; {@link Integer#MAX_VALUE}),
     */
    public int setAllHeaderLengths() throws EvioException {
        // if length info is current, don't bother to recalculate it
        if (lengthsUpToDate) {
//System.err.println(" setAllHeaderLengths: up to date, return");
            return header.getLength();
        }

        int datalen, len;

        if (isLeaf()) {
            // # of 32 bit ints for leaves, 0 for empty containers (also considered leaves)
            datalen = dataLength();
//System.err.println(" setAllHeaderLengths: is leaf, len = " + datalen);
        }
        else {
            datalen = 0;

            if (children == null) {
System.err.println(" setAllHeaderLengths: non leaf with null children!");
                System.exit(1);
            }
            for (BaseStructure child : children) {
                len = child.setAllHeaderLengths();
//System.err.println(" setAllHeaderLengths: child len = " + len);
                // Add this check to make sure structure is not being overfilled
                if (Integer.MAX_VALUE - datalen < len + 1) {
                    throw new EvioException("added data overflowed containing structure");
                }
                datalen += len + 1;  // + 1 for the header length word of each child
            }
//System.err.println(" setAllHeaderLengths: total data len = " + datalen);
        }

        len =  header.getHeaderLength() - 1;  // - 1 for length header word
        if (Integer.MAX_VALUE - datalen < len + 1) {
            throw new EvioException("added data overflowed containing structure");
        }

        datalen += len;
//System.err.println(" setAllHeaderLengths: hdr len = " + datalen);

        // set the datalen for the header
        header.setLength(datalen);
        lengthsUpToDate(true);
        return datalen;
    }


	/**
	 * Write myself out a byte buffer with fastest algorithms I could find.
	 *
	 * @param byteBuffer the byteBuffer to write to.
	 * @return the number of bytes written.
     * @throws BufferOverflowException if too much data to write.
	 */
	public int write(ByteBuffer byteBuffer) throws BufferOverflowException {

        if (byteBuffer.remaining() < getTotalBytes()) {
            throw new BufferOverflowException();
        }

        int startPos = byteBuffer.position();

        // write the header
		header.write(byteBuffer);

        int curPos = byteBuffer.position();

		if (isLeaf()) {
			//BaseStructureHeader header = getHeader();
			switch (header.getDataType()) {
			case DOUBLE64:
                // if data sent over wire or read from file ...
                if (rawBytes != null) {
                    // if bytes do NOT need swapping, this is 36x faster ...
                    if (byteOrder == byteBuffer.order())  {
                        byteBuffer.put(rawBytes, 0, rawBytes.length);
                    }
                    // else if bytes need swapping ...
                    else {
                        DoubleBuffer db = byteBuffer.asDoubleBuffer();
                        DoubleBuffer rawBuf = ByteBuffer.wrap(rawBytes).order(byteOrder).asDoubleBuffer();
                        db.put(rawBuf);
                        byteBuffer.position(curPos + rawBytes.length);
                    }
                }
                // else if user set data thru API (can't-rely-on / no rawBytes array) ...
				else {
                    DoubleBuffer db = byteBuffer.asDoubleBuffer();
                    db.put(doubleData, 0, doubleData.length);
                    byteBuffer.position(curPos + 8*doubleData.length);
				}
				break;

			case FLOAT32:
                if (rawBytes != null) {
                    if (byteOrder == byteBuffer.order()) {
                        byteBuffer.put(rawBytes, 0, rawBytes.length);
                    }
                    else {
                        FloatBuffer db = byteBuffer.asFloatBuffer();
                        FloatBuffer rawBuf = ByteBuffer.wrap(rawBytes).order(byteOrder).asFloatBuffer();
                        db.put(rawBuf);
                        byteBuffer.position(curPos + rawBytes.length);
                    }
                }
				else {
                    FloatBuffer db = byteBuffer.asFloatBuffer();
                    db.put(floatData, 0, floatData.length);
                    byteBuffer.position(curPos + 4*floatData.length);
				}
				break;

			case LONG64:
			case ULONG64:
                if (rawBytes != null) {
                    if (byteOrder == byteBuffer.order()) {
                        byteBuffer.put(rawBytes, 0, rawBytes.length);
                    }
                    else {
                        LongBuffer db = byteBuffer.asLongBuffer();
                        LongBuffer rawBuf = ByteBuffer.wrap(rawBytes).order(byteOrder).asLongBuffer();
                        db.put(rawBuf);
                        byteBuffer.position(curPos + rawBytes.length);
                    }
                }
				else {
                    LongBuffer db = byteBuffer.asLongBuffer();
                    db.put(longData, 0, longData.length);
                    byteBuffer.position(curPos + 8*longData.length);
				}

				break;

			case INT32:
			case UINT32:
                if (rawBytes != null) {
                    if (byteOrder == byteBuffer.order()) {
                        byteBuffer.put(rawBytes, 0, rawBytes.length);
                    }
                    else {
                        IntBuffer db = byteBuffer.asIntBuffer();
                        IntBuffer rawBuf = ByteBuffer.wrap(rawBytes).order(byteOrder).asIntBuffer();
                        db.put(rawBuf);
                        byteBuffer.position(curPos + rawBytes.length);
                    }
                }
				else {
                    IntBuffer db = byteBuffer.asIntBuffer();
                    db.put(intData, 0, intData.length);
                    byteBuffer.position(curPos + 4*intData.length);
				}
				break;

			case SHORT16:
			case USHORT16:
                if (rawBytes != null) {
                    if (byteOrder == byteBuffer.order()) {
                        byteBuffer.put(rawBytes, 0, rawBytes.length);
                    }
                    else {
                        ShortBuffer db = byteBuffer.asShortBuffer();
                        ShortBuffer rawBuf = ByteBuffer.wrap(rawBytes).order(byteOrder).asShortBuffer();
                        db.put(rawBuf);
                        byteBuffer.position(curPos + rawBytes.length);
                    }
                }
				else {
                    ShortBuffer db = byteBuffer.asShortBuffer();
                    db.put(shortData, 0, shortData.length);
                    byteBuffer.position(curPos + 2*shortData.length);

					// might have to pad to 4 byte boundary
                    if (shortData.length % 2 > 0) {
                        byteBuffer.putShort((short) 0);
                    }
				}
				break;

            case UNKNOWN32:
			case CHAR8:
			case UCHAR8:
                if (rawBytes != null) {
                    byteBuffer.put(rawBytes, 0, rawBytes.length);
                }
				else {
                    byteBuffer.put(charData, 0, charData.length);

					// might have to pad to 4 byte boundary
                    byteBuffer.put(padValues, 0, padCount[charData.length%4]);
				}
				break;

            case CHARSTAR8: // rawbytes contains ascii, already padded
                if (rawBytes != null) {
                    byteBuffer.put(rawBytes, 0, rawBytes.length);
                }
                break;

            case COMPOSITE:
                // compositeData object always has rawBytes defined
                if (rawBytes != null) {
                    if (byteOrder == byteBuffer.order()) {
                        byteBuffer.put(rawBytes, 0, rawBytes.length);
                    }
                    else {
//                        System.out.println("Before swap in writing output:");
//                        int[] intA = ByteDataTransformer.getAsIntArray(rawBytes, ByteOrder.BIG_ENDIAN);
//                        for (int i : intA) {
//                            System.out.println("Ox" + Integer.toHexString(i));
//                        }

                        // swap rawBytes
                        byte[] swappedRaw = new byte[rawBytes.length];
                        try {
                            CompositeData.swapAll(rawBytes, 0, swappedRaw, 0,
                                                  rawBytes.length/4, byteOrder);
                        }
                        catch (EvioException e) { /* never happen */ }

                        // write them to buffer
                        byteBuffer.put(swappedRaw, 0, swappedRaw.length);
//                        System.out.println("Swap in writing output:");
//                        intA = ByteDataTransformer.getAsIntArray(swappedRaw, ByteOrder.LITTLE_ENDIAN);
//                        for (int i : intA) {
//                            System.out.println("Ox" + Integer.toHexString(i));
//                        }
                    }
                }
				break;

            default:

            } // switch

		} // isLeaf
		else if (children != null) {
			for (BaseStructure child : children) {
				child.write(byteBuffer);
			}
		} // not leaf

		return byteBuffer.position() - startPos;
	}


    //----------------------------------------------------------------------
    // Methods to append to exising data if any or to set the data if none.
    //----------------------------------------------------------------------


    /**
     * Appends int data to the structure. If the structure has no data, then this
     * is the same as setting the data.
     * @param data the int data to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
     */
    public void appendIntData(int data[]) throws EvioException {

        // make sure the structure is set to hold this kind of data
        DataType dataType = header.getDataType();
        if ((dataType != DataType.INT32) && (dataType != DataType.UINT32)) {
            throw new EvioException("Tried to append int data to a structure of type: " + dataType);
        }

        // if no data to append, just cave
        if (data == null) {
            return;
        }

        // if no int data ...
        if (intData == null) {
            // if no raw data, things are easy
            if (rawBytes == null) {
                intData = data;
                numberDataItems = data.length;
            }
            // otherwise expand raw data first, then add int array
            else {
                int size1 = rawBytes.length/4;
                int size2 = data.length;

                // TODO: Will need to revise this if using Java 9+ with long array indeces
                if (Integer.MAX_VALUE - size1 < size2) {
                    throw new EvioException("added data overflowed containing structure");
                }
                intData = new int[size1 + size2];
                // unpack existing raw data
                ByteDataTransformer.toIntArray(rawBytes, byteOrder, intData, 0);
                // append new data
                System.arraycopy(data, 0, intData, size1, size2);
                numberDataItems = size1 + size2;
            }
        }
        else {
            int size1 = intData.length; // existing data
            int size2 = data.length;    // new data to append

            // TODO: Will need to revise this if using Java 9+ with long array indeces
            if (Integer.MAX_VALUE - size1 < size2) {
                throw new EvioException("added data overflowed containing structure");
            }
            intData = Arrays.copyOf(intData, size1 + size2);  // extend existing array
            System.arraycopy(data, 0, intData, size1, size2); // append
            numberDataItems += size2;
        }

        // This is not necessary but results in a tremendous performance
        // boost (10x) when writing events over a high speed network. Allows
        // the write to be a byte array copy.
        // Storing data as raw bytes limits the # of elements to Integer.MAX_VALUE/4.
        rawBytes = ByteDataTransformer.toBytes(intData, byteOrder);

        lengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * Appends int data to the structure. If the structure has no data, then this
     * is the same as setting the data.
     * @param byteData the data in ByteBuffer form to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
     */
    public void appendIntData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 4) {
            return;
        }

        appendIntData(ByteDataTransformer.toIntArray(byteData));
    }

	/**
	 * Appends short data to the structure. If the structure has no data, then this
	 * is the same as setting the data.
	 * @param data the short data to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
	 */
	public void appendShortData(short data[]) throws EvioException {
		
		DataType dataType = header.getDataType();
		if ((dataType != DataType.SHORT16) && (dataType != DataType.USHORT16)) {
			throw new EvioException("Tried to append short data to a structure of type: " + dataType);
		}
		
		if (data == null) {
			return;
		}
		
		if (shortData == null) {
            if (rawBytes == null) {
                shortData = data;
                numberDataItems = data.length;
            }
            else {
                int size1 = (rawBytes.length - header.getPadding())/2;
                int size2 = data.length;

                if (Integer.MAX_VALUE - size1 < size2) {
                    throw new EvioException("added data overflowed containing structure");
                }
                shortData = new short[size1 + size2];
                ByteDataTransformer.toShortArray(rawBytes, header.getPadding(),
                                                 byteOrder, shortData, 0);
                System.arraycopy(data, 0, shortData, size1, size2);
                numberDataItems = size1 + size2;
            }
		}
		else {
			int size1 = shortData.length;
			int size2 = data.length;

            if (Integer.MAX_VALUE - size1 < size2) {
                throw new EvioException("added data overflowed containing structure");
            }
			shortData = Arrays.copyOf(shortData, size1 + size2);
			System.arraycopy(data, 0, shortData, size1, size2);
            numberDataItems += size2;
		}

        // If odd # of shorts, there are 2 bytes of padding.
        if (numberDataItems%2 != 0) {
            header.setPadding(2);

            if (Integer.MAX_VALUE - 2*numberDataItems < 2) {
                throw new EvioException("added data overflowed containing structure");
            }

            // raw bytes must include padding
            rawBytes = new byte[2*numberDataItems + 2];
            ByteDataTransformer.toBytes(shortData, byteOrder, rawBytes, 0);
        }
        else {
            header.setPadding(0);
            rawBytes = ByteDataTransformer.toBytes(shortData, byteOrder);
        }

        lengthsUpToDate(false);
		setAllHeaderLengths();
	}

    /**
     * Appends short data to the structure. If the structure has no data, then this
     * is the same as setting the data.
     * @param byteData the data in ByteBuffer form to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
     */
    public void appendShortData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 2) {
            return;
        }

        appendShortData(ByteDataTransformer.toShortArray(byteData));
    }

	/**
	 * Appends long data to the structure. If the structure has no data, then this
	 * is the same as setting the data.
	 * @param data the long data to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
	 */
	public void appendLongData(long data[]) throws EvioException {
		
		DataType dataType = header.getDataType();
		if ((dataType != DataType.LONG64) && (dataType != DataType.ULONG64)) {
			throw new EvioException("Tried to append long data to a structure of type: " + dataType);
		}
		
		if (data == null) {
			return;
		}
		
		if (longData == null) {
            if (rawBytes == null) {
                longData = data;
                numberDataItems = data.length;
            }
            else {
                int size1 = rawBytes.length/8;
                int size2 = data.length;

                if (Integer.MAX_VALUE - size1 < size2) {
                    throw new EvioException("added data overflowed containing structure");
                }
                longData = new long[size1 + size2];
                ByteDataTransformer.toLongArray(rawBytes, byteOrder, longData, 0);
                System.arraycopy(data, 0, longData, size1, size2);
                numberDataItems = size1 + size2;
            }
		}
		else {
			int size1 = longData.length;
			int size2 = data.length;

            if (Integer.MAX_VALUE - size1 < size2) {
                throw new EvioException("added data overflowed containing structure");
            }
			longData = Arrays.copyOf(longData, size1 + size2);
			System.arraycopy(data, 0, longData, size1, size2);
            numberDataItems += size2;
		}

        rawBytes = ByteDataTransformer.toBytes(longData, byteOrder);

        lengthsUpToDate(false);
		setAllHeaderLengths();
	}
	
    /**
     * Appends long data to the structure. If the structure has no data, then this
     * is the same as setting the data.
     * @param byteData the data in ByteBuffer form to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
     */
    public void appendLongData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 8) {
            return;
        }

        appendLongData(ByteDataTransformer.toLongArray(byteData));
    }

	/**
	 * Appends byte data to the structure. If the structure has no data, then this
	 * is the same as setting the data.
	 * @param data the byte data to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
	 */
	public void appendByteData(byte data[]) throws EvioException {
		
		DataType dataType = header.getDataType();
		if ((dataType != DataType.CHAR8) && (dataType != DataType.UCHAR8)) {
			throw new EvioException("Tried to append byte data to a structure of type: " + dataType);
		}
		
		if (data == null) {
			return;
		}
		
		if (charData == null) {
            if (rawBytes == null) {
                charData = data;
                numberDataItems = data.length;
            }
            else {
                int size1 = rawBytes.length - header.getPadding();
                int size2 = data.length;

                if (Integer.MAX_VALUE - size1 < size2) {
                    throw new EvioException("added data overflowed containing structure");
                }
                charData = new byte[size1 + size2];
                System.arraycopy(rawBytes, 0, charData, 0, size1);
                System.arraycopy(data, 0, charData, size1, size2);
                numberDataItems = size1 + size2;
            }
		}
		else {
			int size1 = charData.length;
			int size2 = data.length;

            if (Integer.MAX_VALUE - size1 < size2) {
                throw new EvioException("added data overflowed containing structure");
            }
			charData = Arrays.copyOf(charData, size1 + size2);
			System.arraycopy(data, 0, charData, size1, size2);
            numberDataItems += data.length;
		}

        // store necessary padding to 4 byte boundaries.
        int padding = padCount[numberDataItems%4];
        header.setPadding(padding);
//System.out.println("# data items = " + numberDataItems + ", padding = " + padding);

        // Array creation sets everything to zero. Only need to copy in data.
        if (Integer.MAX_VALUE - numberDataItems < padding) {
            throw new EvioException("added data overflowed containing structure");
        }
        rawBytes = new byte[numberDataItems + padding];
        System.arraycopy(charData,  0, rawBytes, 0, numberDataItems);
//        System.arraycopy(padValues, 0, rawBytes, numberDataItems, padding); // unnecessary

        lengthsUpToDate(false);
		setAllHeaderLengths();
	}
	
    /**
     * Appends byte data to the structure. If the structure has no data, then this
     * is the same as setting the data. Data is copied in.
     * @param byteData the data in ByteBuffer form to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
     */
    public void appendByteData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 1) {
            return;
        }

        appendByteData(ByteDataTransformer.toByteArray(byteData));
    }

	/**
	 * Appends float data to the structure. If the structure has no data, then this
	 * is the same as setting the data.
	 * @param data the float data to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
	 */
	public void appendFloatData(float data[]) throws EvioException {
		
		DataType dataType = header.getDataType();
		if (dataType != DataType.FLOAT32) {
			throw new EvioException("Tried to append float data to a structure of type: " + dataType);
		}
		
		if (data == null) {
			return;
		}
		
		if (floatData == null) {
            if (rawBytes == null) {
                floatData = data;
                numberDataItems = data.length;
            }
            else {
                int size1 = rawBytes.length/4;
                int size2 = data.length;

                if (Integer.MAX_VALUE - size1 < size2) {
                    throw new EvioException("added data overflowed containing structure");
                }
                floatData = new float[size1 + size2];
                ByteDataTransformer.toFloatArray(rawBytes, byteOrder, floatData, 0);
                System.arraycopy(data, 0, floatData, size1, size2);
                numberDataItems = size1 + size2;
            }
		}
		else {
			int size1 = floatData.length;
			int size2 = data.length;

            if (Integer.MAX_VALUE - size1 < size2) {
                throw new EvioException("added data overflowed containing structure");
            }
			floatData = Arrays.copyOf(floatData, size1 + size2);
			System.arraycopy(data, 0, floatData, size1, size2);
            numberDataItems += data.length;
		}

        rawBytes = ByteDataTransformer.toBytes(floatData, byteOrder);

        lengthsUpToDate(false);
		setAllHeaderLengths();
	}

    /**
     * Appends float data to the structure. If the structure has no data, then this
     * is the same as setting the data.
     * @param byteData the data in ByteBuffer form to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
     */
    public void appendFloatData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 4) {
            return;
        }

        appendFloatData(ByteDataTransformer.toFloatArray(byteData));
    }

    /**
     * Appends string to the structure (as ascii). If the structure has no data, then this
     * is the same as setting the data. Don't worry about checking for size limits since
     * jevio structures will never contain a char array &gt; {@link Integer#MAX_VALUE} in size.
     * @param s the string to append (as ascii), or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type
     */
    public void appendStringData(String s) throws EvioException {
        appendStringData(new String[] {s});
    }

    /**
     * Appends an array of strings to the structure (as ascii).
     * If the structure has no data, then this
     * is the same as setting the data. Don't worry about checking for size limits since
     * jevio structures will never contain a char array &gt; {@link Integer#MAX_VALUE} in size.
     * @param s the strings to append (as ascii), or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type or to
     *                       badly formatted (not proper evio) data.
     */
    public void appendStringData(String[] s) throws EvioException {

        DataType dataType = header.getDataType();
        if (dataType != DataType.CHARSTAR8) {
            throw new EvioException("Tried to append string to a structure of type: " + dataType);
        }

        if (s == null) {
            return;
        }

        if (badStringFormat) {
            throw new EvioException("cannot add to badly formatted string data");
        }

        // if no existing data ...
        if (stringData == null) {
		    // if no raw data, things are easy
            if (rawBytes == null) {
                // create some storage
                stringsList = new ArrayList<String>(s.length > 10 ? s.length : 10);
                int len = 4; // max padding
                for (String st : s) {
                    len += st.length() + 1;
                }
                stringData = new StringBuilder(len);
                numberDataItems = s.length;
            }
            // otherwise expand raw data first, then add string
            else {
                unpackRawBytesToStrings();
                // Check to see if unpacking was successful before proceeding
                if (badStringFormat) {
                    throw new EvioException("cannot add to badly formatted string data");
                }
                // remove any existing padding
                stringData.delete(stringEnd, stringData.length());
                numberDataItems = stringsList.size() + s.length;
            }
        }
        else {
            // remove any existing padding
            stringData.delete(stringEnd, stringData.length());
            numberDataItems += s.length;
        }

        for (String st : s) {
            // store string
            stringsList.add(st);

            // add string
            stringData.append(st);

            // add ending null
            stringData.append('\000');
        }

        // mark end of data before adding padding
        stringEnd = stringData.length();

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int[] pads = {4,3,2,1};
        switch (pads[stringData.length()%4]) {
            case 4:
                stringData.append("\004\004\004\004");
                break;
            case 3:
                stringData.append("\004\004\004");
                break;
            case 2:
                stringData.append("\004\004");
                break;
            case 1:
                stringData.append('\004');
            default:
        }

        // set raw data
        try {
            rawBytes = stringData.toString().getBytes("US-ASCII");
        }
        catch (UnsupportedEncodingException e) { /* will never happen */ }

        lengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
	 * Appends double data to the structure. If the structure has no data, then this
	 * is the same as setting the data.
	 * @param data the double data to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
	 */
	public void appendDoubleData(double data[]) throws EvioException {
		
		DataType dataType = header.getDataType();
		if (dataType != DataType.DOUBLE64) {
			throw new EvioException("Tried to append double data to a structure of type: " + dataType);
		}
		
		if (data == null) {
			return;
		}
		
		if (doubleData == null) {
            if (rawBytes == null) {
			    doubleData = data;
                numberDataItems = data.length;
            }
            else {
                int size1 = rawBytes.length/8;
                int size2 = data.length;

                if (Integer.MAX_VALUE - size1 < size2) {
                    throw new EvioException("added data overflowed containing structure");
                }
                doubleData = new double[size1 + size2];
                ByteDataTransformer.toDoubleArray(rawBytes, byteOrder, doubleData, 0);
                System.arraycopy(data, 0, doubleData, size1, size2);
                numberDataItems = size1 + size2;
            }
		}
		else {
			int size1 = doubleData.length;
			int size2 = data.length;

            if (Integer.MAX_VALUE - size1 < size2) {
                throw new EvioException("added data overflowed containing structure");
            }
			doubleData = Arrays.copyOf(doubleData, size1 + size2);
			System.arraycopy(data, 0, doubleData, size1, size2);
            numberDataItems += data.length;
		}

        rawBytes = ByteDataTransformer.toBytes(doubleData, byteOrder);

        lengthsUpToDate(false);
		setAllHeaderLengths();
	}

    /**
	 * Appends double data to the structure. If the structure has no data, then this
	 * is the same as setting the data.
     * @param byteData the data in ByteBuffer form to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data has too many elements to store in raw byte array (JVM limit)
	 */
    public void appendDoubleData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 8) {
            return;
        }

        appendDoubleData(ByteDataTransformer.toDoubleArray(byteData));
    }

    /**
     * Appends CompositeData objects to the structure. If the structure has no data, then this
     * is the same as setting the data.
     * @param data the CompositeData objects to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data takes up too much memory to store in raw byte array (JVM limit)
	 */
	public void appendCompositeData(CompositeData[] data) throws EvioException {

		DataType dataType = header.getDataType();
		if (dataType != DataType.COMPOSITE) {
			throw new EvioException("Tried to set composite data in a structure of type: " + dataType);
		}

		if (data == null || data.length < 1) {
			return;
		}

        // Composite data is always in the local (in this case, BIG) endian
        // because if generated in JVM that's true, and if read in, it is
        // swapped to local if necessary. Either case it's big endian.
        if (compositeData == null) {
            if (rawBytes == null) {
                compositeData   = data;
                numberDataItems = data.length;
            }
            else {
                // Decode the raw data we have
                CompositeData[] cdArray = CompositeData.parse(rawBytes, byteOrder);
                if (cdArray == null) {
                    compositeData   = data;
                    numberDataItems = data.length;
                }
                else {
                    // Allocate array to hold everything
                    int len1 = cdArray.length, len2 = data.length;
                    int totalLen = len1 + len2;

                    if (Integer.MAX_VALUE - len1 < len2) {
                        throw new EvioException("added data overflowed containing structure");
                    }
                    compositeData = new CompositeData[totalLen];

                    // Fill with existing object first
                    System.arraycopy(cdArray, 0, compositeData, 0, len1);
//                    for (int i = 0; i < len1; i++) {
//                        compositeData[i] = cdArray[i];
//                    }
                    // Append new objects
                    System.arraycopy(data, 0, compositeData, len1, len2);
//                    for (int i = 0; i < len2; i++) {
//                        compositeData[i+len1] = data[i];
//                    }
                    numberDataItems = totalLen;
                }
            }
        }
        else {
            int len1 = compositeData.length, len2 = data.length;
            int totalLen = len1 + len2;

            if (Integer.MAX_VALUE - len1 < len2) {
                throw new EvioException("added data overflowed containing structure");
            }

            CompositeData[] cdArray = compositeData;
            compositeData = new CompositeData[totalLen];

            // Fill with existing object first
            System.arraycopy(cdArray, 0, compositeData, 0, len1);
            // Append new objects
            System.arraycopy(data, 0, compositeData, len1, len2);

            numberDataItems = totalLen;
        }

        rawBytes  = CompositeData.generateRawBytes(compositeData, byteOrder);
//        int[] intA = ByteDataTransformer.getAsIntArray(rawBytes, ByteOrder.BIG_ENDIAN);
//        for (int i : intA) {
//            System.out.println("Ox" + Integer.toHexString(i));
//        }

        lengthsUpToDate(false);
		setAllHeaderLengths();
	}


    //----------------------------------------------------------------------
    // Methods to set the data, erasing what may have been there previously.
    //----------------------------------------------------------------------


    /**
     * Set the data in this structure to the given array of ints.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#UINT32} or {@link DataType#INT32},
     * it will be reset in the header to {@link DataType#INT32}.
     *
     * @param data the int data to set to.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setIntData(int data[]) throws EvioException {

        DataType dataType = header.getDataType();
        if ((dataType != DataType.INT32) && (dataType != DataType.UINT32)) {
            header.setDataType(DataType.INT32);
        }

        clearData();
        appendIntData(data);
    }

    /**
     * Set the data in this structure to the ints contained in the given ByteBuffer.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#UINT32} or {@link DataType#INT32},
     * it will be reset in the header to {@link DataType#INT32}.
     *
     * @param byteData the int data in ByteBuffer form.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setIntData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 4) {
            return;
        }

        setIntData(ByteDataTransformer.toIntArray(byteData));
    }

    /**
     * Set the data in this structure to the given array of shorts.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#USHORT16} or {@link DataType#SHORT16},
     * it will be reset in the header to {@link DataType#SHORT16}.
     *
     * @param data the short data to set to.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setShortData(short data[]) throws EvioException {

        DataType dataType = header.getDataType();
        if ((dataType != DataType.SHORT16) && (dataType != DataType.USHORT16)) {
            header.setDataType(DataType.SHORT16);
        }

        clearData();
        appendShortData(data);
    }

    /**
     * Set the data in this structure to the shorts contained in the given ByteBuffer.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#USHORT16} or {@link DataType#SHORT16},
     * it will be reset in the header to {@link DataType#SHORT16}.
     *
     * @param byteData the short data in ByteBuffer form.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setShortData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 2) {
            return;
        }

        setShortData(ByteDataTransformer.toShortArray(byteData));
    }

    /**
     * Set the data in this structure to the given array of longs.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#ULONG64} or {@link DataType#LONG64},
     * it will be reset in the header to {@link DataType#LONG64}.
     *
     * @param data the long data to set to.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setLongData(long data[]) throws EvioException {

        DataType dataType = header.getDataType();
        if ((dataType != DataType.LONG64) && (dataType != DataType.ULONG64)) {
            header.setDataType(DataType.LONG64);
        }

        clearData();
        appendLongData(data);
    }

    /**
     * Set the data in this structure to the longs contained in the given ByteBuffer.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#ULONG64} or {@link DataType#LONG64},
     * it will be reset in the header to {@link DataType#LONG64}.
     *
     * @param byteData the long data in ByteBuffer form.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setLongData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 4) {
            return;
        }

        setLongData(ByteDataTransformer.toLongArray(byteData));
    }

    /**
     * Set the data in this structure to the given array of bytes.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#UCHAR8} or {@link DataType#CHAR8},
     * it will be reset in the header to {@link DataType#CHAR8}.
     *
     * @param data the byte data to set to.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setByteData(byte data[]) throws EvioException {

        DataType dataType = header.getDataType();
        if ((dataType != DataType.CHAR8) && (dataType != DataType.UCHAR8)) {
            header.setDataType(DataType.CHAR8);
        }

        clearData();
        appendByteData(data);
    }

    /**
     * Set the data in this structure to the bytes contained in the given ByteBuffer.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#UCHAR8} or {@link DataType#CHAR8},
     * it will be reset in the header to {@link DataType#CHAR8}.
     *
     * @param byteData the byte data in ByteBuffer form.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setByteData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 1) {
            return;
        }

        setByteData(ByteDataTransformer.toByteArray(byteData));
    }

    /**
     * Set the data in this structure to the given array of floats.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#FLOAT32}, it will be reset in the
     * header to that type.
     *
     * @param data the float data to set to.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setFloatData(float data[]) throws EvioException {

        DataType dataType = header.getDataType();
        if (dataType != DataType.FLOAT32) {
            header.setDataType(DataType.FLOAT32);
        }

        clearData();
        appendFloatData(data);
    }

    /**
     * Set the data in this structure to the floats contained in the given ByteBuffer.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#FLOAT32}, it will be reset in the
     * header to that type.
     *
     * @param byteData the float data in ByteBuffer form.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setFloatData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 1) {
            return;
        }

        setFloatData(ByteDataTransformer.toFloatArray(byteData));
    }

    /**
     * Set the data in this structure to the given array of doubles.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#DOUBLE64}, it will be reset in the
     * header to that type.
     *
     * @param data the double data to set to.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setDoubleData(double data[]) throws EvioException {

        DataType dataType = header.getDataType();
        if (dataType != DataType.DOUBLE64) {
            header.setDataType(DataType.DOUBLE64);
        }

        clearData();
        appendDoubleData(data);
    }

    /**
     * Set the data in this structure to the doubles contained in the given ByteBuffer.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#DOUBLE64}, it will be reset in the
     * header to that type.
     *
     * @param byteData the double data in ByteBuffer form.
     * @throws EvioException if data has too many elements to store in raw byte array (JVM limit)
     */
    public void setDoubleData(ByteBuffer byteData) throws EvioException {

        if (byteData == null || byteData.remaining() < 1) {
            return;
        }

        setDoubleData(ByteDataTransformer.toDoubleArray(byteData));
    }

    /**
     * Set the data in this structure to the given array of Strings.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#CHARSTAR8},it will be reset in the
     * header to that type.
     *
     * @param data the String data to set to.
     */
    public void setStringData(String data[]) {

        DataType dataType = header.getDataType();
        if (dataType != DataType.CHARSTAR8) {
            header.setDataType(DataType.CHARSTAR8);
        }

        clearData();
        try {appendStringData(data);}
        catch (EvioException e) {/* never happen */}
    }

    /**
     * Set the data in this structure to the given array of CompositeData objects.
     * All previously existing data will be gone. If the previous data
     * type was not {@link DataType#COMPOSITE}, it will be reset in the
     * header to that type.
     *
     * @param data the array of CompositeData objects to set to.
     * @throws EvioException if data uses too much memory to store in raw byte array (JVM limit)
     */
    public void setCompositeData(CompositeData data[]) throws EvioException {

        DataType dataType = header.getDataType();
        if (dataType != DataType.COMPOSITE) {
            header.setDataType(DataType.COMPOSITE);
        }

        clearData();
        appendCompositeData(data);
    }

}
