//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;
import java.util.Map;

public class DictTest2 extends TestBase {


        /** Writing to a buffer using new interface. */
        public void createCompactEvents(int tag, int num) throws EvioException, IOException {

            try {

                buffer.order(ByteOrder.nativeOrder());
                CompactEventBuilder builder = new CompactEventBuilder(buffer);
                ByteBuffer buf = createCompactEventBuffer(tag, num, ByteOrder.nativeOrder(), 200000, builder);


                EventWriter writer = new EventWriter(new File(writeFileName1), dictionary, false);
                writer.setFirstEvent(buf);
                writer.writeEvent(buf);
                writer.close();

                // Read event back out of file
                EvioReader reader = new EvioReader(writeFileName1);

                System.out.println("createCompactEvents: have dictionary? " + reader.hasDictionaryXML());
                String xmlDict = reader.getDictionaryXML();
                System.out.println("createCompactEvents: read dictionary ->\n\n" + xmlDict + "\n");

                System.out.println("createCompactEvents: have first event? " + reader.hasFirstEvent());
                EvioEvent fe = reader.getFirstEvent();
                System.out.println("createCompactEvents: read first event ->\n\n" + fe.toString() + "\n");

                System.out.println("createCompactEvents: try getting ev from file");;
                EvioEvent ev = reader.parseEvent(1);
                System.out.println("createCompactEvents: event ->\n" + ev.treeToString(""));
                // This sets the proper pos and lim in buf
                ByteBuffer bb = builder.getBuffer();
                System.out.println("createCompactEvents: buf = \n" + bb.toString());


                System.out.println("createCompactEvents: create dictionary object\n");;
                EvioXMLDictionary dict = new EvioXMLDictionary(dictionary);
                System.out.println("createCompactEvents: dictionary ->\n\n" + dict.toString());

                System.out.println("\n\ncreateCompactEvents: search, using dictionary for struct = JUNK");;
                List<BaseStructure> vec =  StructureFinder.getMatchingStructures(ev, "JUNK", dict);
                for (BaseStructure bs : vec) {
                    System.out.println("createCompactEvents: found, thru dict -> " + bs.toString());
                }

                System.out.println("\n\n");
                vec.clear();

                vec = StructureFinder.getMatchingStructures(ev, "SEG5", dict);
                System.out.println("createCompactEvents: find SEG5 -> ");;
                for (BaseStructure bs : vec) {
                    System.out.println("createCompactEvents: found, thru dict -> " + bs.toString());
                }
                System.out.println("\n\n");
                vec.clear();

                //<bank name="TopLevel"   tag="1"  num="1" type="bank" >
                //  <bank name="2Level"   tag="201-203"    type="bank" >
                vec = StructureFinder.getMatchingStructures(ev, "Top.2ndLevel", dict);
                System.out.println("createCompactEvents: find Top.2ndLevel -> ");
                for (BaseStructure bs : vec) {
                    System.out.println("createCompactEvents: found, thru dict -> " + bs.toString());
                }
                System.out.println("\n\n");
                vec.clear();

                //  <leaf name="TagSegUints"   tag="17" /> <<
                System.out.println("createCompactEvents: find Top.2ndLevel.TagSegUints -> ");;
                vec = StructureFinder.getMatchingStructures(ev, "Top.2ndLevel.TagSegUints", dict);
                for (BaseStructure bs : vec) {
                    System.out.println("createCompactEvents: found, thru dict -> " + bs.toString());
                }
                System.out.println("\n\n");
                vec.clear();


                DataType type = dict.getType(101, 101);
                if (type == null) {
                    System.out.println("createCompactEvents: type of tag=101, num=101 has no type");;
                }
                else {
                    System.out.println("createCompactEvents: type of tag=101, num=101 -> " + type.toString());
                }


                //std::string entry = "Top.2ndLevel";
                String entry = "Top.2ndLevel.BankUints";
                //std::string entry = "Top.2ndLevel.SegInts";

                if (dict.exists(entry)) {
                    int tag1, tagEnd1; int num1;
                    // This is a tag range
                    if (dict.isTagRange(entry)) {
                        Integer[] ia = dict.getTagNum(entry);
                        System.out.println("createCompactEvents: tag range of 2nd Level Banks = " + ia[0] + " - " + ia[2]);
                    }
                    else if (dict.isTagNum(entry)) {
                        Integer[] ia = dict.getTagNum(entry);
                        System.out.println("createCompactEvents: tag & num of 2nd Level Banks = tag " + ia[0] + ", num " + ia[1]);
                    }
                    else if (dict.isTagOnly(entry)) {
                        Integer[] ia = dict.getTagNum(entry);
                        System.out.println("createCompactEvents: tag of 2nd Level Banks = " + ia[0] );
                    }
                    else {
                        System.out.println("createCompactEvents: internal error finding tag/tagEnd/num");
                    }
                }
                else {
                    System.out.println("createCompactEvents: no tag for 2nd Level Banks" );;
                }


                Integer tagg = dict.getTag("Tag 5");
                if (tagg != null) {
                    System.out.println("createCompactEvents: tag for dict entry \"Tag 5\" = " + tag);
                }
                else {
                    System.out.println("createCompactEvents: no dict entry for \"Tag 5\"" );;
                }


                // Retrieve & print info from dictionary
                entry = "CompositeData";
                System.out.println("Getting stuff for name = \"CompositeData\":");
                tag = dict.getTag(entry);
                System.out.println("    tag         = " + tag);
                Integer num_ = dict.getNum("CompositeData"); // check for null
                System.out.println("    num         = " + num);
                System.out.println("    type        = " + dict.getType(entry).toString());
                System.out.println("    format      = " + dict.getFormat(entry));
                System.out.println("    description = " + dict.getDescription(entry));

                System.out.println("Getting stuff for tag = 8, num = 8:");;
                System.out.println("    type        = " + dict.getType(8, 8).toString());
                System.out.println("    name        = " + dict.getName(8,8));
                System.out.println("    format      = " + dict.getFormat(8,8));
                System.out.println("    description = " + dict.getDescription(8,8));

                // first -> name, second -> sharedptr to entry
                System.out.println("\n\nPrint out contents of dictionary:");;
                Map<String, EvioDictionaryEntry> map = dict.getMap();
                for (Map.Entry<String, EvioDictionaryEntry> entry1 : map.entrySet()) {
                    String key = entry1.getKey();
                    EvioDictionaryEntry value = entry1.getValue();
                    System.out.println("   " + key + " :   " + value.toString());
                }

            }
            catch (EvioException e) {
                e.printStackTrace();
            }
        }


        /** Writing to a buffer using original evio interface. */
        void createObjectEvents(int tag, int num) throws EvioException, IOException{

            try {

                EvioEvent event = createEventBuilderEvent(tag, num);

                System.out.println("Event:\n" + event.treeToString(""));
                System.out.println("Event Header:\n" + event.getHeader().toString());

                // Take event & write it into buffer
                EventWriter writer = new EventWriter(new File(writeFileName1), dictionary, false);
                System.out.println("    createObjectEvents: set First Event, size bytes = " + event.getTotalBytes());

            //    writer.setFirstEvent(event);
                writer.writeEvent(event);
                writer.close();

                // Read event back out of file
                EvioReader reader = new EvioReader(writeFileName1);

                System.out.println("    createObjectEvents: have dictionary? " + reader.hasDictionaryXML());
                String xmlDict = reader.getDictionaryXML();
                System.out.println("    createObjectEvents: read dictionary ->\n\n" + xmlDict);

                System.out.println("    createObjectEvents: have first event? " + reader.hasFirstEvent());
                if (reader.hasFirstEvent()) {
                    EvioEvent fe = reader.getFirstEvent();
                    System.out.println("    createObjectEvents: read first event ->\n\n" + fe.treeToString(""));
                }

                System.out.println("    createObjectEvents: try getting ev #1");;
                EvioEvent ev = reader.parseEvent(1);
                System.out.println("    createObjectEvents: event ->\n" + ev.treeToString(""));
            }
            catch (EvioException e) {
                e.printStackTrace();
            }

        }

    public static void main(String args[]) {
        try {
            DictTest2 tester = new DictTest2();
            tester.createCompactEvents(1,1);
            tester.createObjectEvents(1,1);
        }
        catch (EvioException | IOException e) {
            e.printStackTrace();
        }
    }





}