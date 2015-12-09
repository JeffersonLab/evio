package org.jlab.coda.jevio;

import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.StringWriter;
import java.nio.*;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.IllegalFormatException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This class contains generally useful methods.
 * @author timmer
 * Date: 6/11/13
 */
public class Utilities {

    /**
     * This method generates part of a file name given a base file name as an argument.<p>
     *
     * The base file name may contain up to 2, C-style integer format specifiers
     * (such as <b>%03d</b>, or <b>%x</b>). If more than 2 are found, an exception
     * will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the returned string.
     * In the {@link #generateFileName(String, int, int, long, int)} method, the first
     * occurrence will be substituted with the given runNumber value.
     * If the file is being split, the second will be substituted with the split number.<p>
     *
     * The base file name may contain characters of the form <b>$(ENV_VAR)</b>
     * which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.<p>
     *
     * Finally, the base file name may contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.
     *
     * @param baseName        file name to start with
     * @param runType         run type/configuration name
     * @param newNameBuilder  object which contains generated base file name
     * @return                number of C-style int format specifiers found in baseName arg
     * @throws EvioException  if baseName arg is improperly formatted;
     *                        if baseName or newNameBuilder arg is null
     */
    public static int generateBaseFileName(String baseName, String runType,
                                           StringBuilder newNameBuilder)
            throws EvioException {

        String baseFileName;

        if (baseName == null || newNameBuilder == null) {
            throw new EvioException("null arg(s)");
        }

        // Replace all %s occurrences with run type
        baseName = (runType == null)?  baseName.replace("%s", "") :
                                       baseName.replace("%s", runType);

        // Scan for environmental variables of the form $(xxx)
        // and substitute the values for them (blank string if not found)
        if (baseName.contains("$(")) {
            Pattern pattern = Pattern.compile("\\$\\((.*?)\\)");
            Matcher matcher = pattern.matcher(baseName);
            StringBuffer result = new StringBuffer(100);

            while (matcher.find()) {
                String envVar = matcher.group(1);
                String envVal = System.getenv(envVar);
                if (envVal == null) envVal = "";
//System.out.println("generateBaseFileName: replacing " + envVar + " with " + envVal);
                matcher.appendReplacement(result, envVal);
            }
            matcher.appendTail(result);
//System.out.println("generateBaseFileName: Resulting string = " + result);
            baseFileName = result.toString();
        }
        else {
            baseFileName = baseName;
        }

        // How many C-style int specifiers are in baseFileName?
        Pattern pattern = Pattern.compile("%(\\d*)([xd])");
        Matcher matcher = pattern.matcher(baseFileName);
        StringBuffer result = new StringBuffer(100);

        int specifierCount = 0;
        while (matcher.find()) {
            String width = matcher.group(1);
            // Make sure any number preceding "x" or "d" starts with a 0
            // or else there will be empty spaces in the file name.
            if (width.length() > 0 && !width.startsWith("0")) {
                String newWidth = "0" + width;
//System.out.println("generateFileName: replacing " + width + " with " + newWidth);
                matcher.appendReplacement(result, "%" + newWidth + matcher.group(2));
            }

            specifierCount++;
        }
        matcher.appendTail(result);
        baseFileName = result.toString();

        if (specifierCount > 2) {
            throw new EvioException("baseName arg is improperly formatted");
        }

        // Return the base file name
        newNameBuilder.delete(0, newNameBuilder.length()).append(baseFileName);

        // Return the number of C-style int format specifiers
        return specifierCount;
    }


    /**
     * This method generates a complete file name from the previously determined baseFileName
     * obtained from calling {@link #generateBaseFileName(String, String, StringBuilder)}.
     * If evio data is to be split up into multiple files (split > 0), numbers are used to
     * distinguish between the split files with splitNumber.
     * If baseFileName contains C-style int format specifiers (specifierCount > 0), then
     * the first occurrence will be substituted with the given runNumber value.
     * If the file is being split, the second will be substituted with the splitNumber.
     * If 2 specifiers exist and the file is not being split, no substitutions are made.
     * If no specifier for the splitNumber exists, it is tacked onto the end of the file name.
     *
     * @param baseFileName   file name to use as a basis for the generated file name
     * @param specifierCount number of C-style int format specifiers in baseFileName arg
     * @param runNumber      CODA run number
     * @param split          number of bytes at which to split off evio file
     * @param splitNumber    number of the split file
     *
     * @return generated file name
     *
     * @throws IllegalFormatException if the baseFileName arg contains printing format
     *                                specifiers which are not compatible with integers
     *                                and interfere with formatting
     */
    public static String generateFileName(String baseFileName, int specifierCount,
                                          int runNumber, long split, int splitNumber)
                        throws IllegalFormatException {

        String fileName = baseFileName;

        // If we're splitting files ...
        if (split > 0L) {
            // For no specifiers: tack split # on end of base file name
            if (specifierCount < 1) {
                fileName = baseFileName + "." + splitNumber;
            }
            // For 1 specifier: insert run # at specified location, then tack split # on end
            else if (specifierCount == 1) {
                fileName = String.format(baseFileName, runNumber);
                fileName += "." + splitNumber;
            }
            // For 2 specifiers: insert run # and split # at specified locations
            else {
                fileName = String.format(baseFileName, runNumber, splitNumber);
            }
        }
        // If we're not splitting files ...
        else {
            if (specifierCount == 1) {
                // Insert runNumber
                fileName = String.format(baseFileName, runNumber);
            }
            else if (specifierCount == 2) {
                // First get rid of the extra (last) int format specifier as no split # exists
                Pattern pattern = Pattern.compile("(%\\d*[xd])");
                Matcher matcher = pattern.matcher(fileName);
                StringBuffer result = new StringBuffer(100);

                if (matcher.find()) {
                    // Only look at second int format specifier
                    if (matcher.find()) {
                        matcher.appendReplacement(result, "");
                        matcher.appendTail(result);
                        fileName = result.toString();
//System.out.println("generateFileName: replacing last int specifier =  " + fileName);
                    }
                }

                // Insert runNumber into first specifier
                fileName = String.format(fileName, runNumber);
            }
        }

        return fileName;
    }


    /**
     * This method takes a byte buffer and saves it to the specified file,
     * overwriting whatever is there.
     *
     * @param fileName       name of file to write
     * @param buf            buffer to write to file
     * @param overWriteOK    if {@code true}, OK to overwrite previously existing file
     * @param addBlockHeader if {@code true}, add evio block header for proper evio file format
     * @throws IOException   if trouble writing to file
     * @throws EvioException if file exists but overwriting is not permitted;
     *                       if null arg(s)
     */
    public static void bufferToFile(String fileName, ByteBuffer buf,
                                    boolean overWriteOK, boolean addBlockHeader)
            throws IOException, EvioException{

        if (fileName == null || buf == null) {
            throw new EvioException("null arg(s)");
        }

        File file = new File(fileName);

        // If we can't overwrite and file exists, throw exception
        if (!overWriteOK && (file.exists() && file.isFile())) {
            throw new EvioException("File exists but over-writing not permitted, "
                    + file.getPath());
        }

        int limit    = buf.limit();
        int position = buf.position();

        FileOutputStream fileOutputStream = new FileOutputStream(file);
        FileChannel fileChannel = fileOutputStream.getChannel();

        if (addBlockHeader) {
            ByteBuffer blockHead = ByteBuffer.allocate(32);
            blockHead.order(buf.order());
            blockHead.putInt(8 + (limit - position)/4);   // total len of block in words
            blockHead.putInt(1);                          // block number
            blockHead.putInt(8);                          // header len in words
            blockHead.putInt(1);                          // event count
            blockHead.putInt(0);                          // reserved
            blockHead.putInt(0x204);                      // last block, version 4
            blockHead.putInt(0);                          // reserved
            blockHead.putInt(BlockHeaderV4.MAGIC_NUMBER); // 0xcoda0100
            blockHead.flip();
            fileChannel.write(blockHead);
        }

        fileChannel.write(buf);
        fileChannel.close();
        buf.limit(limit).position(position);
    }


    /**
     * This method takes a byte buffer and prints out the desired number of words
     * from the given position.
     *
     * @param buf            buffer to print out
     * @param position       position of data (bytes) in buffer to start printing
     * @param words          number of 32 bit words to print in hex
     * @param label          a label to print as header
     */
    synchronized public static void printBuffer(ByteBuffer buf, int position, int words, String label) {

        if (buf == null) {
            System.out.println("printBuffer: buf arg is null");
            return;
        }

        int origPos = buf.position();
        buf.position(position);

        if (label != null) System.out.println(label + ":");

        IntBuffer ibuf = buf.asIntBuffer();
        words = words > ibuf.capacity() ? ibuf.capacity() : words;
        for (int i=0; i < words; i++) {
            System.out.println("  Buf(" + i + ") = 0x" + Integer.toHexString(ibuf.get(i)));
        }
        System.out.println();

        buf.position(origPos);
   }


    /**
     * This method takes an EvioNode object and converts it to an EvioEvent object.
     * @param node EvioNode object to EvioEvent
     * @throws EvioException if node is not a bank or cannot parse node's buffer
     */
    public static EvioEvent nodeToEvent(EvioNode node) throws EvioException {

        if (node == null) {
            return null;
        }

        if (node.getTypeObj() != DataType.ALSOBANK &&
            node.getTypeObj() != DataType.BANK) {
            throw new EvioException("node is not a bank");
        }

        // Easiest way to do this is have a reader parse the node's backing buffer.
        // First create a buffer with space for event and an evio block header.
        int totalLen = node.len + 1 + 8;
        ByteBuffer buf = ByteBuffer.allocate(4*totalLen);

        // Get copy of node's backing buffer (evio bank header and data)
        ByteBuffer eventBuf = node.getStructureBuffer(true);

        // Make sure all data in buf is the same endian
        buf.order(eventBuf.order());

        // Fill in block header
        buf.putInt(totalLen);
        buf.putInt(1);
        buf.putInt(8);
        buf.putInt(1);
        buf.putInt(0);
        buf.putInt(0x204);
        buf.putInt(0);
        buf.putInt(0xc0da0100);

        // Fill in event bytes.
        buf.put(eventBuf);
        buf.flip();

        EvioEvent event;

        try {
            EvioReader reader = new EvioReader(buf);
            event = reader.parseEvent(1);
        }
        catch (IOException e) {
            e.printStackTrace();
            throw new EvioException("cannot parse node's buffer");
        }

        return event;
    }


    /*---------------------------------------------------------------------------
     *  EvioNode XML output
     *--------------------------------------------------------------------------*/


    /**
     * This method takes an EvioNode object and converts it to a readable, XML String.
     * @param node EvioNode object to print out
     * @throws EvioException if node is not a bank or cannot parse node's buffer
     */
    public static String toXML(EvioNode node) throws EvioException {

        if (node == null) {
            return null;
        }

        StringWriter sWriter = null;
        XMLStreamWriter xmlWriter = null;
        try {
            sWriter = new StringWriter();
            xmlWriter = XMLOutputFactory.newInstance().createXMLStreamWriter(sWriter);
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }

        String xmlIndent = "";
        nodeToString(node, xmlIndent, xmlWriter);

        return sWriter.toString();
    }


    /**
     * Increase the indentation of the given XML indentation.
     * @param xmlIndent String of spaces to increase.
     * @return increased indentation String
     */
    static String increaseXmlIndent(String xmlIndent) {
        xmlIndent += "   ";
        return xmlIndent;
    }


    /**
     * Decrease the indentation of the given XML indentation.
     * @param xmlIndent String of spaces to increase.
     * @return Decreased indentation String
     */
    static String decreaseXmlIndent(String xmlIndent) {
        xmlIndent = xmlIndent.substring(0, xmlIndent.length() - 3);
        return xmlIndent;
    }


    /**
     * All structures have a common start to their xml writing
     * @param xmlWriter the writer used to write the node.
     * @param xmlElementName name of xml element to start.
     * @param xmlIndent String of spaces.
     */
    static private void commonXMLStart(XMLStreamWriter xmlWriter, String xmlElementName,
                                       String xmlIndent) {
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
     * All structures close their xml writing.
     * @param xmlWriter the writer used to write the node.
     * @param xmlIndent String of spaces.
     */
    static private void commonXMLClose(XMLStreamWriter xmlWriter, String xmlIndent) {
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
     * This recursive method takes an EvioNode object and
     * converts it to a readable, XML format String.
     * @param node EvioNode object to print out.
     * @param xmlIndent String of spaces.
     * @param xmlWriter the writer used to write the node.
     */
    static private void nodeToString(EvioNode node, String xmlIndent,
                                     XMLStreamWriter xmlWriter) {

        DataType nodeType = node.getTypeObj();
        DataType dataType = node.getDataTypeObj();

        try {
            // If top level ...
            if (node.isEvent) {
                int totalLen = node.getLength() + 1;
                xmlIndent = increaseXmlIndent(xmlIndent);
                xmlWriter.writeCharacters(xmlIndent);
                xmlWriter.writeComment(" Buffer " + node.getEventNumber() + " contains " + totalLen + " words (" + 4*totalLen + " bytes)");
                commonXMLStart(xmlWriter, "event", xmlIndent);
                xmlWriter.writeAttribute("format", "evio");
                xmlWriter.writeAttribute("count", "" + node.getEventNumber());
                if (node.getDataTypeObj().isStructure()) {
                    xmlWriter.writeAttribute("content", getTypeName(dataType));
                }
                xmlWriter.writeAttribute("data_type", String.format("0x%x", node.getDataType()));
                xmlWriter.writeAttribute("tag", "" + node.getTag());
                xmlWriter.writeAttribute("num", "" + node.getNum());
                xmlWriter.writeAttribute("length", "" + node.getLength());
                xmlIndent = increaseXmlIndent(xmlIndent);

                for (EvioNode n : node.childNodes) {
                    // Recursive call
                    nodeToString(n, xmlIndent, xmlWriter);
                }

                xmlIndent = decreaseXmlIndent(xmlIndent);
                commonXMLClose(xmlWriter, xmlIndent);
                xmlWriter.writeCharacters("\n");
                xmlIndent = decreaseXmlIndent(xmlIndent);
            }

            // If bank, segment, or tagsegment ...
            else if (dataType.isStructure()) {
                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(xmlIndent);
                xmlWriter.writeStartElement(getTypeName(nodeType));

                if (node.getDataTypeObj().isStructure()) {
                    xmlWriter.writeAttribute("content",  getTypeName(dataType));
                }
                xmlWriter.writeAttribute("data_type", String.format("0x%x", node.getDataType()));
                xmlWriter.writeAttribute("tag", "" + node.getTag());
                if (nodeType == DataType.BANK || nodeType == DataType.ALSOBANK) {
                    xmlWriter.writeAttribute("num", "" + node.getNum());
                }
                xmlWriter.writeAttribute("length", "" + node.getLength());
                xmlWriter.writeAttribute("ndata", "" + getNumberDataItems(node));

                ArrayList<EvioNode> childNodes = node.getChildNodes();
                if (childNodes != null) {
                    xmlIndent = increaseXmlIndent(xmlIndent);

                    for (EvioNode n : childNodes) {
                        // Recursive call
                        nodeToString(n, xmlIndent, xmlWriter);
                    }

                    xmlIndent = decreaseXmlIndent(xmlIndent);
                }
                commonXMLClose(xmlWriter, xmlIndent);
            }

            // If structure containing data ...
            else {
                int count;
                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(xmlIndent);
                xmlWriter.writeStartElement(getTypeName(dataType));
                xmlWriter.writeAttribute("data_type", String.format("0x%x", node.getDataType()));
                xmlWriter.writeAttribute("tag", "" + node.getTag());
                if (nodeType == DataType.BANK || nodeType == DataType.ALSOBANK) {
                    xmlWriter.writeAttribute("num", "" + node.getNum());
                }
                xmlWriter.writeAttribute("length", "" + node.getLength());
                count = getNumberDataItems(node);
                xmlWriter.writeAttribute("ndata", "" + count);

                xmlIndent = increaseXmlIndent(xmlIndent);
                writeXmlData(node, count, xmlIndent, xmlWriter);
                xmlIndent = decreaseXmlIndent(xmlIndent);

                commonXMLClose(xmlWriter, xmlIndent);
            }
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }
    }


    /**
     * Get the number of stored data items like number of banks, ints, floats, etc.
     * (not the size in ints or bytes). Some items may be padded such as shorts
     * and bytes. This will tell the meaningful number of such data items.
     * In the case of containers, returns number of 32-bit words not in header.
     *
     * @param node node to examine
     * @return number of stored data items (not size or length),
     *         or number of bytes if container
     */
    static private int getNumberDataItems(EvioNode node) {
        int numberDataItems = 0;

        if (node.getDataTypeObj().isStructure()) {
            numberDataItems = node.getDataLength();
        }
        else {
            // We can figure out how many data items
            // based on the size of the raw data
            DataType type = node.getDataTypeObj();

            switch (type) {
                case UNKNOWN32:
                case CHAR8:
                case UCHAR8:
                    numberDataItems = (4*node.getDataLength() - node.getPad());
                    break;
                case SHORT16:
                case USHORT16:
                    numberDataItems = (4*node.getDataLength() - node.getPad())/2;
                    break;
                case INT32:
                case UINT32:
                case FLOAT32:
                    numberDataItems = node.getDataLength();
                    break;
                case LONG64:
                case ULONG64:
                case DOUBLE64:
                    numberDataItems = node.getDataLength()/2;
                    break;
                case CHARSTAR8:
                    String[] s = BaseStructure.unpackRawBytesToStrings(
                                    node.bufferNode.buffer, node.dataPos, 4*node.dataLen);

                    if (s == null) {
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
                    CompositeData[] compositeData = null;
                    try {
                        // Data needs to be in a byte array
                        byte[] rawBytes = new byte[4*node.dataLen];
                        // Wrap array with ByteBuffer for easy transfer of data
                        ByteBuffer rawBuffer = ByteBuffer.wrap(rawBytes);

                        // The node's backing buffer may or may not have a backing array.
                        // Just assume it doesn't and proceed from there.
                        ByteBuffer buf = node.getByteData(true);

                        // Write data into array
                        rawBuffer.put(buf).order(buf.order());

                        compositeData = CompositeData.parse(rawBytes, rawBuffer.order());
                    }
                    catch (EvioException e) {
                        e.printStackTrace();
                    }
                    if (compositeData != null) numberDataItems = compositeData.length;
                    break;
                default:
            }
        }

        return numberDataItems;
    }


    /**
     * This method returns an XML element name given an evio data type.
     * @param type evio data type
     * @return XML element name used in evio event xml output
     */
    static private String getTypeName(DataType type) {

        if (type == null) return null;

        switch (type) {
            case CHAR8:
                return "int8";
            case UCHAR8:
                return "uint8";
            case SHORT16:
                return "int16";
            case USHORT16:
                return "uint16";
            case INT32:
                return "int32";
            case UINT32:
                return "uint32";
            case LONG64:
                return "int64";
            case ULONG64:
                return "uint64";
            case FLOAT32:
                return "float32";
            case DOUBLE64:
                return "float64";
            case CHARSTAR8:
                return "string";
            case COMPOSITE:
                return "composite";
            case UNKNOWN32:
                return "unknown32";

            case TAGSEGMENT:
                return "tagsegment";

            case SEGMENT:
            case ALSOSEGMENT:
                return "segment";

            case BANK:
            case ALSOBANK:
                return "bank";

            default:
                return "unknown";
        }
    }


    /**
  	 * Write data as XML output for EvioNode.
  	 * @param node the EvioNode object whose data is to be converted to String form.
     * @param count number of data items (shorts, longs, doubles, etc.) in this node
     *              (not children)
     * @param xmlIndent String of spaces.
     * @param xmlWriter the writer used to write the node.
  	 */
  	static private void writeXmlData(EvioNode node, int count, String xmlIndent,
                                     XMLStreamWriter xmlWriter) {

  		// Only leaves write data
  		if (node.getDataTypeObj().isStructure()) {
  			return;
  		}

        try {
            String s;
            String indent = String.format("\n%s", xmlIndent);

            ByteBuffer buf = node.getByteData(true);

  			switch (node.getDataTypeObj()) {
  			case DOUBLE64:
  				DoubleBuffer dbuf = buf.asDoubleBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%24.16e  ", dbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case FLOAT32:
                FloatBuffer fbuf = buf.asFloatBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%14.7e  ", fbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case LONG64:
  			case ULONG64:
                LongBuffer lbuf = buf.asLongBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%20d  ", lbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case INT32:
  			case UINT32:
                IntBuffer ibuf = buf.asIntBuffer();
                for (int i=0; i < count; i++) {
                    if (i%5 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%11d  ", ibuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case SHORT16:
  			case USHORT16:
                ShortBuffer sbuf = buf.asShortBuffer();
                for (int i=0; i < count; i++) {
                    if (i%5 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%6d  ", sbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

            case UNKNOWN32:
  			case CHAR8:
  			case UCHAR8:
                for (int i=0; i < count; i++) {
                    if (i%5 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%4d  ", buf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case CHARSTAR8:
                String[] stringdata = BaseStructure.unpackRawBytesToStrings(
                        node.bufferNode.buffer, node.dataPos, 4 * node.dataLen);

                if (stringdata == null) {
                    break;
                }

                for (String str : stringdata) {
                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCData(str);
                }
  				break;

              case COMPOSITE:
                  CompositeData[] compositeData;
                  // Data needs to be in a byte array
                  byte[] rawBytes = new byte[4*node.dataLen];
                  // Wrap array with ByteBuffer for easy transfer of data
                  ByteBuffer rawBuffer = ByteBuffer.wrap(rawBytes);

                  // Write data into array
                  rawBuffer.put(buf).order(buf.order());

                  compositeData = CompositeData.parse(rawBytes, buf.order());
                  if (compositeData != null) {
                      for (CompositeData cd : compositeData) {
                          cd.toXML(xmlWriter, xmlIndent, true);
                      }
                  }
                  break;

              default:
              }
  		}
  		catch (Exception e) {
  			e.printStackTrace();
  		}
  	}

}
