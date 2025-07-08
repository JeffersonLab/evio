/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.jlab.coda.jevio.*;

/**
 * Class used to convert evio files into hipo files.
 * @author gavalian
 */
public class Evio6Converter {

    /**
     * Method to convert evio file into hipo file.
     * Uses EvioCompactReader internally and compresses data to lz4-best.
     * Note this only works with evio version 4 and earlier files, not version 6.
     *
     * @param inFile input evio format file.
     * @param outFile output hipo format file.
     * @param neventsToConvert number of events in input file to convert (starting from first),
     *                         or &lt;= 0 for all events.
     */
    public static void convert(String inFile, String outFile, int neventsToConvert) {

        try {
            EvioCompactReader reader = new EvioCompactReader(inFile);
            int nevents = reader.getEventCount();
            ByteOrder order = reader.getByteOrder();

            // If an EvioCompactReader is reading a buffer, then that buffer may or may not have
            // a backing array. However, if reading a file, we're guaranteed that any event
            // we retrieve in ByteBuffer form is backed by an array.

            System.out.println(" THE FILE ORDER = " + order);
            Writer writer = new Writer(outFile,order,10000,8*1024*1024);
            writer.setCompressionType(CompressionType.RECORD_COMPRESSION_LZ4_BEST);
            System.out.println("OPENED FILE: ENTRIES = " + nevents);
            long start_writer = System.currentTimeMillis();

            for(int i = 1; i < nevents; i++){
                // Get data representing event in ByteBuffer form (ready to read)
                ByteBuffer buffer = reader.getEventBuffer(i, false);

                // Take offset of buffer in underlying array into account.
                int bufOffset = 0;
                boolean hasArray = buffer.hasArray();
                if (hasArray) {
                    bufOffset = buffer.arrayOffset();
                }

                int offset = buffer.position() + bufOffset;
                int length = buffer.limit();

                // Get array underlying buffer
                byte[] evioData = buffer.array();

                buffer.order(ByteOrder.LITTLE_ENDIAN);
                int nsize = 20;
                if(evioData.length<80) nsize = evioData.length/4;
                for(int k = 0; k < nsize; k++){
                  //  System.out.print(String.format("0x%08X ", buffer.getInt(k*4)));
                  //      if((k+1)%10==0) System.out.println();
                }
                //System.out.println();
                writer.addEvent(evioData, offset, length);
                if (neventsToConvert >= 0  &&  i > neventsToConvert) break;
                if (i%500==0) {
                    System.out.println(" processed events # " + i);
                }
            }
            writer.close();
            long end_writer = System.currentTimeMillis();
            double time = (end_writer-start_writer)/1000.0;
            System.out.println(String.format("\nWRITING TIME : %.2f sec\n", time));
        } catch (EvioException | IOException ex) {
            Logger.getLogger(Evio6Converter.class.getName()).log(Level.SEVERE, null, ex);
        }
    }


    /**
     * Method to convert evio file into hipo file.
     * Internally uses EvioReader set to read events sequentially.
     *
     * @param inFile input evio format file.
     * @param outFile output hipo format file.
     * @param nthreads number of threads used to compress data.
     * @param compressionType type of data compression to use.
     */
    public static void convert(String inFile, String outFile, int nthreads, CompressionType compressionType){

        try {
            EvioReader reader = new EvioReader(new File(inFile),false,true);
            int nevents = reader.getEventCount();
            ByteOrder order = reader.getByteOrder();
            WriterMT writer = new WriterMT(order,10000,8*1024*1024,compressionType,
                    nthreads,nthreads*2);
            writer.open(outFile);
            System.out.println("OPENED FILE: ENTRIES = " + nevents);
            long start_writer = System.currentTimeMillis();

            for(int i = 1; i < nevents; i++) {
                EvioEvent event = reader.nextEvent();
                byte[]    data  = event.getRawBytes();
                writer.addEvent(data);
                if (i%5000==0) {
                    System.out.println(" processed events # " + i);
                }
            }
            writer.close();
            long end_writer = System.currentTimeMillis();
            double time = (end_writer-start_writer)/1000.0;
            System.out.println(String.format("\nWRITING TIME : %.2f\n", time));

        } catch (HipoException | EvioException | IOException ex) {
            Logger.getLogger(Evio6Converter.class.getName()).log(Level.SEVERE, null, ex);
        }
    }


    /**
     * Example use of this class.
     * @param args if any args, 1st = number of threads for compression,
     *             2nd = compression type, 3rd = input file, 4th = output file.
     *             If no args, use 2 threads for lz4-fast compression and default file names.
     */
    public static void main(String[] args){
        
        System.out.println("Hello World...... new horizons with N EVENTS");

        int numberOfThreads =  2;
        CompressionType compressionType = CompressionType.RECORD_COMPRESSION_LZ4;

        String inputFile = "./clas_000810.evio.324";
        String outputFile = "converted_v6.evio";
        
        if (args.length > 0) {
            if (args.length < 4) {
                System.out.println("\n\n\t Usage : converter [n-threads] [c-type] [input file] [out file]");
                System.out.println("\n");
                System.exit(0);
            }

            numberOfThreads = Integer.parseInt(args[0]);

            int ctype = Integer.parseInt(args[1]);
            if (ctype > 4 || ctype < 0) {
                compressionType = CompressionType.RECORD_UNCOMPRESSED;
            }
            else {
                compressionType =  CompressionType.getCompressionType(ctype);
            }

            inputFile  = args[2];
            outputFile = args[3];
        }

        long  start_time = System.currentTimeMillis();
        Evio6Converter.convert(inputFile, outputFile, numberOfThreads, compressionType);
        long  end_time   = System.currentTimeMillis();
        
        long time_diff = end_time - start_time;
        double duration = time_diff/1000.0;
        System.out.println(String.format(" Time elapsed = %.2f sec",duration));
    }
}
