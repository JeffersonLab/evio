/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.jlab.coda.jevio.EvioCompactReader;
import org.jlab.coda.jevio.EvioException;

/**
 *
 * @author gavalian
 */
public class Evio6Converter {
    
    public static void convert(String inFile, String outFile){
        EvioCompactReader  reader;
        try {
            
            reader = new EvioCompactReader(inFile);
            int nevents = reader.getEventCount();
            ByteOrder order = reader.getByteOrder();
            
            order = ByteOrder.BIG_ENDIAN;
            
            System.out.println(" THE FILE ORDER = " + order);
            Writer writer = new Writer(outFile,order,10000,8*1024*1024);
            writer.setCompressionType(1);
            System.out.println("OPENED FILE: ENTRIES = " + nevents);
            long start_writer = System.currentTimeMillis();
            for(int i = 1; i < nevents; i++){
                ByteBuffer buffer = reader.getEventBuffer(i,true); 
                writer.addEvent(buffer.array());
                if(i%500==0){
                    System.out.println(" processed events # " + i);
                }
            }
            writer.close();
            long end_writer = System.currentTimeMillis();
            double time = (end_writer-start_writer)/1000.0;
            System.out.println(String.format("\nWRITING TIME : %.2f sec\n", time));
        } catch (EvioException ex) {
            Logger.getLogger(Evio6Converter.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(Evio6Converter.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    public static void convert(String inFile, String outFile, int nthreads, int compressionType){
        EvioCompactReader  reader;
        try {
            
            reader = new EvioCompactReader(inFile);
            int nevents = reader.getEventCount();
            ByteOrder order = reader.getByteOrder();
            WriterMT writer = new WriterMT(order,10000,8*1024*1024,compressionType,
                    nthreads,nthreads*4);
            writer.open(outFile);
            System.out.println("OPENED FILE: ENTRIES = " + nevents);
            long start_writer = System.currentTimeMillis();
            for(int i = 1; i < nevents; i++){
                ByteBuffer buffer = reader.getEventBuffer(i,true);
                writer.addEvent(buffer.array());
                if(i%5000==0){
                    System.out.println(" processed events # " + i);
                }
            }
            writer.close();
            long end_writer = System.currentTimeMillis();
            double time = (end_writer-start_writer)/1000.0;
            System.out.println(String.format("\nWRITING TIME : %.2f\n", time));
        } catch (EvioException ex) {
            Logger.getLogger(Evio6Converter.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(Evio6Converter.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    public static void main(String[] args){
        
        System.out.println("Hello World...... new horizons");
        
        int numberOfThreads = 2;
        int compressionType = 1;
        
        
        String inputFile = "/Users/gavalian/Work/Software/project-2a.0.0/clas_000810.evio.324";
        String outputFile = "converted_v6.evio";
        
        if(args.length>0){
            if(args.length<4){
                System.out.println("\n\n\t Usage : converter [n-threads] [c-type] [input file] [out file]");
                System.out.println("\n");
                System.exit(0);
            }
            numberOfThreads = Integer.parseInt(args[0]);
            compressionType = Integer.parseInt(args[1]);
            inputFile  = args[2];
            outputFile = args[3];
        }
        /*
        if(args.length>1){
            outputFile = args[1];
        }*/
        
        //Evio6Converter.convert(inputFile, outputFile, 4);
        long  start_time = System.currentTimeMillis();
        //Evio6Converter.convert(inputFile, outputFile);
        Evio6Converter.convert(inputFile, outputFile, numberOfThreads, compressionType);
        long  end_time   = System.currentTimeMillis();
        
        long time_diff = end_time - start_time;
        double duration = time_diff/1000.0;
        System.out.println(String.format(" Time elapsed = %.2f sec",duration));
    }
}
