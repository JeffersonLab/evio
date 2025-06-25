/*
 * Copyright (c) 2014, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio.dev;

import java.io.IOException;

import org.jlab.coda.jevio.EvioException;
import org.jlab.coda.jevio.EvioReader;

public class EvioReadFileSimple {
    
    public static void main(String[] args) {
        // This is a simple example of how to read an Evio file.
        // It assumes that the Evio file is already created and contains events.
        
        if (args.length != 1) {
            System.out.println("Usage: java EvioReadFileSimple <evio-file>");
            return;
        }

        String filename = args[0]; // Replace with your Evio file name
        
        try {
            EvioReader reader = new EvioReader(filename);

            int evCount = reader.getEventCount();

            reader.close();
        } catch (EvioException | IOException e) {
            e.printStackTrace();
        }
    }

}
