package org.jlab.coda.jevio.test;


import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

/**
 * Test program.
 * @author timmer
 * Feb 14, 2014
 */
public class CompactBuilderTest extends TestBase {


    ByteOrder order = ByteOrder.nativeOrder();


    /** For WRITING a local file. */
    public static void main(String args[]) {

        CompactBuilderTest tester = new CompactBuilderTest();
        //tester.CompactEBTest();
        tester.searchBuffer(3, 3);
    }


    public void searchBuffer(int tag, int num) {

        int bufSize = 20000;

        try {
            // Create buffer with data
            CompactEventBuilder builder = new CompactEventBuilder(bufSize, order, false);
            buffer = createCompactEventBuffer(1, 1, order, bufSize, builder);

            // Create buffer to write into
            ByteBuffer writeBuf = ByteBuffer.allocate(bufSize);
            writeBuf.order(order);
            EventWriter writer = new EventWriter(writeBuf, dictionary);

            writer.writeEvent(buffer);
            writer.close();
            // Get writeBuf back but with pos & lim set for reading
            writeBuf = writer.getByteBuffer();

            System.out.println("searchBuffer: create EvioCompactReader to read newly created writeBuf");
            EvioCompactReader reader = new EvioCompactReader(writeBuf);
            // search event #1 for struct with tag, num
            System.out.println("searchBuffer: search event #1 for tag = " + tag + ", num = " + num);
            List<EvioNode> returnList = reader.searchEvent(1, tag, num);
            if (returnList.isEmpty()) {
                System.out.println("GOT NOTHING IN SEARCH for ev 1, tag = " + tag + ", num = " + num);
            }
            else {
                System.out.println("Found " + returnList.size() + " structs");
                for (EvioNode nd : returnList) {
                    System.out.println("NODE: " + nd + "\n");
                }
            }

            System.out.println("searchBuffer: search event #1 for dict entry \"JUNK\"");
            EvioXMLDictionary xmlDict = new EvioXMLDictionary(dictionary);
            List<EvioNode> retList = reader.searchEvent(1, "JUNK", xmlDict);
            if (retList.isEmpty()) {
                System.out.println("GOT NOTHING IN SEARCH for dictionary entry = InBank");
            }
            else {
                System.out.println("Found " + retList.size() + " structs");
                for (EvioNode nd : retList) {
                    System.out.println("NODE: " + nd + "\n");
                }
            }


                // To do more extensive searches, need to use EvioReader and EvioEvent .

            // Match only tags, not num
            System.out.println("searchBuffer: create EvioReader to read newly created writeBuf");
            EvioReader reader2 = new EvioReader(writeBuf);
            EvioEvent event = reader2.parseEvent(1);
            tag = 41;
            System.out.println("searchBuffer: get matching struct for tag = " + tag);
            List<BaseStructure> returnList2 = StructureFinder.getMatchingStructures(event, tag);
            if (returnList2.isEmpty()) {
                System.out.println("GOT NOTHING IN SEARCH for ev 1, tag = " + tag);
            }
            else {
                System.out.println("Using StructureFinder , found " + returnList2.size() + " structs");
                for (BaseStructure ev : returnList2) {
                    System.out.println("Struct: " + ev);
                }
            }

        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }





}
