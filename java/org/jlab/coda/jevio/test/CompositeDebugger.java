/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;


import java.nio.ByteBuffer;
import java.util.ArrayList;

/**
 * Class to find a bad composite banks and print out the bad data in hex.
 *
 * @author timmer
 * @since 11/15/17
 */
public class CompositeDebugger {

    private int filterTag = -1;
    private int filterNum = -1;
    private int bytesViewed = 20;
    private int eventNumber;
    private String fileName = "/daqfs/home/timmer/rafopar044.evio";


    /**
     * Method to decode the command line used to start this application.
     * @param args command line arguments
     */
    private void decodeCommandLine(String[] args) {

        // loop over all args
        for (int i = 0; i < args.length; i++) {

            if (args[i].equalsIgnoreCase("-h")) {
                usage();
                System.exit(-1);
            }
            else if (args[i].equalsIgnoreCase("-t")) {
                filterTag = Integer.parseInt(args[i + 1]);
                i++;
            }
            else if (args[i].equalsIgnoreCase("-n")) {
                filterNum = Integer.parseInt(args[i + 1]);
                i++;
            }
            else if (args[i].equalsIgnoreCase("-e")) {
                eventNumber = Integer.parseInt(args[i + 1]);
                i++;
            }
            else if (args[i].equalsIgnoreCase("-b")) {
                bytesViewed = Integer.parseInt(args[i + 1]);
                i++;
            }
            else if (args[i].equalsIgnoreCase("-f")) {
                fileName = args[i + 1];
                i++;
            }
            else {
                usage();
                System.exit(-1);
            }
        }

        return;
    }


    /** Method to print out correct program command line usage. */
    private static void usage() {
        System.out.println("\nUsage:\n\n" +
            "   java org.jlab.coda.jevio.test.CompositeDebugger\n" +
            "        [-t <tag>]     tag of structures to look for\n"+
            "        [-n <num>]     num of structures to look for\n" +
            "        [-e <event #>] number of specific event to look at\n" +
            "        [-b <bytes>]   bytes of bad structure to view\n" +
            "        [-f <file>]    file to read\n" +
            "        [-h]           print this help\n");
    }


    /** Constructor. */
    CompositeDebugger(String[] args) {
        decodeCommandLine(args);
    }

    /** Run as a stand-alone application. */
    public static void main(String[] args) {
        CompositeDebugger debugger = new CompositeDebugger(args);
        debugger.run();
    }

    public void run() {

        int evCount;

        try {
            EvioCompactReader reader = new EvioCompactReader(fileName);

            // Get each event in the buffer
            evCount = reader.getEventCount();
            System.out.println("Event count = " + evCount);

            topLoop:
            for (int i=1; i <= evCount; i++) {
                // If event # specified
                if (eventNumber > 0 && eventNumber != i) {
                    continue;
                }

                EvioNode eventNode = reader.getScannedEvent(i);
                ArrayList<EvioNode> nodeList = eventNode.getAllNodes();

                // Look through all structures (bank, seg, tagseg) associated with this event
                for (EvioNode node : nodeList) {
                    // Pick out those whose data type is composite
                    if (node.getDataTypeObj() == DataType.COMPOSITE) {
                        ByteBuffer compBuffer = node.getByteData(true);
                        try {
                            CompositeData compData = new CompositeData(compBuffer.array(), reader.getByteOrder());
                        }
                        catch (EvioException e) {
                            // Print out the bad bytes
                            int tag = node.getTag();
                            int num = node.getNum();

                            // Only look at specific tag/num if specified on command line
                            if (filterTag > -1 && tag != filterTag) {
                                continue;
                            }
                            if (filterNum > -1 && num != filterNum) {
                                continue;
                            }

                            String label = "Bad composite bank for event " + (i) +
                                    ", tag = " + tag + "(0x" + Integer.toHexString(tag) +
                                    "), num = " + num + "(0x" + Integer.toHexString(num) +
                                    "), pad = " + node.getPad() + ", pos = " + node.getPosition();
                            Utilities.printBuffer(compBuffer, 0, bytesViewed, label);
                        }
                    }
                }
            }

            reader.close();

        }
        catch (Exception e) {
            e.printStackTrace();
            System.exit(-1);
        }
    }


}
