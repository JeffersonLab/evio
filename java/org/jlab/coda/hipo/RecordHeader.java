/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Generic HEADER class to compose a record header or file header
 * with given buffer sizes and paddings and compression types.<p>
 *
 * <pre>
 * RECORD HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
 *    +----------------------------------+
 *  1 |         Record Length            | // 32bit words, inclusive
 *    +----------------------------------+
 *  2 +         Record Number            |
 *    +----------------------------------+
 *  3 +         Header Length            | // 14 (words)
 *    +----------------------------------+
 *  4 +       Event (Index) Count        |
 *    +----------------------------------+
 *  5 +      Index Array Length          | // bytes
 *    +-----------------------+---------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     | // bytes
 *    +------+---------------------------+
 * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits)
 *    +----------------------------------+
 * 11 +        General Register 1        | // UID 1st (64 bits)
 *    +--                              --+
 * 12 +                                  |
 *    +----------------------------------+
 * 13 +        General Register 2        | // UID 2nd (64 bits)
 *    +--                              --+
 * 14 +                                  |
 *    +----------------------------------+
 *
 * -------------------
 *   Compression Type
 * -------------------
 *     0  = none
 *     1  = LZ4 fastest
 *     2  = LZ4 best
 *     3  = gzip
 *
 * -------------------
 *   Bit Info Word
 * -------------------
 *     0-7  = version
 *     8    = true if dictionary is included (relevant for first record only)
 *     9    = true if this record is the last in file or stream
 *    10-13 = type of events contained: 0 = ROC Raw,
 *                                      1 = Physics
 *                                      2 = PartialPhysics
 *                                      3 = DisentangledPhysics
 *                                      4 = User
 *                                      5 = Control
 *                                     15 = Other
 *    14-19 = reserved
 *    20-21 = pad 1
 *    22-23 = pad 2
 *    24-25 = pad 3
 *    26-27 = reserved
 *    28-31 = general header type: 0 = Evio record,
 *                                 3 = Evio file trailer
 *                                 4 = HIPO record,
 *                                 7 = HIPO file trailer
 *
 *
 *
 * ------------------------------------------------------------
 * ------------------------------------------------------------
 *
 * FILE HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
 *    +----------------------------------+
 *  1 |              ID                  | // HIPO: 0x43455248, Evio: 0x4556494F
 *    +----------------------------------+
 *  2 +          File Number             | // split file #
 *    +----------------------------------+
 *  3 +         Header Length            | // 14 (words)
 *    +----------------------------------+
 *  4 +      Record (Index) Count        |
 *    +----------------------------------+
 *  5 +      Index Array Length          | // bytes
 *    +-----------------------+---------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     | // 0 (bytes)
 *    +------+---------------------------+
 * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits)
 *    +----------------------------------+
 * 11 +         Trailer Position         | // File offset to trailer head (64 bits) .
 *    +--                              --+ // 0 = no offset available or no trailer exists.
 * 12 +                                  |
 *    +----------------------------------+
 * 13 +          User Register 1         |
 *    +----------------------------------+
 * 14 +          User Register 2         |
 *    +----------------------------------+
 *
 * -------------------
 *   Bit Info Word
 * -------------------
 *     0-7  = version
 *     8    = true if dictionary is included (relevant for first record only)
 *     9    = true if this file has "first" event (in every split file)
 *    10    = File trailer with index array exists
 *    11-19 = reserved
 *    20-21 = pad 1
 *    22-23 = pad 2
 *    24-25 = pad 3 (always 0)
 *    26-27 = reserved
 *    28-31 = general header type: 1 = Evio file
 *                                 2 = Evio extended file
 *                                 5 = HIPO file
 *                                 6 = HIPO extended file
 *
 * </pre>
 *
 * @author gavalian
 */
public class RecordHeader {

        private long            position = 0L;
        private int          recordLength = 0;
        private int          recordNumber = 0;
        private int               entries = 0;
        private int          headerLength = 0;
        private int      userHeaderLength = 0;
        private int           indexLength = 0;
        private int            dataLength = 0;
        private int  compressedDataLength = 0;
        private int       compressionType = 0;
            /*
             * These quantities are updated automatically
             * when the lengths are set.
             */
        private int             indexLengthPadding = 0;
        private int        userHeaderLengthPadding = 0;
        private int              dataLengthPadding = 0;
        private int    compressedDataLengthPadding = 0;
        private int            recordLengthPadding = 0;

        private int           dataLengthWords = 0;
        private int compressedDataLengthWords = 0;
        private int     userHeaderLengthWords = 0;
        private int         recordLengthWords = 0;
        private int         headerLengthWords = 0;
            /*
             * User registers that will be written at the end of the
             * header.
             */
        private long  recordUserRegisterFirst = 0L;
        private long recordUserRegisterSecond = 0L;

        private int         headerVersion = 6;
        private int         headerMagicWord;

        private final int   HEADER_MAGIC_LE = 0xc0da0100;
        private final int   HEADER_MAGIC_BE = 0x0010dac0;

        //private ByteOrder   headerByteOrder = ByteOrder.LITTLE_ENDIAN;
        //private final int headerMagicWord = 0xc0da0100;

        public RecordHeader() {
            headerMagicWord = HEADER_MAGIC_LE;
        }
        
        public RecordHeader(long _pos, int _l, int _e){
            position = _pos; recordLength = _l; entries = _e;
        }
        /**
         * Returns padded length of the array for given length in bytes.
         * @param length
         * @return 
         */
        private int getWords(int length){
            int padding = getPadding(length);
            int   words = length/4;
            if(padding>0) words++;
            return words;
        }
        /**
         * returns padding value for the given length
         * @param length
         * @return 
         */
        private int getPadding(int length){
            return length%4;
        }
        
        public long           getPosition() { return position;}
        public int              getLength() { return recordLength;}
        public int             getEntries() { return entries;}
        public int     getCompressionType() { return compressionType;}
        public int    getUserHeaderLength() { return userHeaderLength;}
        public int    getUserHeaderLengthWords() { return userHeaderLengthWords;}
        public int             getVersion() { return headerVersion;}
        public int    getDataLength() { return dataLength;}
        public int    getDataLengthWords() { return dataLengthWords;}
        public int    getIndexLength() { return indexLength;}
        public int    getCompressedDataLength() { return compressedDataLength;}
        public int    getCompressedDataLengthWords() { return compressedDataLengthWords;}
        public int    getHeaderLength() {return headerLength;}
        public int    getRecordNumber() { return recordNumber;}
        
        
        
        public long   getUserRegisterFirst(){ return this.recordUserRegisterFirst;}
        public long   getUserRegisterSecond(){ return this.recordUserRegisterSecond;}
        
        
        public RecordHeader  setPosition(long pos) { position = pos; return this;}
        public RecordHeader  setRecordNumber(int num) { recordNumber = num; return this;}
        public RecordHeader  setVersion(int version) { headerVersion = version; return this;}
        
        public RecordHeader  setLength(int length) { 
            recordLength        = length;
            recordLengthWords   = getWords(length);
            recordLengthPadding = getPadding(length);
            
            //System.out.println(" LENGTH = " + recordLength + "  WORDS = " + this.recordLengthWords
            //+ "  PADDING = " + this.recordLengthPadding + " SIZE = " + recordLengthWords*4 );
            return this;
        }
        
        public RecordHeader  setDataLength(int length) { 
            dataLength = length; 
            dataLengthWords = getWords(length);
            dataLengthPadding = getPadding(length);
            return this;
            
        }
        
        public RecordHeader  setCompressedDataLength(int length) { 
            compressedDataLength = length;
            compressedDataLengthWords = getWords(length);
            compressedDataLengthPadding = getPadding(length);
            return this;
        }
        
        public RecordHeader  setIndexLength(int length) { 
            indexLength = length; 
            //indexLengthWords = getWords(length);            
            return this;
        }
        
        public RecordHeader  setCompressionType(int type) { compressionType = type; return this;}
        public RecordHeader  setEntries(int n) { entries = n; return this;}
        
        public RecordHeader  setUserHeaderLength(int length) { 
            userHeaderLength = length; 
            userHeaderLengthWords   = getWords(length);
            userHeaderLengthPadding = getPadding(length);
            return this;
        }
        
        public RecordHeader  setHeaderLength(int length) { 
            headerLength = length; 
            headerLengthWords = length/4;
            return this;
        }
        
        public RecordHeader setUserRegisterFirst(long reg){
            recordUserRegisterFirst = reg; return this;
        }
        
        public RecordHeader setUserRegisterSecond(long reg){
            recordUserRegisterSecond = reg; return this;
        }
        
        /**
         * Writes the header information to the byte buffer provided
         * The offset provides the offset in the byte buffer
         * @param buffer byte buffer to write header to
         * @param offset position of first word to be written
         */
        public void writeHeader(ByteBuffer buffer, int offset){
            
            buffer.putInt( 0*4 + offset, recordLengthWords);
            buffer.putInt( 1*4 + offset, recordNumber);
            buffer.putInt( 2*4 + offset, headerLength);
            buffer.putInt( 3*4 + offset, entries);
            buffer.putInt( 4*4 + offset, indexLength);
            
            int versionWord = (headerVersion&0x000000FF);
            
            buffer.putInt( 5*4 + offset, versionWord);
            buffer.putInt( 6*4 + offset, userHeaderLength);
            buffer.putInt( 7*4 + offset, headerMagicWord); // word number 8
            
            int compressedWord = ((this.compressedDataLength)&(0x0FFFFFFF))
                    |( (compressionType&0x000000FF) << 28);
            buffer.putInt( 8*4 + offset, dataLength);
            buffer.putInt( 9*4 + offset, compressedWord);

            buffer.putLong( 10*4 + offset, recordUserRegisterFirst);
            buffer.putLong( 12*4 + offset, recordUserRegisterSecond);
        }
        
        public void writeHeader(ByteBuffer buffer){
            writeHeader(buffer,0);
        }
        
        /**
         * Reads the header information from a byte buffer. and validates
         * it by checking the magic word (which is in position #8, starting 
         * with #1). This magic word determines also the byte order.
         * LITTLE_ENDIAN or BIG_ENDIAN.
         * @param buffer
         * @param offset 
         */
        public void readHeader(ByteBuffer buffer, int offset){
            
            recordLengthWords   = buffer.getInt(    0 + offset );
            recordLength        = recordLengthWords * 4;
            recordLengthPadding = 0;
            
            recordNumber = buffer.getInt(  1*4 + offset );
            headerLength = buffer.getInt(  2*4 + offset);
            entries      = buffer.getInt(  3*4 + offset);
            
            indexLength  = buffer.getInt( 4*4 + offset);
            setIndexLength(indexLength);
            
            int  versionWord = buffer.getInt(  5*4 + offset);
            
            headerVersion    = (versionWord&0x000000FF);
            
            userHeaderLength = buffer.getInt( 6*4 + offset);
            setUserHeaderLength(userHeaderLength);
            
            headerMagicWord  = buffer.getInt( 7*4 + offset);
            dataLength       = buffer.getInt( 8*4 + offset);
            setDataLength(dataLength);
            
            int compressionWord = buffer.getInt( 9*4 + offset);
            
            compressionType = (compressionWord&0xF0000000)>>28;
            compressedDataLength = (compressionWord&0x0FFFFFFF);
            setCompressedDataLength(compressedDataLength);
            
            //indexLength = entries*4;
            
            recordUserRegisterFirst = buffer.getLong(  10*4 + offset);
            recordUserRegisterSecond = buffer.getLong( 12*4 + offset);
        }
        
        public void readHeader(ByteBuffer buffer){
            readHeader(buffer,0);
        }
        
        private int getBitInfoWord(){
            int word = 0x00000000;
            word = word | (userHeaderLengthPadding<<16);
            word = word | (dataLengthPadding<<18);
            word = word | (compressedDataLengthPadding<<20);
            word = word | (headerVersion&0x000000FF);
            return word;
        }
        /**
         * returns string representation of the record data.
         * @return 
         */
        @Override
        public String toString(){
            
            
            StringBuilder str = new StringBuilder();            
            str.append(String.format("%24s : %d\n","version",this.headerVersion));
            str.append(String.format("%24s : %d\n","record #",this.recordNumber));
            str.append(String.format("%24s : %8d / %8d / %8d\n","user header length",
                    this.userHeaderLength,this.userHeaderLengthWords, this.userHeaderLengthPadding));
            str.append(String.format("%24s : %8d / %8d / %8d\n","   data length",
                    this.dataLength, this.dataLengthWords,this.dataLengthPadding));
            str.append(String.format("%24s : %8d / %8d / %8d\n","record length",
                    this.recordLength, this.recordLengthWords,this.recordLengthPadding));
            str.append(String.format("%24s : %8d / %8d / %8d\n","compressed length",
                    this.compressedDataLength, this.compressedDataLengthWords,
                    this.compressedDataLengthPadding));
            str.append(String.format("%24s : %d\n",
                    "header length",this.headerLength));
            str.append(String.format("%24s : %X\n","magic word",this.headerMagicWord));
            Integer bitInfo = this.getBitInfoWord();
            str.append(String.format("%24s : %s\n","bit info word",Integer.toBinaryString(bitInfo)));
            str.append(String.format("%24s : %d\n","record entries",this.entries));
            str.append(String.format("%24s : %d\n","   compression type",this.compressionType));
            
            str.append(String.format("%24s : %d\n","  index length",this.indexLength));
            
            str.append(String.format("%24s : %d\n","user register #1",this.recordUserRegisterFirst));
            str.append(String.format("%24s : %d\n","user register #2",this.recordUserRegisterSecond));

            return str.toString();
        }
        
        private String padLeft(String original, String pad, int upTo){
            int npadding = upTo - original.length();
            StringBuilder str = new StringBuilder();
            if(npadding>0) for(int i = 0;i < npadding; i++) str.append(pad);
            str.append(original);
            return str.toString();
        }
        
        public void byteBufferBinaryString(ByteBuffer buffer){
            int nwords = buffer.array().length/4;
            for(int i = 0; i < nwords; i++){
                int value = buffer.getInt(i*4);
                System.out.println(String.format("  word %4d : %36s  0x%8s : %18d", i,
                        padLeft(Integer.toBinaryString(value),"0",32),
                        padLeft(Integer.toHexString(value),"0",8),
                        value));
            }
        }
        
        public static void main(String[] args){
            RecordHeader header = new RecordHeader();
            
            header.setCompressedDataLength(861);
            header.setDataLength(12457);
            header.setUserHeaderLength(459);
            header.setIndexLength(324);
            header.setLength(16 + header.getCompressedDataLengthWords());
            header.setUserRegisterFirst(1234567L);
            header.setUserRegisterSecond(4567890L);
            header.setRecordNumber(23);
            header.setEntries(3245);
            header.setHeaderLength(14);
            header.setCompressionType(1);
            System.out.println(header);
            
            byte[] array = new byte[14*4];
            ByteBuffer buffer = ByteBuffer.wrap(array);
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            
            
            header.writeHeader(buffer);
            
            //header.byteBufferBinaryString(buffer);
            
            RecordHeader header2 = new RecordHeader();
            header2.readHeader(buffer);
            System.out.println(header2);
        }
}
