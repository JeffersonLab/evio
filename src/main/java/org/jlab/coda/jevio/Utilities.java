/*
 * Copyright (c) 2016, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio;

import org.jlab.coda.hipo.RecordHeader;

import javax.xml.namespace.QName;
import javax.xml.stream.*;
import javax.xml.stream.events.*;
import java.io.*;
import java.math.BigInteger;
import java.nio.*;
import java.nio.channels.FileChannel;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This class contains generally useful methods.
 * @author timmer
 * Date: 6/11/13
 */
final public class Utilities {


    // Some methods to help deal with padding data to 4-byte boundaries

    /** Array to help find number of bytes to pad data. */
    static public int[] padValue = {0,3,2,1};


    /**
     * Returns length padded to 4-byte boundary for given length in bytes.
     * @param length length in bytes.
     * @return length in bytes padded to 4-byte boundary.
     */
    static public int getWords(int length) {
        int words = length/4;
        if (getPadding(length) > 0) words++;
        return words;
    }


    /**
     * Returns number of bytes needed to pad to 4-byte boundary for the given length.
     * @param length length in bytes.
     * @return number of bytes needed to pad to 4-byte boundary.
     */
    static public int getPadding(int length) {return padValue[length%4];}


    /**
     * Get the evio version of the given file.
     * @param file file name.
     * @return the evio version of the given file.
     * @throws EvioException if version cannot be found.
     */
    static public int getEvioVersion(File file) throws EvioException {
        // try with resources, close automatically called
        try (RandomAccessFile rFile = new RandomAccessFile(file, "r")) {
            // Read first 32 bytes of file header
            ByteBuffer byteBuffer = ByteBuffer.wrap(new byte[32]);
            rFile.read(byteBuffer.array());
            return getEvioVersion(byteBuffer);
        } catch (IOException ex) {
            throw new EvioException(ex);
        }
    }


    /**
     * Get the evio version of the given evio header in ByteBuffer form.
     * The buffer can contain any evio version and be a file, record, or block header.
     * Side effect is that the headerData arg is set to the correct endian order.
     * @param headerData ByteBuffer object with at least 8 words (32 bytes) of data.
     * @return the evio version of the given data.
     * @throws EvioException if version cannot be found.
     */
    static public int getEvioVersion(ByteBuffer headerData) throws EvioException {

        // Have enough remaining bytes to read 8 words of header?
        if (headerData.limit() < 32) {
            throw new EvioException("data must contain at least 32 bytes");
        }

        // Set the byte order to match the file's ordering.

        // Check the magic number for endianness (buffer defaults to big endian)
        ByteOrder byteOrder = headerData.order();

        int magicNumber = headerData.getInt(RecordHeader.MAGIC_OFFSET);
        if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
            if (byteOrder == ByteOrder.BIG_ENDIAN) {
                byteOrder = ByteOrder.LITTLE_ENDIAN;
            }
            else {
                byteOrder = ByteOrder.BIG_ENDIAN;
            }
            headerData.order(byteOrder);

            // Reread magic number to make sure things are OK
            magicNumber = headerData.getInt(RecordHeader.MAGIC_OFFSET);
            if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
                throw new EvioException("magic number cannot be read from data");
            }
        }

        // Find the version number
        int bitInfo = headerData.getInt(RecordHeader.BIT_INFO_OFFSET);
        return bitInfo & RecordHeader.VERSION_MASK;
    }


    /**
     * <p>This method generates part of a file name given a base file name as an argument.</p>
     *
     * <p>The base file name may contain up to 3, C-style integer format specifiers
     * using "d" and "x" (such as <b>%03d</b>, or <b>%x</b>).
     * If more than 3 are found, an exception will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the returned string.
     * See the {@link #generateFileName(String, int, int, long, int, int, int)}
     * documentation to understand the exact way that run number,
     * split number and stream id are substituted into the specifiers.</p>
     *
     * <p>The base file name may contain characters of the form <b>$(ENV_VAR)</b>
     * which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.</p>
     *
     * Finally, the base file name may contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.
     *
     * @param baseName        file name to start with
     * @param runType         run type/configuration name
     * @param newNameBuilder  object which contains generated base file name
     * @return                number of C-style int format
     *                        specifiers found in baseName arg.
     * @throws EvioException  if baseName arg is improperly formatted;
     *                        if baseName or newNameBuilder arg is null
     */
    static public int generateBaseFileName(String baseName, String runType,
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

        if (specifierCount > 3) {
            throw new EvioException("baseName arg is improperly formatted");
        }

        // Return the base file name
        newNameBuilder.delete(0, newNameBuilder.length()).append(baseFileName);

        // Return # of C-style int format specifiers
        return specifierCount;
    }


    /**
     * <p>This method generates a complete file name from the previously determined baseFileName
     * obtained from calling {@link #generateBaseFileName(String, String, StringBuilder)}.
     * If evio data is to be split up into multiple files (split &gt; 0), numbers are used to
     * distinguish between the split files with splitNumber.</p>
     *
     * <p>The given fileName may contain up to 3, C-style int format specifiers which will be substituted
     * with runNumber, splitNumber and streamId in the manner described below.</p>
     *
     * <ul>
     *   <li>If file is to be split:
     *     <ul>
     *       <li>If no specifiers:
     *         <ul>
     *           <li>for one stream, splitNumber is tacked onto the end of the file name as <b>.&lt;splitNumber&gt;</b></li>
     *           <li>for multiple streams, streamId and splitNumber are tacked onto the end of the file name
     *               as <b>.&lt;streamId&gt;.&lt;splitNumber&gt;</b></li>
     *           <li>No run numbers are ever tacked on without a specifier </li>
     *         </ul>
     *       </li>
     *       <li>If 1 specifier:
     *         <ul>
     *           <li>add runNumber according to specifier</li>
     *           <li>for one stream, splitNumber is tacked onto the end of the file name as <b>.&lt;splitNumber&gt;</b></li>
     *           <li>for multiple streams, streamId and splitNumber are tacked onto the end of the file name
     *               as <b>.&lt;streamId&gt;.&lt;splitNumber&gt;</b></li>
     *         </ul>
     *       </li>
     *       <li>If 2 specifiers:
     *         <ul>
     *           <li>add runNumber according to first specifier</li>
     *           <li>for one stream, add splitNumber according to second specifier</li>
     *           <li>for multiple streams, add splitNumber according to second specifier, but place
     *               <b>&lt;streamId&gt;.</b> just before the splitNumber</li>
     *         </ul>
     *       </li>
     *       <li>If 3 specifiers:
     *         <ul>
     *           <li>add runNumber according to first specifier</li>
     *           <li>add streamId according to second specifier add splitNumber according to third specifier</li>
     *         </ul>
     *       </li>
     *     </ul>
     *   </li>
     *   <li>If file is NOT split:
     *     <ul>
     *       <li>If no specifiers:
     *         <ul>
     *           <li>streamId is tacked onto the end of the file name as <b>.&lt;streamId&gt;</b></li>
     *           <li>No run numbers are ever tacked on without a specifier.</li>
     *         </ul>
     *       </li>
     *       <li>If 1 specifier:
     *         <ul>
     *           <li>add runNumber according to specifier</li>
     *           <li>for multiple streams, streamId is tacked onto the end of the file name as .<b>&lt;streamId&gt;</b></li>
     *         </ul>
     *       </li>
     *       <li>If 2 specifiers:
     *         <ul>
     *           <li>add runNumber according to first specifier</li>
     *           <li>remove second specifier</li>
     *           <li>for multiple streams, streamId is tacked onto the end of the file name as <b>.&lt;streamId&gt;</b></li>
     *         </ul>
     *       </li>
     *       <li>If 3 specifiers:
     *         <ul>
     *           <li>add runNumber according to first specifier</li>
     *           <li>add streamId according to second specifier</li>
     *           <li>remove third specifier</li>
     *         </ul>
     *       </li>
     *     </ul>
     *   </li>
     * </ul>
     *
     * If there are more than 3 specifiers, <b>NO SUBSTITUTIONS ARE DONE on the extra specifiers</b>.
     *
     * @param fileName       file name to use as a basis for the generated file name.
     * @param specifierCount number of C-style int format specifiers in baseFileName arg.
     * @param runNumber      CODA run number.
     * @param split          number of bytes at which to split off evio file.
     * @param splitNumber    number of the split file.
     * @param streamId       number of the stream id.
     * @param streamCount    total number of streams.
     *
     * @return generated file name
     *
     * @throws IllegalFormatException if the baseFileName arg contains printing format
     *                                specifiers which are not compatible with integers
     *                                and interfere with formatting.
     */
    static public String generateFileName(String fileName, int specifierCount,
                                                int runNumber, long split, int splitNumber,
                                                int streamId, int streamCount)
            throws IllegalFormatException {

        if (streamCount < 1) streamCount = 1;
        if (splitNumber < 1) splitNumber = 0;
        if (runNumber < 0)   runNumber = 0;
        if (streamId < 0)    streamId = 0;
        boolean oneStream = streamCount < 2;

        if (fileName == null || fileName.length() < 1) {
            fileName = "file";
        }

//System.out.println("generateFileName: split# = " + splitNumber + ", start with    " + fileName +
//",    streamId = " + streamId + ", stream count = " + streamCount + ", one stream = " + oneStream);
        // NOTE: no run #s are tacked on the end!

        // If we're splitting files which is always the case of CODA users ...
        if (split > 0L) {
            // For no specifiers:  tack stream id and split # onto end of file name
            if (specifierCount < 1) {
                if (oneStream) {
                    fileName += "." + splitNumber;
                }
                else {
                    fileName += "." + streamId + "." + splitNumber;
                }
            }
            // For 1 specifier: insert run # at specified location,
            // then tack stream id and split # onto end of file name
            else if (specifierCount == 1) {
                fileName = String.format(fileName, runNumber);
                if (oneStream) {
                    fileName += "." + splitNumber;
                }
                else {
                    fileName += "." + streamId + "." + splitNumber;
                }
            }
            // For 2 specifiers: insert run # and split # at specified locations
            // and place stream id immediately before split #.
            else if (specifierCount == 2) {
                if (!oneStream) {
                    // In order to place streamId before split#, place a %d in the filename
                    // immediately before 2nd specifier.
                    Pattern pattern = Pattern.compile("(%\\d*[xd])");
                    Matcher matcher = pattern.matcher(fileName);
                    StringBuffer result = new StringBuffer(100);

                    if (matcher.find()) {
                        // Only look at 2nd int format specifier.
                        // Replace it with "%d.<match>"
                        if (matcher.find()) {
                            matcher.appendReplacement(result, "%d.$1");
                            matcher.appendTail(result);
                            fileName = result.toString();
                        }
                    }

                    fileName = String.format(fileName, runNumber, streamId, splitNumber);
                }
                else {
                    fileName = String.format(fileName, runNumber, splitNumber);
                }
            }
            // For 3 specifiers: insert run #, stream id, and split # at specified locations
            else if (specifierCount == 3) {
                fileName = String.format(fileName, runNumber, streamId, splitNumber);
            }

        }
        // If we're not splitting files, then CODA isn't being used and stream id is
        // probably meaningless.
        else {
            // For no specifiers:  tack stream id onto end of file name
            if (specifierCount < 1) {
                if (!oneStream) {
                    fileName += "." + streamId;
                }
            }
            else if (specifierCount == 1) {
                // Insert runNumber
                fileName = String.format(fileName, runNumber);
                if (!oneStream) {
                    fileName += "." + streamId;
                }
            }
            else if (specifierCount == 2) {
                // First get rid of the extra (2nd) int format specifier as no split # exists
                Pattern pattern = Pattern.compile("(%\\d*[xd])");
                Matcher matcher = pattern.matcher(fileName);
                StringBuffer result = new StringBuffer(100);

                if (matcher.find()) {
                    // Only look at second int format specifier
                    if (matcher.find()) {
                        matcher.appendReplacement(result, "");
                        matcher.appendTail(result);
                        fileName = result.toString();
                    }
                }

                // Insert runNumber into first specifier
                fileName = String.format(fileName, runNumber);
                if (!oneStream) {
                    fileName += "." + streamId;
                }
            }
            else if (specifierCount == 3) {
                // Get rid of extra (3rd) int format specifier as no split # exists
                Pattern pattern = Pattern.compile("(%\\d*[xd])");
                Matcher matcher = pattern.matcher(fileName);
                StringBuffer result = new StringBuffer(100);

                if (matcher.find()) {
                    if (matcher.find()) {
                        // Remove 3rd int format specifier
                        if (matcher.find()) {
                            matcher.appendReplacement(result, "");
                            matcher.appendTail(result);
                            fileName = result.toString();
                        }
                    }
                }

                // Insert runNumber into first specifier, stream id into 2nd
                fileName = String.format(fileName, runNumber, streamId);
            }
        }
//System.out.println("generateFileName: end with    " + fileName);

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
    static public void bufferToFile(String fileName, ByteBuffer buf,
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


    /** This method prints out the current stack trace. */
    static public void printStackTrace() {
        for (StackTraceElement ste : Thread.currentThread().getStackTrace()) {
            System.out.println(ste);
        }
    }


    /**
     * This method takes a byte buffer and prints out the desired number of words
     * from the given position. Will <b>not</b> print out the extra 1, 2, or 3
     * bytes left at the end of the buffer. NOT thread-safe.
     *
     * @param buf       buffer to print out
     * @param position  position of data (bytes) in buffer to start printing
     * @param words     number of 32 bit words to print in hex
     * @param label     a label to print as header
     */
    static public void printBuffer(ByteBuffer buf, int position, int words, String label) {

        if (buf == null) {
            System.out.println("printBuffer: buf arg is null");
            return;
        }

        if (position >= buf.capacity()) {
            System.out.println("position is at or past buffer's capacity");
            return;
        }

        int origPos = buf.position();
        int origLim = buf.limit();
        // set pos = 0, lim = cap
        buf.clear();
        buf.position(position);

        if (label != null) System.out.println(label + ":");

        IntBuffer ibuf = buf.asIntBuffer();
        words = words > ibuf.capacity() ? ibuf.capacity() : words;
        for (int i=0; i < words; i++) {
            if (i%5 == 0) {
                System.out.print("\n  Buf(" + (i + 1) + "-" + (i + 5) + ") =  ");
            }

            System.out.print(String.format("%08x", ibuf.get(i)) + "  ");
        }
        System.out.println();
        System.out.println();

        buf.limit(origLim).position(origPos);
   }


    /**
     * This method takes a byte buffer and prints out the desired number of bytes
     * from the given position. Prints all bytes.
     *
     * @param buf       buffer to print out
     * @param position  position of data (bytes) in buffer to start printing
     * @param bytes     number of bytes to print in hex
     * @param label     a label to print as header
     */
    static public void printBytes(ByteBuffer buf, int position, int bytes, String label) {

        if (buf == null) {
            System.out.println("printBuffer: buf arg is null");
            return;
        }

        if (position >= buf.capacity()) {
            System.out.println("position is at or past buffer's capacity");
            return;
        }

        int origPos = buf.position();
        int origLim = buf.limit();
        // set pos = 0, lim = cap
        buf.clear();

        bytes = bytes + position > buf.capacity() ? (buf.capacity() - position) : bytes;

        if (label != null) System.out.println(label + ":");

        for (int i=0; i < bytes; i++) {
            if (i%20 == 0) {
                System.out.print("\n  Buf(" + (i + 1) + "-" + (i + 20) + ") =  ");
            }
            else if (i%4 == 0) {
                System.out.print("  ");
            }

            System.out.print(String.format("%02x", buf.get(i + position)) + " ");
        }
        System.out.println();
        System.out.println();

        buf.limit(origLim).position(origPos);
   }

    /**
     * This method takes an EvioNode and prints out the desired number of bytes
     * from the backing array at the given node's position, in hex. Prints all bytes.
     *
     * @param node      node with backing buffer to print out
     * @param bytes     number of bytes to print in hex
     * @param label     a label to print as header
     */
    static public void printBytes(EvioNode node, int bytes, String label) {
        ByteBuffer buf = node.getBuffer();
        printBytes(buf, node.getPosition(), bytes, label);
    }

    /**
     * This method takes a byte array and prints out the desired number of bytes
     * from the given index. Prints all bytes.
     *
     * @param array   byte array to print out
     * @param offset  offset into array to start printing
     * @param bytes   number of bytes to print in hex
     * @param label   a label to print as header
     */
    static public void printBytes(byte[] array, int offset, int bytes, String label) {

        if (array == null) {
            System.out.println("printBuffer: array arg is null");
            return;
        }

        if (offset < 0 || bytes < 0) {
            System.out.println("printBuffer: offset and bytes args must be >= 0");
            return;
        }

        int limit = bytes + offset > array.length ? array.length : bytes + offset;

        if (label != null) System.out.println(label + ":");

        for (int i = offset; i < limit; i++) {
            if ((i - offset) %20 == 0) {
                System.out.print("\n  array[" + (i + 1) + "-" + (i + 20) + "] =  ");
            }
            else if ((i-offset) %4 == 0) {
                System.out.print("  ");
            }

            System.out.print(String.format("%02x", array[i]) + " ");
        }
        System.out.println();
        System.out.println();
    }


    /**
     * This method takes a file and prints out the desired number of bytes
     * from the given offset.
     *
     * @param fileName file to print out
     * @param offset   offset into file to start printing
     * @param bytes    number of bytes to print in hex
     * @param label    a label to print as header
     */
    static public void printBytes(String fileName, long offset, int bytes, String label) {

        if (fileName == null || fileName.length() < 1) {
            System.out.println("printBuffer: file name arg is invalid");
            return;
        }

        if (offset < 0 || bytes < 0) {
            System.out.println("printBuffer: offset and bytes args must be >= 0");
            return;
        }

        try {
            RandomAccessFile raf = new RandomAccessFile(fileName, "rw");
            long limit = bytes + offset > raf.length() ? raf.length() : bytes + offset;
            int dataLen = (int)(limit - offset);
            byte[] array = new byte[dataLen];
            raf.readFully(array);

            printBytes(array, 0, dataLen, label);
        }
        catch (IOException e) {
            e.printStackTrace();
        }
    }


    /**
     * This method reads part of a file into a ByteBuffer.
     * The buffer's position will be at the end of the data that is read.
     * The caller will have to flip the buffer to read it.
     *
     * @param fileName file to read from.
     * @param buf      buffer to write into.
     * @throws EvioException if file not found, is a directory, or could not be read;
     *                       if bad file name or unable to do I/O.
     */
    static public void readBytes(String fileName, ByteBuffer buf) throws EvioException {

        if (fileName == null || fileName.length() < 1) {
            System.out.println("printBuffer: file name arg is invalid");
            return;
        }

        try {
            File file = new File(fileName);
            FileInputStream fileStream = new FileInputStream(file);
            FileChannel fileChannel = fileStream.getChannel();

            fileChannel.read(buf);
            fileStream.close();
        }
        catch (Exception ex) {
            throw new EvioException(ex);
        }
    }


    /**
     * Method to create a byte array representing a bank with one string as data.
     * Not the most efficient implementation.
     * @param text  string to use as data.
     * @param tag   bank's tag.
     * @param num   bank's num.
     * @param order byte order of returned bank. If null use big endian.
     * @return byte array containing evio bank with string data. Null if text is null.
     */
    static public byte[] stringToBank(String text, short tag,
                                            byte num, ByteOrder order) {

        if (text  == null) return null;
        if (order == null) order = ByteOrder.BIG_ENDIAN;

        // Turn string data into bytes
        byte[] data = BaseStructure.stringsToRawBytes(new String[] {text});
        // Create room for data + 2-int header
        byte[] bank = new byte[data.length + 8];
        // Copy data into bank array
        System.arraycopy(data, 0, bank, 8, data.length);

        // Set bank header length
        int bankLen = (data.length/4 + 1);

        try {
            // Write 1st header word
            ByteDataTransformer.toBytes(bankLen, order, bank, 0);

            // Write 2nd header word
            if (order == ByteOrder.BIG_ENDIAN) {
                ByteDataTransformer.toBytes(tag, order, bank, 4);
                bank[6] = (byte)(DataType.BANK.getValue() & 0x3f); // no padding
                bank[7] = num;
            }
            else {
                bank[4] = num;
                bank[5] = (byte)(DataType.BANK.getValue() & 0x3f); // no padding
                ByteDataTransformer.toBytes(tag, order, bank, 6);
            }
        }
        catch (EvioException e) {/* never happen */}

        return bank;
    }


    /**
     * Method to create a byte array representing a bank.
     * @param bank  bank to encode.
     * @param order byte order of returned bank. If null use big endian.
     * @return byte array containing evio bank as byte array. Null if bank is null.
     */
    static public byte[] bankToBytes(EvioBank bank, ByteOrder order) {

        if (bank  == null) return null;
        if (order == null) order = ByteOrder.BIG_ENDIAN;

        byte[] bankArray = bank.toArray();

        if (order == bank.getByteOrder()) {
            return bankArray;
        }

        ByteBuffer bankBuf = ByteBuffer.wrap(bankArray);
        try {
            ByteDataTransformer.swapEvent(bankBuf, null, 0, 0);
        }
        catch (EvioException e) {/* never happen */}

        return bankArray;
    }


    /**
     * This method takes an EvioNode object and converts it to an EvioEvent object.
     * @param node EvioNode object to EvioEvent
     * @return EvioEvent object created from EvioNode arg.
     * @throws EvioException if node is not a bank or cannot parse node's buffer
     */
    static public EvioEvent nodeToEvent(EvioNode node) throws EvioException {

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


    /**
     * Return the power of 2 closest to the given argument.
     *
     * @param x value to get the power of 2 closest to.
     * @param roundUp if true, round up, else down
     * @return -1 if x is negative or the closest power of 2 to value
     */
    static public int powerOfTwo(int x, boolean roundUp) {
        if (x < 0) return -1;

        // The following algorithms are found in
        // "Hacker's Delight" by Henry Warren Jr.

        if (roundUp) {
            x = x - 1;
            x |= (x>>1);
            x |= (x>>2);
            x |= (x>>4);
            x |= (x>>8);
            x |= (x>>16);
            return x + 1;
        }

        int y;
        do {
            y = x;
            x &= (x - 1);
        } while (x != 0);
        return y;
    }


    /*---------------------------------------------------------------------------
     *  XML input
     *--------------------------------------------------------------------------*/

    /**
     * Class that keeps info about a particular xml level.
     */
    private static class EvioXmlLevel {
        /** Number of data items. */
        int nData;
        /** Header's tag value. */
        int tag = -1;
        /** Header's num value. */
        int num;

        /** Type of data as an object stored in node. */
        DataType dataTypeObj;

        /** Parent structure's data type. */
        DataType parentDataTypeObj;

        /** Container. */
        BaseStructure bs;
    }


    /**
     * This method takes a string representation of evio events
     * (each starting with lt; event gt;)
     * and converts them to a list of EvioEvent objects.
     *
     * @param xmlString xml format string to parse
     * @return list of EvioEvent objects constructed from arg
     * @throws EvioException if xml is not in proper format
     */
    static public List<EvioEvent> toEvents(String xmlString) throws EvioException {
        return toEvents(xmlString, 0, 0, null, false);
    }


    /**
     * This method takes a string representation of evio events
     * (each starting with lt; event gt;)
     * and converts them to a list of EvioEvent objects.
     *
     * @param xmlString   xml format string to parse
     * @param maxEvents   max number of events to return
     * @param skip        number of events to initially skip over
     * @param dictionary  dictionary object
     * @param debug       true for debug output
     * @return list of EvioEvent objects constructed from arg
     * @throws EvioException if xml is not in proper format
     */
    static public List<EvioEvent> toEvents(String xmlString, int maxEvents, int skip,
                                                 EvioXMLDictionary dictionary, boolean debug)
            throws EvioException {

        if (xmlString == null) {
            return null;
        }

        // For a single event, keep track of the state of each XML level in a stack.
        // This is faster than the old "Stack" java class.
        ArrayDeque<EvioXmlLevel> xmlStack = new ArrayDeque<EvioXmlLevel>(10);
        EvioXmlLevel level=null, upLevel;

        EventBuilder eb = new EventBuilder(null);
        StringReader sReader = new StringReader(xmlString);
        XMLInputFactory f = XMLInputFactory.newInstance();
        ArrayList<EvioEvent> eventList = new ArrayList<EvioEvent>();
        DataType dataType;
        int eventCount=0;
        Integer[] tagNum;
        boolean eventBeginning=true, fileBeginning=true;
        boolean skipEvent=false, inDictionary, haveDictionary=(dictionary != null);

        try {
            XMLEventReader evReader = f.createXMLEventReader(sReader);

            while(evReader.hasNext()) {

                XMLEvent event = evReader.nextEvent();

                switch (event.getEventType()) {

                    case XMLEvent.START_ELEMENT:
                        if (skipEvent) break;

                        StartElement se = event.asStartElement();
                        String name = se.getName().getLocalPart();
if (debug) System.out.println("START_ELEMENT " + name + ":");

                        // Keep track of our parent structure's data type
                        // so we know what kind of container we are if not
                        // bank, seg, or tagseg element.
                        DataType parentDataType = null;
                        if (level != null) {
                            parentDataType = level.dataTypeObj;
                            // Push previous level onto stack for later use
                            xmlStack.push(level);
                        }

                        level = new EvioXmlLevel();
                        level.parentDataTypeObj = parentDataType;
                        inDictionary = false;

                        // Retrieve any dictionary info.
                        // May be overwritten by explicitly set tag/num/type attributes.
                        if (haveDictionary) {
                            tagNum = dictionary.getTagNum(name);
                            if (tagNum != null) {
                                inDictionary = true;
                                level.tag = tagNum[0];
                                level.num = tagNum[1];
if (debug) System.out.println("FOUND dict entry(" + name + "): tag = " + level.tag +
                              ", num = " + level.num);
                            }
                            level.dataTypeObj = dictionary.getType(name);
                        }

                        // Read in relevant attributes
                        Iterator it = se.getAttributes();
                        while (it.hasNext()) {
                            Attribute attr = (Attribute)it.next();
                            String attrName = attr.getName().getLocalPart();
                            String valStr = attr.getValue();
                            boolean hex = false;

                            // Convert attribute value to integer.
                            // Watch out for hex numbers, they can't be parsed as is.
                            if (valStr.startsWith("0x") || valStr.startsWith("0X")) {
                                valStr = valStr.substring(2, valStr.length());
                                hex = true;
                            }

                            // Only bother parsing for relevant tags
                            try {
                                if (attrName.equalsIgnoreCase("tag")) {
                                    if (hex) { level.tag = Integer.parseInt(valStr, 16); }
                                    else     { level.tag = Integer.parseInt(valStr); }
                                }
                                else if (attrName.equalsIgnoreCase("num")) {
                                    if (hex) { level.num = Integer.parseInt(valStr, 16); }
                                    else     { level.num = Integer.parseInt(valStr); }
                                }
                                else if (attrName.equalsIgnoreCase("ndata")) {
                                    if (hex) { level.nData = Integer.parseInt(valStr, 16); }
                                    else     { level.nData = Integer.parseInt(valStr); }
                                }
                                else if (attrName.equalsIgnoreCase("data_type")) {
                                    int typ;
                                    if (hex) { typ = Integer.parseInt(valStr, 16); }
                                    else     { typ = Integer.parseInt(valStr); }
                                    level.dataTypeObj = DataType.getDataType(typ);
                                }
                            }
                            catch (NumberFormatException e) {
                                throw new EvioException("attribute has non-numeric value in line " +
                                                        event.getLocation().getLineNumber());
                            }
                        }

                        if (fileBeginning) {
                            // If we're at the beginning of the xml file, multiple events can
                            // only be contained if the top level element is "evio-data".
                            // If not, there must be only 1 event or xml protocol is violated
                            // since only 1 top-level element is permitted.
                            // If there is no dictionary, an event must be named "event",
                            // otherwise it could be any valid xml-element string and must
                            // exist in the dictionary.
                            if (!inDictionary &&
                                !name.equalsIgnoreCase("evio-data") &&
                                !name.equalsIgnoreCase("event")) {
                                    throw new EvioException("file must start with <evio-data> or <event>");
                            }
                        }

                        // This is the OPTIONAL top level xml surrounding multiple events.
                        // Not necessary for only 1 event.
                        if (name.equalsIgnoreCase("evio-data")) {
                            if (!fileBeginning) {
                                throw new EvioException("<evio-data> element must be at file beginning");
                            }
                            // Don't track this top level
                            level = null;
                            fileBeginning = false;
                        }
                        // Start building an EvioEvent here
                        else if (eventBeginning) {
                            // Element name must either be "event" or something in the dictionary
                            if (!inDictionary && !name.equalsIgnoreCase("event")) {
                                throw new EvioException("event must start with <event> element, not " + name);
                            }

                            // If we've hit our max, we're done
                            if ((maxEvents > 0) && (eventCount >= maxEvents + skip)) {
if (debug) System.out.println("Hit max # events, quitting");
                                return eventList;
                            }

                            // Skip over this event since first "skip" # of events are ignored
                            if (skip > eventCount++) {
if (debug) System.out.println("Skipping event #" + eventCount);
                                level = null;
                                skipEvent = true;
                                fileBeginning = false;
                                break;
                            }

                            // Read "format" attribute to see if data is in evio format,
                            // if it isn't skip this event
                            Attribute attr = se.getAttributeByName(new QName("format"));
                            if (attr != null) {
                                if (!attr.getValue().equalsIgnoreCase("evio")) {
                                    level = null;
                                    skipEvent = true;
                                    fileBeginning = false;
                                    break;
                                }
                            }

                            // Read "count" attribute and store
                            int count = 1;
                            attr = se.getAttributeByName(new QName("count"));
                            if (attr != null) {
                                try {
                                    count = Integer.parseInt(attr.getValue());
                                }
                                catch (NumberFormatException e) {
                                    throw new EvioException("attribute has non-numeric value in line " +
                                                            event.getLocation().getLineNumber());
                                }
                            }

                            // Very first element, so create our EventBuilder object
                            eb = new EventBuilder(level.tag, level.dataTypeObj, level.num);
                            eb.getEvent().setEventNumber(count);
                            level.bs = eb.getEvent();
                            eventBeginning = false;
                            fileBeginning = false;
                        }
                        else if (name.equalsIgnoreCase("bank")) {
                            if (level.dataTypeObj == null) {
                                throw new EvioException("must specify data type at line " +
                                                         event.getLocation().getLineNumber());
                            }
                            level.bs = new EvioBank(level.tag, level.dataTypeObj, level.num);
                        }
                        else if (name.equalsIgnoreCase("segment")) {
                            if (level.dataTypeObj == null) {
                                throw new EvioException("must specify data type at line " +
                                                         event.getLocation().getLineNumber());
                            }
                            level.bs = new EvioSegment(level.tag, level.dataTypeObj);
                        }
                        else if (name.equalsIgnoreCase("tagsegment")) {
                            if (level.dataTypeObj == null) {
                                throw new EvioException("must specify data type at line " +
                                                         event.getLocation().getLineNumber());
                            }
                            level.bs = new EvioTagSegment(level.tag, level.dataTypeObj);
                        }
                        else {
                            // If we're here, we don't immediately recognize the element name.
                            // It could be one of the pre-defined evio containers like:
                            //    int32, string, float64, etc.
                            // Or it could be defined in the dictionary, if it exists.

                            // Look to see if it's pre-defined
                            dataType = getDataType(name);

                            // If not pre-defined, see if it's in the dictionary
                            if (dataType == null) {
                                if (inDictionary) {
                                    // Found element in the dictionary (several lines up),
                                    // but did we find a data type there? If not, trouble.
                                    if (level.dataTypeObj == null) {
                                        throw new EvioException("dictionary entry for \"" + name +
                                                                "\" does not specify data type at line " +
                                                                event.getLocation().getLineNumber());
                                    }
                                }
                                else {
                                    throw new EvioException("unknown element, " + name +
                                                            ", in line " + event.getLocation().getLineNumber());
                                }
                            }
                            else {
                                if (level.dataTypeObj == null) {
                                    level.dataTypeObj = dataType;
                                }
                                else if (level.dataTypeObj != dataType) {
                                    throw new EvioException("mismatching data type: name = " + name +
                                                                    " and data_type = " + level.dataTypeObj +
                                                                    " at line " + event.getLocation().getLineNumber());
                                }
                            }

                            // We contain data by definition. But are we a bank, seg, or tagseg?
                            // This can only be known by looking at our parent data type.
                            if (level.parentDataTypeObj != null) {
                                switch (level.parentDataTypeObj) {
                                    case BANK:
                                    case ALSOBANK:
                                        level.bs = new EvioBank(level.tag, level.dataTypeObj, level.num);
                                        break;
                                    case SEGMENT:
                                    case ALSOSEGMENT:
                                        level.bs = new EvioSegment(level.tag, level.dataTypeObj);
                                        break;
                                    case TAGSEGMENT:
                                        level.bs = new EvioTagSegment(level.tag, level.dataTypeObj);
                                        break;
                                    default:
                                        throw new EvioException("parent container must be bank, seg or tagseg " +
                                        ", in line " + event.getLocation().getLineNumber());
                                }
                            }

                            if (name.equalsIgnoreCase("composite")) {
                                // Because the composite type contains not just numbers, as in
                                // other non-container data types, but also additional levels of
                                // xml, we need to parse this separately.
                                parseComposite(level, evReader);
                            }
                        }

                        break;


                    case XMLEvent.END_ELEMENT:
                        name = event.asEndElement().getName().getLocalPart();
if (debug) System.out.println("END_ELEMENT " + name);

                        if (skipEvent) {
                            if (name.equalsIgnoreCase("event")) {
                                // Done with the skipped event, move to next
                                skipEvent = false;
                                break;
                            }
                            break;
                        }

                        if (xmlStack.size() < 1) {
                            if (name.equalsIgnoreCase("evio-data")) {
                                // Done with everything
                                break;
                            }
                            // We're done with event, move on to next if any
                            eventList.add(eb.getEvent());
                            eventBeginning = true;
                            level = null;
                            break;
                        }

                        // Go up an xml level
                        upLevel = xmlStack.pop();

                        // Add lower level bank/seg/tagseg to upper level container
                        try {eb.addChild(upLevel.bs, level.bs);}
                        catch (EvioException e) {/* never happen */}

                        level = upLevel;
                        break;


                    case XMLEvent.CHARACTERS:
                        if (skipEvent) {
                            break;
                        }

                        Characters cs = event.asCharacters();
                        // Whitespace is all ignorable.
                        // CDATA element is not recognized by parser but
                        // only the chars inside a single CDATA construct are
                        // given in a single call to cs.getData().
                        if (!cs.isWhiteSpace()) {
                            parseData(level, cs.getData(), cs.getLocation());
                        }
                        break;


                    case XMLEvent.START_DOCUMENT:
if (debug) System.out.println("START_DOCUMENT");
                        break;


                    case XMLEvent.END_DOCUMENT:
if (debug) System.out.println("END_DOCUMENT");
                        break;


                    default:

                }
            }
        }
        catch (XMLStreamException e) {
            throw new EvioException(e);
        }

        return eventList;
    }


    /**
     * This method parses an evio container of composite format data.
     * @param level    xml level
     * @param evReader object used to parse xml
     * @throws EvioException
     * @throws XMLStreamException
     */
    static private void parseComposite(EvioXmlLevel level, XMLEventReader evReader)
            throws EvioException, XMLStreamException {

        String[] formats = new String[level.nData];
        CompositeData[] cdArray = new CompositeData[level.nData];
        CompositeData.Data[] cData = new CompositeData.Data[level.nData];
        int cDataCount=-1;
        int tag, num, repeats=1;
        ArrayList<String> strings = new ArrayList<String>();
        DataType dataType=null;
        String valStr, name;
        boolean isComp, debug = false, lookForFormat = false;

        while (evReader.hasNext()) {

            XMLEvent event = evReader.nextEvent();
            isComp = false;

            switch (event.getEventType()) {

                case XMLEvent.START_ELEMENT:

                    StartElement se = event.asStartElement();
                    name = se.getName().getLocalPart();
if (debug) System.out.println("    comp START_ELEMENT " + name + ":");

                    // Start of entire, single CompositeData(CD) item
                    if (name.equalsIgnoreCase("comp")) {
                        cData[++cDataCount] = new CompositeData.Data();
                        isComp = true;
                    }

                    // Number of repeats for a single format specifier
                    repeats = 1;
                    Attribute attr;
                    try {
                        attr = se.getAttributeByName(new QName("count"));
                        if (attr != null) {
                            repeats = Integer.parseInt(attr.getValue());
                        }

                        // This attribute will never appear together with "count"
                        attr = se.getAttributeByName(new QName("N"));
                        if (attr != null) {
                            repeats = Integer.parseInt(attr.getValue());
                            cData[cDataCount].addN(repeats);
                        }

                        attr = se.getAttributeByName(new QName("n"));
                        if (attr != null) {
                            repeats = Integer.parseInt(attr.getValue());
                            cData[cDataCount].addn((short)repeats);
                        }

                        attr = se.getAttributeByName(new QName("m"));
                        if (attr != null) {
                            repeats = Integer.parseInt(attr.getValue());
                            cData[cDataCount].addm((byte)repeats);
                        }
                    }
                    catch (NumberFormatException e) {
                        throw new EvioException("attribute has non-numeric value in line " +
                                                se.getLocation().getLineNumber());
                    }

                    // Full format string of CD item
                    if (name.equalsIgnoreCase("format")) {
                        // Though not used, get tag for completeness from segment
                        attr = se.getAttributeByName(new QName("tag"));
                        if (attr != null) {
                            try {
                                // Strip off any hex 0x's
                                valStr = attr.getValue();
                                if (valStr.startsWith("0x") || valStr.startsWith("0X")) {
                                    valStr = valStr.substring(2, valStr.length());
                                    tag = Integer.parseInt(valStr, 16);
                                }
                                else {
                                    tag = Integer.parseInt(valStr);
                                }
                                cData[cDataCount].setFormatTag(tag);
                            }
                            catch (NumberFormatException e) {
                                throw new EvioException("attribute has non-numeric value in line " +
                                                        se.getLocation().getLineNumber());
                            }
                        }

                        // Look for format string when next reading characters
                        lookForFormat = true;
                    }
                    // Data section of CD item
                    else if (name.equalsIgnoreCase("data")) {
                        // Though not used, get tag & num for completeness from bank
                        try {
                            attr = se.getAttributeByName(new QName("tag"));
                            if (attr != null) {
                                // Strip off any hex 0x's
                                valStr = attr.getValue();
                                if (valStr.startsWith("0x") || valStr.startsWith("0X")) {
                                    valStr = valStr.substring(2, valStr.length());
                                    tag = Integer.parseInt(valStr, 16);
                                }
                                else {
                                    tag = Integer.parseInt(valStr);
                                }
                                cData[cDataCount].setDataTag(tag);

                            }

                            attr = se.getAttributeByName(new QName("num"));
                            if (attr != null) {
                                // Strip off any hex 0x's
                                valStr = attr.getValue();
                                if (valStr.startsWith("0x") || valStr.startsWith("0X")) {
                                    valStr = valStr.substring(2, valStr.length());
                                    num = Integer.parseInt(valStr, 16);
                                }
                                else {
                                    num = Integer.parseInt(valStr);
                                }
                                cData[cDataCount].setDataNum(num);

                            }
                        }
                        catch (NumberFormatException e) {
                            throw new EvioException("attribute has non-numeric value in line " +
                                                    se.getLocation().getLineNumber());
                        }
                    }
                    else if (name.equalsIgnoreCase("row")) {
                        // This element is only used for ease of human reading, not parsing
                    }
                    else if (name.equalsIgnoreCase("repeat")) {
                        // Used to specify repeat (n or count) parameter, see above
                    }
                    else if (name.equalsIgnoreCase("paren")) {
                        // This element is only used for ease of human reading, not parsing
                    }
                    // Numeric or string data
                    else {
                        // Filter out bad elements here
                        if (!isComp && (getDataType(name) == null)) {
                            throw new EvioException("unknown element, " + name +
                                                    ", in line " + se.getLocation().getLineNumber());
                        }

                        // Type of data to read next
                        dataType = getDataType(name);

                        // If strings, collect one-at-a-time
                        if (dataType == DataType.CHARSTAR8) {
                            strings.clear();
                        }
                    }

                    break;


                case XMLEvent.END_ELEMENT:
                    name = event.asEndElement().getName().getLocalPart();
if (debug) System.out.println("    comp END_ELEMENT :" + name);

                    // All strings are now collected, add them to cData structure
                    if (name.equalsIgnoreCase("string")) {
                        String[] strs = new String[strings.size()];
                        strings.toArray(strs);
                        cData[cDataCount].addString(strs);
                    }
                    // All cData items are now collected, add them to evio container
                    else if (name.equalsIgnoreCase("comp") && (cDataCount+1) >= level.nData) {
//                        System.out.println("Got last CompositeData item");
                        for (int i=0; i < level.nData; i++) {
                            cdArray[i] = new CompositeData(formats[i], cData[i],
                                                           level.bs.byteOrder);
                        }
                        level.bs.appendCompositeData(cdArray);
                        return;
                    }

                    break;


                case XMLEvent.CHARACTERS:
                    Characters cs = event.asCharacters();
                    // Whitespace is all ignorable.
                    if (cs.isWhiteSpace()) {
                        break;
                    }

                    String s = cs.getData();

                    if (debug) {
                        System.out.println("    comp CHARACTERS:");
                        System.out.println("    " + s.trim());
                    }

                    // Time to read in the format string
                    if (lookForFormat) {
                        formats[cDataCount] = s.trim();
if (debug) System.out.println("      Format = " + formats[cDataCount]);
                        lookForFormat = false;
                    }
                    else {
                        if (dataType == null) {
                            throw new EvioException("Internal error: data type null, line " +
                                                    cs.getLocation().getLineNumber());
                        }

                        // Strings are collected and added as an array when complete
                        if (dataType == DataType.CHARSTAR8) {
                            strings.add(s);
                        }
                        // Numeric data
                        else {
                            parseCompositeData(dataType, repeats, s,
                                               cData[cDataCount], cs.getLocation());
                        }
                    }

                    break;

                default:

            }
        }
    }


    /**
     * This method parses actual data, not containers.
     * @param level      xml level
     * @param data       data in String form
     * @param location   place in xml file being parsed
     * @throws EvioException
     */
    static private void parseData(EvioXmlLevel level, String data, Location location)
            throws EvioException {

// System.out.println("CHARACTERS:");
// System.out.println("    " + data);

        // Split string on whitespace only if numerical type
        String[] values = null;
        if (level.dataTypeObj != DataType.CHARSTAR8) {
            // Remove whitespace fore and aft first, then
            // split data into pieces separated by all whitespace
            values = data.trim().split("\\s+");
            if (values.length != level.nData) {
                throw new EvioException("# data items: attribute = " + level.nData +
                            ", actual = " + values.length +
                            " in line " + location.getLineNumber() + ", col " + location.getColumnNumber());
            }
        }

        String val;

        try {
            switch (level.dataTypeObj) {
                case UCHAR8:
                    byte[] bArray = new byte[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            bArray[i] = (byte)Short.parseShort(val, 16);
                        }
                        else {
                            bArray[i] = (byte)Short.parseShort(val);
                        }
                    }

                    try {level.bs.appendByteData(bArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case CHAR8:
                    bArray = new byte[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            bArray[i] = Byte.parseByte(val, 16);
                        }
                        else {
                            bArray[i] = Byte.parseByte(val);
                        }
                    }

                    try {level.bs.appendByteData(bArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case USHORT16:
                    short[] sArray = new short[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            sArray[i] = (short)Integer.parseInt(val, 16);
                        }
                        else {
                            sArray[i] = (short)Integer.parseInt(val);
                        }
                    }

                    try {level.bs.appendShortData(sArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case SHORT16:
                    sArray = new short[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            sArray[i] = Short.parseShort(val, 16);
                        }
                        else {
                            sArray[i] = Short.parseShort(val);
                        }
                    }

                    try {level.bs.appendShortData(sArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case UINT32:
                    int[] iArray = new int[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            iArray[i] = (int)Long.parseLong(val, 16);
                        }
                        else {
                            iArray[i] = (int)Long.parseLong(val);
                        }
                    }

                    try {level.bs.appendIntData(iArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case INT32:
                    iArray = new int[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            iArray[i] = Integer.parseInt(val, 16);
                        }
                        else {
                            iArray[i] = Integer.parseInt(val);
                        }
                    }

                    try {level.bs.appendIntData(iArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case ULONG64:
                    long[] lArray = new long[values.length];
                    BigInteger bg;
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            bg = new BigInteger(val, 16);
                        }
                        else {
                            bg = new BigInteger(val);
                        }
                        lArray[i] = bg.longValue();
                        // For java version 8+, no BigIntegers necessary:
                        //lArray[i] = Long.parseUnsignedLong(val, 16);
                    }

                    try {level.bs.appendLongData(lArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case LONG64:
                    lArray = new long[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            lArray[i] = Long.parseLong(val, 16);
                        }
                        else {
                            lArray[i] = Long.parseLong(val);
                        }
                    }

                    try {level.bs.appendLongData(lArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case FLOAT32:
                    float[] fArray = new float[values.length];
                    for (int i=0; i < values.length; i++) {
                        fArray[i] = Float.parseFloat(values[i]);
                    }

                    try {level.bs.appendFloatData(fArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                case DOUBLE64:
                    double[] dArray = new double[values.length];
                    for (int i=0; i < values.length; i++) {
                        dArray[i] = Double.parseDouble(values[i]);
                    }

                    try {level.bs.appendDoubleData(dArray);}
                    catch (EvioException e) {/* never happen */}

                    break;

                // Strings are parsed one-by-one since each string
                // is surrounded by the CDATA construct.
                case CHARSTAR8:
                    try {level.bs.appendStringData(data);}
                    catch (EvioException e) {/* never happen */}

                    break;

                default:
            }
        }
        catch (NumberFormatException e) {
            throw new EvioException(e.getMessage() +
                        " in line " + location.getLineNumber() +
                        ", col " + location.getColumnNumber());
        }

    }


    static private void parseCompositeData(DataType dataType, int count, String data,
                                   CompositeData.Data cData, Location location)
            throws EvioException {

        // Split string on whitespace only if numerical type
        String[] values;
        if (dataType != DataType.CHARSTAR8) {
            // Split data into pieces separated by all whitespace
            values = data.trim().split("\\s+");

            if (count != values.length) {
                throw new EvioException("# data items: attribute = " + count +
                            ", actual = " + values.length +
                            " in line " + location.getLineNumber() + ", col " + location.getColumnNumber());
            }
        }
        else {
            return;
        }

        String val;

        try {
            switch (dataType) {
                case UCHAR8:
                    byte[] bArray = new byte[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            bArray[i] = (byte)Short.parseShort(val, 16);
                        }
                        else {
                            bArray[i] = (byte)Short.parseShort(val);
                        }
                    }
                    cData.addUchar(bArray);

                    break;

                case CHAR8:
                    bArray = new byte[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            bArray[i] = Byte.parseByte(val, 16);
                        }
                        else {
                            bArray[i] = Byte.parseByte(val);
                        }
                    }
                    cData.addChar(bArray);

                    break;

                case USHORT16:
                    short[] sArray = new short[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            sArray[i] = (short)Integer.parseInt(val, 16);
                        }
                        else {
                            sArray[i] = (short)Integer.parseInt(val);
                        }
                    }
                    cData.addUshort(sArray);

                    break;

                case SHORT16:
                    sArray = new short[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            sArray[i] = Short.parseShort(val, 16);
                        }
                        else {
                            sArray[i] = Short.parseShort(val);
                        }
                    }
                    cData.addShort(sArray);

                    break;

                case UINT32:
                    int[] iArray = new int[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            iArray[i] = (int)Long.parseLong(val, 16);
                        }
                        else {
                            iArray[i] = (int)Long.parseLong(val);
                        }
                    }
                    cData.addUint(iArray);

                    break;

                case INT32:
                    iArray = new int[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            iArray[i] = Integer.parseInt(val, 16);
                        }
                        else {
                            iArray[i] = Integer.parseInt(val);
                        }
                    }
                    cData.addInt(iArray);

                    break;

                case ULONG64:
                    long[] lArray = new long[values.length];
                    BigInteger bg;
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            bg = new BigInteger(val, 16);
                        }
                        else {
                            bg = new BigInteger(val);
                        }
                        lArray[i] = bg.longValue();
                        // For java version 8+, no BigIntegers necessary:
                        //lArray[i] = Long.parseUnsignedLong(val, 16);
                    }
                    cData.addUlong(lArray);

                    break;

                case LONG64:
                    lArray = new long[values.length];
                    for (int i=0; i < values.length; i++) {
                        val = values[i];
                        if (val.startsWith("0x") || val.startsWith("0X")) {
                            val = val.substring(2, val.length());
                            lArray[i] = Long.parseLong(val, 16);
                        }
                        else {
                            lArray[i] = Long.parseLong(val);
                        }
                    }
                    cData.addLong(lArray);

                    break;

                case FLOAT32:
                    float[] fArray = new float[values.length];
                    for (int i=0; i < values.length; i++) {
                        fArray[i] = Float.parseFloat(values[i]);
                    }
                    cData.addFloat(fArray);

                    break;

                case DOUBLE64:
                    double[] dArray = new double[values.length];
                    for (int i=0; i < values.length; i++) {
                        dArray[i] = Double.parseDouble(values[i]);
                    }
                    cData.addDouble(dArray);

                    break;

                default:
            }
        }
        catch (NumberFormatException e) {
            throw new EvioException(e.getMessage() +
                        " in line " + location.getLineNumber() +
                        ", col " + location.getColumnNumber());
        }
    }


    /**
     * This method returns an XML element name given an evio data type.
     * @param type evio data type
     * @return XML element name used in evio event xml output
     */
    static public DataType getDataType(String type) {

        if (type == null) return null;

             if (type.equalsIgnoreCase("int8"))       return DataType.CHAR8;
        else if (type.equalsIgnoreCase("uint8"))      return DataType.UCHAR8;
        else if (type.equalsIgnoreCase("int16"))      return DataType.SHORT16;
        else if (type.equalsIgnoreCase("uint16"))     return DataType.USHORT16;
        else if (type.equalsIgnoreCase("int32"))      return DataType.INT32;
        else if (type.equalsIgnoreCase("uint32"))     return DataType.UINT32;
        else if (type.equalsIgnoreCase("int64"))      return DataType.LONG64;
        else if (type.equalsIgnoreCase("uint64"))     return DataType.ULONG64;
        else if (type.equalsIgnoreCase("long64"))     return DataType.LONG64;
        else if (type.equalsIgnoreCase("ulong64"))    return DataType.ULONG64;
        else if (type.equalsIgnoreCase("float32"))    return DataType.FLOAT32;
        else if (type.equalsIgnoreCase("float64"))    return DataType.DOUBLE64;
        else if (type.equalsIgnoreCase("double64"))   return DataType.DOUBLE64;
        else if (type.equalsIgnoreCase("string"))     return DataType.CHARSTAR8;
        else if (type.equalsIgnoreCase("composite"))  return DataType.COMPOSITE;
        else if (type.equalsIgnoreCase("unknown32"))  return DataType.UNKNOWN32;
        else if (type.equalsIgnoreCase("tagsegment")) return DataType.TAGSEGMENT;
        else if (type.equalsIgnoreCase("segment"))    return DataType.ALSOSEGMENT;
        else if (type.equalsIgnoreCase("bank"))       return DataType.ALSOBANK;

        return null;
    }


    /*---------------------------------------------------------------------------
     *  XML output for EvioNode.
     *  The reason this is not part of the EvioNode class is that EvioNode
     *  objects are kept as small and as simple as possible since they are
     *  used a great deal in compute-intensive situations.
     *--------------------------------------------------------------------------*/

    /**
     * Increase the indentation of the given XML indentation.
     * @param xmlIndent String of spaces to increase.
     * @return increased indentation String
     */
    static String increaseXmlIndent(String xmlIndent) {
        if (xmlIndent == null) return "   ";
        xmlIndent += "   ";
        return xmlIndent;
    }


    /**
     * Decrease the indentation of the given XML indentation.
     * @param xmlIndent String of spaces to increase.
     * @return Decreased indentation String
     */
    static String decreaseXmlIndent(String xmlIndent) {
        if (xmlIndent == null || xmlIndent.length() < 3) return "";
        xmlIndent = xmlIndent.substring(0, xmlIndent.length() - 3);
        return xmlIndent;
    }


    /**
     * This method takes an EvioNode object and converts it to a readable, XML String.
     * @param node EvioNode object to print out
     * @param hex  if true, ints get displayed in hexadecimal
     * @return XML string.
     * @throws EvioException if node is not a bank or cannot parse node's buffer
     */
    static public String toXML(EvioNode node, boolean hex) throws EvioException {

        if (node == null) {
            return null;
        }

        StringWriter sWriter = null;
        XMLStreamWriter xmlWriter = null;
        try {
            sWriter = new StringWriter();
            xmlWriter = XMLOutputFactory.newInstance().createXMLStreamWriter(sWriter);
            String xmlIndent = "";
            nodeToString(node, xmlIndent, hex, xmlWriter);
            xmlWriter.flush();
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }

        return sWriter.toString();
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
     * @param node      EvioNode object to print out.
     * @param xmlIndent String of spaces.
     * @param hex       if true, ints get displayed in hexadecimal
     * @param xmlWriter the writer used to write the node.
     */
    static private void nodeToString(EvioNode node, String xmlIndent,
                                           boolean hex, XMLStreamWriter xmlWriter) {

        DataType nodeType = node.getTypeObj();
        DataType dataType = node.getDataTypeObj();

        try {
            // If top level ...
            if (node.isEvent) {
                int totalLen = node.getLength() + 1;
                xmlIndent = increaseXmlIndent(xmlIndent);
                xmlWriter.writeCharacters(xmlIndent);
                xmlWriter.writeComment(" Buffer " + node.getEventNumber() + " contains " +
                                       totalLen + " words (" + 4*totalLen + " bytes)");
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
                    nodeToString(n, xmlIndent, hex, xmlWriter);
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

                List<EvioNode> childNodes = node.getChildNodes();
                if (childNodes != null) {
                    xmlIndent = increaseXmlIndent(xmlIndent);

                    for (EvioNode n : childNodes) {
                        // Recursive call
                        nodeToString(n, xmlIndent, hex, xmlWriter);
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
                writeXmlData(node, count, xmlIndent, hex, xmlWriter);
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
                                    node.buffer, node.dataPos, 4*node.dataLen);

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
  	 * Write data as XML output for EvioNode.<p>
     *
     * Warning: this is essentially duplicated in
     * {@link BaseStructure#commonXMLDataWrite(XMLStreamWriter, boolean)},
     * so any change here needs to be made there too.
     *
  	 * @param node      the EvioNode object whose data is to be converted to String form.
     * @param count     number of data items (shorts, longs, doubles, etc.) in this node
     *                  (not children)
     * @param xmlIndent string of spaces to be used as xml indentation
     * @param hex       if true, ints get displayed in hexadecimal
     * @param xmlWriter the writer used to write the node.
  	 */
    static private void writeXmlData(EvioNode node, int count, String xmlIndent,
                                           boolean hex, XMLStreamWriter xmlWriter) {

  		// Only leaves write data
        DataType dataTypeObj = node.getDataTypeObj();
  		if (dataTypeObj.isStructure()) {
  			return;
  		}

        try {
            String s;
            String indent = String.format("\n%s", xmlIndent);

            ByteBuffer buf = node.getByteData(true);

  			switch (dataTypeObj) {
  			case DOUBLE64:
  				DoubleBuffer dbuf = buf.asDoubleBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%25.17g  ", dbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case FLOAT32:
                FloatBuffer fbuf = buf.asFloatBuffer();
                for (int i=0; i < count; i++) {
                    if (i%4 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%15.8g  ", fbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case LONG64:
                LongBuffer lbuf = buf.asLongBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%016x  ", lbuf.get(i));
                    }
                    else {
                        s = String.format("%20d  ", lbuf.get(i));
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case ULONG64:
                lbuf = buf.asLongBuffer();
                BigInteger bg;
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%016x  ", lbuf.get(i));
                    }
                    else {
                        bg = new BigInteger(1, ByteDataTransformer.toBytes(lbuf.get(i), ByteOrder.BIG_ENDIAN));
                        s = String.format("%20s  ", bg.toString());
                        // For java version 8+, no BigIntegers necessary:
                        //s = String.format("%20s  ", Long.toUnsignedString(lbuf.get(i)));
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case INT32:
                IntBuffer ibuf = buf.asIntBuffer();
                for (int i=0; i < count; i++) {
                    if (i%4 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%08x  ", ibuf.get(i));
                    }
                    else {
                        s = String.format("%11d  ", ibuf.get(i));
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case UINT32:
                ibuf = buf.asIntBuffer();
                for (int i=0; i < count; i++) {
                    if (i%4 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%08x  ", ibuf.get(i));
                    }
                    else {
                        s = String.format("%11d  ", ((long) ibuf.get(i)) & 0xffffffffL);
                        //s = String.format("%11s  ", Integer.toUnsignedString(ibuf.get(i)));
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case SHORT16:
                ShortBuffer sbuf = buf.asShortBuffer();
                for (int i=0; i < count; i++) {
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%04x  ", sbuf.get(i));
                    }
                    else {
                        s = String.format("%6d  ", sbuf.get(i));
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case USHORT16:
                sbuf = buf.asShortBuffer();
                for (int i=0; i < count; i++) {
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%04x  ", sbuf.get(i));
                    }
                    else {
                        s = String.format("%6d  ", ((int) sbuf.get(i)) & 0xffff);
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case CHAR8:
                for (int i=0; i < count; i++) {
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%02x  ", buf.get(i));
                    }
                    else {
                        s = String.format("%4d  ", buf.get(i));
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

            case UNKNOWN32:
  			case UCHAR8:
                for (int i=0; i < count; i++) {
                    if (i%8 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }

                    if (hex) {
                        s = String.format("0x%02x  ", buf.get(i));
                    }
                    else {
                        s = String.format("%4d  ", ((short) buf.get(i)) & 0xff);
                    }
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case CHARSTAR8:
                String[] stringdata = BaseStructure.unpackRawBytesToStrings(
                        node.buffer, node.dataPos, 4 * node.dataLen);

                if (stringdata == null) {
                    break;
                }

                for (String str : stringdata) {
                    xmlWriter.writeCharacters(indent);
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
                          cd.toXML(xmlWriter, xmlIndent, hex);
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
