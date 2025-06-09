package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.DoubleBuffer;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Created by IntelliJ IDEA.
 * User: timmer
 * Date: 4/12/11
 * Time: 10:21 AM
 * To change this template use File | Settings | File Templates.
 */
public class DictTest extends TestBase {

    static String xmlDict4 =
            "<JUNK>\n"  +
                    "<moreJunk/>"  +

                    "<xmlDict attr='junk'>\n"  +
                    "<leaf name='leaf21' tag= '2.1' num = '2.1' />\n"  +
                    "<leaf name='leaf2'  tag= '2'   num = '2' />\n"  +
                    "<leaf name='leaf3'  tag= '2'   num = '2' />\n"  +

                    "<dictEntry name='pretty-print'  tag= '456' />\n"  +
                    "<dictEntry name='first'  tag= '123'   num = '123' />\n"  +
                    "<dictEntry name='second'  tag= '123'   num = '123' />\n"  +
                    "<dictEntry name='a' tag= '1.7'   num = '1.8' />\n"  +

                    "<bank name='b10tag' tag= '10' />\n"  +
                    "<bank name='b5tag'  tag= '5' num='5' />\n"  +

                    "<bank name='b1' tag= '10' num='0' attr ='gobbledy gook' >\n"  +
                        "<bank name='b2' tag= '20' num='20' >\n"  +
                            "<leaf name='l1' tag= '30' num='31'>\n"  +
                                "<bank name='lowest' tag= '111' num='222' />\n"  +
                            "</leaf>\n" +
                            "<leaf name='l2' tag= '31' num='32' />\n"  +
                        "</bank>\n"  +
                    "</bank>\n"  +

                    "</xmlDict>\n" +

                    "<xmlDict>\n"  +
                    "<leaf name='leaf21' tag= '3' num = '3' />\n"  +
                    "<leaf name='a'  tag= '33'   num = '44' />\n"  +
                    "</xmlDict>\n" +

                    "</JUNK>";


    static String description =
            "\n" +
            "     i  TDC some comment\n" +
            "     F  ADC blah min=5\n" +
            "     N  multiplier\n";

    static String xmlDict5 =
            "<xmlDict>\n" +
                "<dictEntry name=\"first\"  tag=\"123\"   num =\"  456B\" type=\"ComPosiTe\" >\n" +
                    "<description format='FD2i' >\n" +
                        description +
                    "</description>\n" +
                "</dictEntry>\n" +

                "<dictEntry name=\"second(%n)\"  tag=\"234\"   num =\"  254  -  256 \" type=\"Bank\" />\n" +
                "<dictEntry name=\"third\"  tag=\"456\"   num =\"  1  -   BLAH \" type=\"SegmENT\" />\n" +
                "<dictEntry name=\"fourth\"  tag=\"567\"   num =\"  BLAH  -   3 \" type=\"TAGsegment\" />\n" +
                "<dictEntry name=\"fifth\"  tag=\"678\"   num =\"256\" type=\"Bank\" />\n" +

                "<dictEntry name=\"A(%t)\"  tag=\"65536\"   num =\"1\" />\n" +
                "<dictEntry name=\"Arange\"  tag=\"65534-65536\"  />\n" +
                "<dictEntry name=\"Brange\"  tag=\"  1 -  3 \"  />\n" +
                "<dictEntry name=\"B\"  tag=\"  Z1\"  />\n" +

                "<bank name='b1'   tag='10'   num='0' type='inT32' >\n" +
                    "<description format='2(N3F)' >" +
                        "this is a bank of signed 32 bit integers" +
                    "</description>\n" +

                    "<bank name=\"a(%n)\"  tag=\"234\"   num =\"  254  -  256 \" type=\"Bank\" />\n" +
                    "<bank name=\"b\"  tag=\"456\"   num =\"  1  -   BLAH \" type=\"SegmENT\" />\n" +
                    "<bank name=\"c\"  tag=\"567\"   num =\"  BLAH  -   3 \" type=\"TAGsegment\" />\n" +
                    "<bank name=\"d\"  tag=\"678\"   num =\"256\" type=\"Bank\" />\n" +

                    "<leaf name=\"A(%t)\"  tag=\"65536\"   num =\"1\" />\n" +
                    "<leaf name=\"Arange\"  tag=\"65534-65536\"  />\n" +
                    "<leaf name=\"Brange\"  tag=\"  2 -  4 \"  />\n" +
                    "<leaf name=\"B\"  tag=\"  Z1\"  />\n" +

                    "</bank>" +
            "</xmlDict>";


    static String xmlDict7 =

            // There are several format errors in the dictionary below
            // They'll show up in the printout
            // "description" is a node not an attribute!!

            "<xmlDict>\n" +
                    "  <bank name=\"HallD\"          tag=\"6-8\"  >\n" +
                    "      <description format=\"blah\" >\n"  +
                    "          hall_d_tag_range"  + "\n" +
                    "      </description>\n" +
                    "      <bank name=\"TAG7\"       tag=\"7\"  />\n" +
                    "      <bank name=\"DC(%t)\"     tag=\"6\" num=\"0\" >\n" +
                    "          <description format=\"DC Format\" >tag 6, num 0 bank</description>" +
                    "          <leaf name=\"xpos(%n)\"   tag=\"6\" num=\"1\" type=\"BLAH_BLAH\" />\n" +
                    "          <bank name=\"ypos(%n)\"   tag=\"6\" num=\"2\" />\n" +
                    "          <bank name=\"zpos(%n)\"   tag=\"6\" num=\"3\" />\n" +
                    "          <bank name=\"zpos(%t-%n)\"   tag=\"6\" num=\"4-5\" />\n" +
                    "          <bank name=\"tag72only\"          tag=\"72\" type=\"TAG_72_ONLY\" />\n" +
                    "          <bank name=\"tag72only_dup\"      tag=\"72\" />\n" +
                    "          <bank name=\"tagrange73-74\"      tag=\"73-74\" type=\"BaNk\"  />\n" +
                    "          <bank name=\"tagrange73-74_dup\"  tag=\"73-74\" />\n" +
                    "      </bank >\n" +
                    "      <bank name=\"TOF\"        tag=\"8\" num=\"0\"  type=\"bank\" >\n" +
                    "          <bank name=\"ypos(%n)\"   tag=\"6\" num=\"2\" />\n" +
                    "          <bank name=\"duplicate_ypos(2)\"   tag=\"6\" num=\"2\" />\n" +
                    "          <leaf name=\"xpos\"   tag=\"8\" num=\"1\" >\n" +
                    "               <leaf name=\"xpos_leaf\"   tag=\"9\" num=\"1\" />\n" +
                    "          </leaf >\n" +
                    "          <bank name=\"ypos\"   tag=\"8\" num=\"2\" />\n" +
                    "      </bank >\n" +
                    "  </bank >\n" +
                    "  <dictEntry name=\"BadTag\" tag=\"55\" type=\"JunkType\" />\n" +
                    "  <dictEntry name=\"BadType??\" tag=\"66\" num=\"6\" type=\"ActualJunk\" />\n" +
                    "  <dictEntry name=\"TaggiesOnly\" tag=\"5\" num=\"3\" type=\"bANk\" >\n" +
                    "       <description format=\"My Format\" >tag 5 description</description>\n" +
                    "  </dictEntry>\n" +
                    "  <dictEntry name=\"Rangy_Small\" tag=\"75 - 76\"  />\n" +
                    "  <dictEntry name=\"Rangy\"       tag=\"75 - 78\"  />\n" +
                    "  <dictEntry name=\"TAG11\"       tag=\"11\" num=\"ZAP\" />\n" +
                    "  <dictEntry name=\"TAG12\"       tag=\"12\" type=\"bank\" description=\"desc_is_not_attribute\" />\n" +
                    "  <dictEntry name=\"TAG1\"        tag=\"1\" />\n" +
                    "  <dictEntry name=\"TAG1\"        tag=\"2\" />\n" +
                    "  <dictEntry name=\"num=(%t-%n)\"  tag=\"123\" num=\"1-7\" />\n" +
                    "  <dictEntry name=\"num=(7)\"      tag=\"123\" num=\"7\" />\n" +
                    "  <dictEntry name=\"num=(%n)\"     tag=\"123\" />\n" +
            "</xmlDict>"
            ;


    public static void main(String args[]) {
        DictTest test = new DictTest();
        test.testDict7();
        test.testDict5();
        test.testDict4();
        test.testDict3();
        test.testDict2();
    }



    public void testDict7() {

        EvioXMLDictionary dict = new EvioXMLDictionary(xmlDict7, null, true);

        System.out.println("\nValid dictionary entries:\n");


        Map<String, EvioDictionaryEntry> map = dict.getMap();
        int i=0;
        Set<Map.Entry<String, EvioDictionaryEntry>> set = map.entrySet();
        for (Map.Entry<String, EvioDictionaryEntry> entry : set) {
            String entryName =  entry.getKey();
            EvioDictionaryEntry entryData = entry.getValue();

            System.out.print("key = " + entryName + ", tag=" +
                    entryData.getTag() + ", tagEnd=" + entryData.getTagEnd());

            Integer num = entryData.getNum();
            if (num == null) {
                System.out.print(", num=<undefined>");
            }
            else {
                System.out.print(", num=" + num);
            }

            System.out.println(", type=" + entryData.getType() + ", entryType=" + entryData.getEntryType());
        }

        System.out.println();

        System.out.println("\ntagNumMap entries:\n");
        Map<EvioDictionaryEntry,String> map1 = dict.tagNumMap;
        Set<EvioDictionaryEntry> keys1 = map1.keySet();
        Set<Map.Entry<EvioDictionaryEntry,String>> entrySet = map1.entrySet();
        for (Map.Entry<EvioDictionaryEntry,String> ss : entrySet) {
            EvioDictionaryEntry entry = ss.getKey();
            String val = ss.getValue();
            System.out.println("VAL = " + val + ": KEY = " + entry);
        }
        System.out.println();

        Integer tag = 6;
        Integer num = 2;
        Integer tagEnd = 0;

        String n = dict.getName(tag, num, tagEnd);
        System.out.println("getName(tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd + ") = " + n);

        EvioDictionaryEntry e = new EvioDictionaryEntry(tag, num, tagEnd, null);
        String name = dict.tagNumMap.get(e);

        System.out.println("1 name = " + name + ", for tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd);

        EvioDictionaryEntry parentEntry = new EvioDictionaryEntry(6, 0, 0, null);
        EvioDictionaryEntry newEntry = new EvioDictionaryEntry(tag, num, tagEnd, null, null, null, parentEntry);
        name = dict.tagNumMap.get(newEntry);

        System.out.println("2 name = " + name + ", for tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd + " and parent = 6/0/0");


        Integer pTag = 8;
        Integer pNum = 0;
        Integer pTagEnd = 0;
        String nm = dict.getName(tag, num, tagEnd, pTag, pNum, pTagEnd);
        System.out.println("getName(tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd + ", pTag = " + pTag +
                                   ", pNum = " + pNum + ", pTagEnd = " + pTagEnd +
                                   ") = " + nm);

        System.out.println();

        System.out.println("description for tag = 5, num = 3, tagEnd = 0 => \"" +
                dict.getDescription(5, 3, 0) + "\"");

        System.out.println("description for tag = 6, num = 0, tagEnd = 0 => \"" +
                dict.getDescription(6, 0, 0) + "\"");

        System.out.println("description for tag = 6, num = 6, tagEnd = 0 => \"" +
                dict.getDescription(6, 6, 0) + "\"");

        System.out.println("format for tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd + " => \"" +
                           dict.getFormat(tag, num, tagEnd) + "\"");

        System.out.println();
        System.out.println("Dictionary.toString() gives:\n" + dict.toString());
    }

    public void testDict5() {

        EvioXMLDictionary dict = new EvioXMLDictionary(xmlDict5, null, true);
        System.out.println("\n\nNew Dictionary:\n" + dict.toString());

        boolean exists = dict.exists("second(1)");
        if (!exists) {
            System.out.println("Entry name = \"second(1)\" does not exist\n");
        }
        else {
            System.out.println("Getting stuff for name = \"second(1)\":");
            System.out.println("    tag         = " + dict.getTag("second(1)"));
            System.out.println("    num         = " + dict.getNum("second(1)"));
            System.out.println("    type        = " + dict.getType("second(1)"));
            System.out.println("    format      = " + dict.getFormat("second(1)"));
            System.out.println("    description = " + dict.getDescription("second(1)"));
        }

        System.out.println("Getting stuff for tag = 10, num = 0:");
        System.out.println("    type        = " + dict.getType(10,0));
        System.out.println("    name        = " + dict.getName(10,0));
        System.out.println("    format      = " + dict.getFormat(10,0));
        System.out.println("    description = " + dict.getDescription(10,0));


        System.out.println("\nGetting stuff for name = \"b1\":");
        System.out.println("    tag         = " + dict.getTag("b1"));
        System.out.println("    num         = " + dict.getNum("b1"));
        System.out.println("    type        = " + dict.getType("b1"));
        System.out.println("    format      = " + dict.getFormat("b1"));
        System.out.println("    description = " + dict.getDescription("b1"));
        System.out.println("");
    }


    public void testDict4() {

        EvioXMLDictionary dict = new EvioXMLDictionary(xmlDict4, null, true);
        System.out.println("\n\nNew Dictionary:\n" + dict.toString());


        int i=0;
        Map<String, EvioDictionaryEntry> map = dict.getMap();
        Set<Map.Entry<String, EvioDictionaryEntry>> set = map.entrySet();
        for (Map.Entry<String, EvioDictionaryEntry> entry : set) {
            String entryName =  entry.getKey();
            EvioDictionaryEntry entryData = entry.getValue();

            System.out.print("entry " + (++i) + "name = " + entryName + ", tag=" +
                    entryData.getTag() + ", tagEnd=" + entryData.getTagEnd());

            Integer num = entryData.getNum();
            if (num == null) {
                System.out.print(", num=<undefined>");
            }
            else {
                System.out.print(", num=" + num);
            }

            System.out.println(", type=" + entryData.getType() + ", entryType=" + entryData.getEntryType());
        }
        System.out.println();


        EvioEvent bank20 = new EvioEvent(456, DataType.BANK, 20);
        String dictName = dict.getName(bank20);
        System.out.println("Bank tag=456/num=20 corresponds to entry, \"" + dictName + "\"");

        EvioEvent bank11 = new EvioEvent(10, DataType.BANK, 0);
        dictName = dict.getName(bank11);
        System.out.println("Bank tag=10/num=0 corresponds to entry, \"" + dictName + "\"");

        EvioTagSegment tseg = new EvioTagSegment(10, DataType.INT32);
        dictName = dict.getName(tseg);
        System.out.println("TagSegment tag=10 corresponds to entry, \"" + dictName + "\"");

        EvioSegment seg = new EvioSegment(10, DataType.INT32);
        dictName = dict.getName(seg);
        System.out.println("Segment tag=10 corresponds to entry, \"" + dictName + "\"");

        EventBuilder builder = new EventBuilder(bank11);
        EvioBank bank12 = new EvioBank(20, DataType.SEGMENT, 20);
        try {
            builder.addChild(bank11, bank12);
        }
        catch (EvioException e) {}
        dictName = dict.getName(bank12);
        System.out.println("Bank tag=20/num=20 corresponds to entry, \"" + dictName + "\"");

        EvioEvent ev = new EvioEvent(2, DataType.INT32, 2);
        dictName = dict.getName(ev);
        System.out.println("Event tag=2/num=2 corresponds to entry, \"" + dictName + "\"");

        EvioSegment seg2 = new EvioSegment(5, DataType.INT32);
        dictName = dict.getName(seg2);
        System.out.println("Segment tag=5 corresponds to entry, \"" + dictName + "\"");
        System.out.println();




        System.out.println("TEST NEW FEATURE:");
        Integer[] tn = dict.getTagNum("b1.b2.l1");
        if (tn != null) {
            System.out.println("Dict entry of b1.b2.l1 has tag = " + tn[0] +
                            " and num = " + tn[1]);
        }

        tn = dict.getTagNum("a");
        if (tn != null) {
            System.out.println("Dict entry of \"a\" has tag = " + tn[0] +
                            " and num = " + tn[1]);
        }
        else {
            System.out.println("Dict NO entry for \"a\"");
        }

        tn = dict.getTagNum("b1.b2.l1.lowest");
        if (tn != null) {
            System.out.println("Dict entry of b1.b2.l1.lowest has tag = " + tn[0] +
                            " and num = " + tn[1]);
        }
        else {
            System.out.println("Dict NO entry for b1.b2.l1.lowest");
        }

        System.out.println("\nTEST NEW toXml() method:");
        System.out.println(dict.toXML());
    }


    public void testDict3()  {
        try {
            int tag = 1;
            int num = 1;

            buffer.order(ByteOrder.nativeOrder());
            ByteBuffer buf = createCompactEventBuffer(tag, num, ByteOrder.nativeOrder(), 200000, null);


            EventWriter writer = new EventWriter(buffer, dictionary);
            writer.writeEvent(buf);
            writer.close();

            // Read event back out of buffer
            EvioReader reader = new EvioReader(writer.getByteBuffer());

            EvioEvent ev = reader.parseEvent(1);
            System.out.println("    event ->\n" + ev.treeToString(""));

            EvioXMLDictionary dict = new EvioXMLDictionary(dictionary, null, true);
            System.out.println("\n    dictionary ->\n" + dict.toString());


            System.out.println("\n    Use StructureFinder class to search event:");

            System.out.println("\n      find structs w/ dict entry = JUNK");
            List<BaseStructure> vec =  StructureFinder.getMatchingStructures(ev, "JUNK", dict);
            for (BaseStructure bs : vec) {
                System.out.println("        found -> " + bs.toString());
            }
            vec.clear();

            vec = StructureFinder.getMatchingStructures(ev, "SEG5", dict);
            System.out.println("      find SEG5 -> ");
            for (BaseStructure bs : vec) {
                System.out.println("        found -> " + bs.toString());
            }
            System.out.println("\n");
            vec.clear();

            //<bank name="TopLevel"   tag="1"  num="1" type="bank" >
            //  <bank name="2Level"   tag="201-203"    type="bank" >
            vec = StructureFinder.getMatchingStructures(ev, "Top.2ndLevel", dict);
            System.out.println("      find Top.2ndLevel -> ");
            for (BaseStructure bs : vec) {
                System.out.println("        found -> " + bs.toString());
            }
            System.out.println("\n");
            vec.clear();

            //  <leaf name="TagSegUints"   tag="17" /> <<
            System.out.println("      find Top.2ndLevel.TagSegUints -> ");
            vec = StructureFinder.getMatchingStructures(ev, "Top.2ndLevel.TagSegUints", dict);
            for (BaseStructure bs : vec) {
                System.out.println("        found -> " + bs.toString());
            }
            System.out.println("\n");
            vec.clear();

            System.out.println("      find structs whose parent is Top.2ndLevel -> ");
            vec = StructureFinder.getMatchingParent(ev, "Top.2ndLevel", dict);
            for (BaseStructure bs : vec) {
                System.out.println("        found child -> " + bs.toString());
            }
            System.out.println("\n");
            vec.clear();

            System.out.println("      find structs whose child is Top.2ndLevel.InTagSeg -> ");
            vec = StructureFinder.getMatchingChild(ev, "Top.2ndLevel.InTagSeg", dict);
            for (BaseStructure bs : vec) {
                System.out.println("        found parent -> " + bs.toString());
            }
            System.out.println("\n");
            vec.clear();




            System.out.println("    find tag & num = 101:");
            DataType type = dict.getType(101, 101);
            if (type == null) {
                System.out.println("      has no type");
            }
            else {
                System.out.println("      data type = " + type.toString());
            }


            System.out.println("\n    find Top.2ndLevel.BankUints");
            //std::string entry = "Top.2ndLevel";
            String entry = "Top.2ndLevel.BankUints";
            //std::string entry = "Top.2ndLevel.SegInts";

            if (dict.exists(entry)) {
                int tag1, tagEnd1; int num1;
                // This is a tag range
                if (dict.isTagRange(entry)) {
                    Integer[] ia = dict.getTagNum(entry);
                    System.out.println("      tag range of 2nd Level Banks = " + ia[0] + " - " + ia[2]);
                }
                else if (dict.isTagNum(entry)) {
                    Integer[] ia = dict.getTagNum(entry);
                    System.out.println("      tag & num of 2nd Level Banks = tag " + ia[0] + ", num " + ia[1]);
                }
                else if (dict.isTagOnly(entry)) {
                    Integer[] ia = dict.getTagNum(entry);
                    System.out.println("      tag of 2nd Level Banks = " + ia[0] );
                }
                else {
                    System.out.println("      internal error finding tag/tagEnd/num");
                }
            }
            else {
                System.out.println("      no tag for "+ entry);
            }


            System.out.println("\n    find Tag 5:");
            Integer tagg = dict.getTag("Tag 5");
            if (tagg != null) {
                System.out.println("      tag = " + tag);
            }
            else {
                System.out.println("      no dict entry" );
            }


            // Retrieve & print info from dictionary
            entry = "CompositeData";
            System.out.println("\n    Getting stuff for name = \"CompositeData\":");
            tag = dict.getTag(entry);
            System.out.println("      tag = " + tag);
            Integer num_ = dict.getNum("CompositeData"); // check for null
            System.out.println("      num = " + num_);
            System.out.println("      type = " + dict.getType(entry).toString());
            System.out.println("      format = " + dict.getFormat(entry));
            System.out.println("      description = " + dict.getDescription(entry));

            System.out.println("\n    Getting stuff for tag = 8, num = 8:");
            System.out.println("      type  = " + dict.getType(8, 8).toString());
            System.out.println("      name = " + dict.getName(8,8));
            System.out.println("      format = " + dict.getFormat(8,8));
            System.out.println("      description = " + dict.getDescription(8,8));

            // first -> name, second -> sharedptr to entry
            System.out.println("\n\n    Print out contents of dictionary:");;
            Map<String, EvioDictionaryEntry> map = dict.getMap();
            for (Map.Entry<String, EvioDictionaryEntry> entry1 : map.entrySet()) {
                String key = entry1.getKey();
                EvioDictionaryEntry value = entry1.getValue();
                System.out.println("   " + key + " :   " + value.toString());
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /** For WRITING a local file. */
    public void testDict2() {

        try {
            int tag = 1;
            int num = 1;


            buffer.order(ByteOrder.nativeOrder());
            ByteBuffer buf = createCompactEventBuffer(tag, num, ByteOrder.nativeOrder(), 200000, null);
            EventWriter writer = new EventWriter(buffer);
            writer.writeEvent(buf);
            writer.close();

            EvioCompactReader reader = new EvioCompactReader(writer.getByteBuffer());
            System.out.println("\n\n  The EvioCompactReader has a limited ability to search an event for a specific tag & num:");


            System.out.println("\n    In buffer, find EvioNode w/ tag=201/num=201 -> ");
            List<EvioNode> returnList = reader.searchEvent(1, 201, 201);
            if (returnList.size() < 1) {
                System.out.println("      got nothing for ev 1");
            }
            else {
                for (EvioNode node : returnList) {
                    System.out.println("      found -> " + node);
                }
            }


            EvioXMLDictionary dict = new EvioXMLDictionary(dictionary, null, true);
            System.out.println("    In buffer, find EvioNode for entry = Top.2ndLevel.BankUints -> ");

            try {
                returnList = reader.searchEvent(1, "Top.2ndLevel.BankUints", dict);
                if (returnList.size() < 1) {
                    System.out.println("      got nothing for ev 1");
                }
                else {
                    for (EvioNode node : returnList) {
                        System.out.println("      found -> " + node);
                    }
                }
            }
            catch (EvioException e) {
                System.out.println("      no such dictionary entry or entry did not specify a single tag and single num");
            }

        }
        catch (Exception e) {
            e.printStackTrace();
            System.exit(-1);
        }
    }



}
