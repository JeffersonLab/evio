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
    private int bytesViewed = 80;
    private int eventNumber;
    boolean checkSwap = false;
    private boolean lookAtAllCompositeData = false;
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
            else if (args[i].equalsIgnoreCase("-a")) {
                lookAtAllCompositeData = true;
            }
            else if (args[i].equalsIgnoreCase("-s")) {
                checkSwap = true;
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
            "        [-a]           show ALL composite data (not just errors)\n" +
            "        [-s]           swap data as well\n" +
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
                        ByteBuffer swapBuf = null;
                        ByteBuffer compBuffer = node.getByteData(true);

                        try {
                            //CompositeData compData = new CompositeData(cData, reader.getByteOrder());

                            if (checkSwap) {
                                byte[] cData = compBuffer.array();
                                byte[] swapped = new byte[cData.length];
                                swapBuf = ByteBuffer.wrap(swapped);
                                CompositeData.swapAll(cData, 0, swapped, 0, cData.length / 4, compBuffer.order());
                            }

                            if (lookAtAllCompositeData) {
                                printData(i, node, compBuffer, false);

                                if (checkSwap) {
                                    System.out.println("\nNow look at swapped data\n");
                                    printData(i, node, swapBuf, false);
                                }
                            }
                        }
                        catch (EvioException e) {
                            printData(i, node, compBuffer, true);
                            System.out.println("ERROR: " + e.getMessage());
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


    /** Print out some of the composite data in hex. */
    private void printData(int eventNumber, EvioNode node,
                           ByteBuffer compBuffer, boolean hasError) {
        // Print out the bytes
        int tag = node.getTag();
        int num = node.getNum();

        // Only look at specific tag/num if specified on command line
        if (filterTag > -1 && tag != filterTag) {
            return;
        }
        if (filterNum > -1 && num != filterNum) {
            return;
        }

        String label;

        if (hasError) {
            label = "Composite bank (contains error) for event " + eventNumber;
        }
        else {
            label = "Composite bank for event " + eventNumber;
        }

        label += ", tag = " + tag + "(0x" + Integer.toHexString(tag) +
                 "), num = " + num + "(0x" + Integer.toHexString(num) +
                 "), pad = " + node.getPad() + ", pos = " + node.getPosition() +
                 ", data len = " + node.getDataLength() + " words";

        Utilities.printBytes(compBuffer, 0, bytesViewed, label);
    }


}
