package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.util.Map;
import java.util.Set;

/**
 * Created by IntelliJ IDEA.
 * User: timmer
 * Date: 4/12/11
 * Time: 10:21 AM
 * To change this template use File | Settings | File Templates.
 */
public class DictTest {

    static String xmlDict1 =
            "<xmlDict>"  +
                    "<xmldumpDictEntry name='a' tag= '1'   num = '1' />"  +
                    "<xmldumpDictEntry name='b' tag= '1.2' num = '1.2' />"  +
            "</xmlDict>";

    static String xmlDict2 =
            "<xmlDict>"  +
                    "<xmldumpDictEntry name='a' tag= '1'   num = '1' />"  +
                    "<xmldumpDictEntry name='b'    num = '1.3' />"  +
            "</xmlDict>";

    static String xmlDict3 =
            "<xmlDict>"  +
                    "<xmldumpDictEntry name='a' tag= '1'   num = '1' />"  +
                    "<xmldumpDictEntry name='b' tag= '1.2' />"  +
            "</xmlDict>";

    static String xmlDict4 =
        "<JUNK>"  +
            "<moreJunk/>"  +

            "<xmlDict attr='junk'>"  +
                "<leaf name='leaf21' tag= '2.1' num = '2.1' />"  +
                "<leaf name='leaf2'  tag= '2'   num = '2' />"  +
                "<leaf name='leaf3'  tag= '2'   num = '2' />"  +

                "<dictEntry name='pretty-print'  tag= '456' />"  +
                "<dictEntry name='first'  tag= '123'   num = '123' />"  +
                "<dictEntry name='second'  tag= '123'   num = '123' />"  +
                "<dictEntry name='a' tag= '1.7'   num = '1.8' />"  +

                "<bank name='b1' tag= '10' num='0' attr ='gobbledy gook' >"  +
                    "<bank name='b2' tag= '20' num='20' >"  +
                        "<leaf name='l1' tag= '30' num='31'>"  +
                            "<bank name='lowest' tag= '111' num='222' />"  +
                        "</leaf>" +
                        "<leaf name='l2' tag= '31' num='32' />"  +
                    "</bank>"  +
                "</bank>"  +

            "</xmlDict>" +

            "<xmlDict>"  +
                "<leaf name='leaf21' tag= '3.1' num = '3.1' />"  +
                "<leaf name='a'  tag= '33'   num = '44' />"  +
            "</xmlDict>" +

        "</JUNK>"
            ;

    static String description =
            "\n" +
            "     i  TDC some comment\n" +
            "     F  ADC blah min=5\n" +
            "     N  multiplier\n";

    static String xmlDict5 =
            "<xmlDict>"  +
                "<dictEntry name='first'  tag='123'   num ='456' type='ComPosiTe' >" +
                    "<description format='FD2i' >"  +
                        description  +
                    "</description>" +
                "</dictEntry>"  +
                "<bank name='b1'   tag='10'   num='0' type='inT32' >"  +
                    "<description format='2(N3F)' >"  +
                        "this is a bank of signed 32 bit integers"  +
                    "</description>" +
                "</bank>"  +
            "</xmlDict>"
            ;


    static String xmlDict7 =
            "<xmlDict>\n" +
                    "  <bank name=\"HallD\"          tag=\"6-8\" type=\"booger\" >\n" +
//                    "  <bank name=\"HallD\"          tag=\"6-8\"  >\n" +
                    "      <description format=\"blah\" >"  +
                    "          hall_d_tag_range"  +
                    "      </description>" +
                    "      <bank name=\"TAG7\"       tag=\"7\"  />\n" +
                    "      <bank name=\"DC(%t)\"     tag=\"6\" num=\"0\" description=\"dc(typ)(num)\" >\n" +
                    "          <leaf name=\"xpos(%n)\"   tag=\"6\" num=\"1\" />\n" +
                    "          <bank name=\"ypos(%n)\"   tag=\"8\" num=\"2\" />\n" +
                    "          <bank name=\"zpos(%n)\"   tag=\"6\" num=\"3\" />\n" +
                    "          <bank name=\"zpos(%n)\"   tag=\"6\" num=\"4\" />\n" +
                    "      </bank >\n" +
                    "      <bank name=\"TOF\"        tag=\"8\" num=\"0\"  type=\"blah\" >\n" +
                    "          <leaf name=\"xpos\"   tag=\"8\" num=\"1\" />\n" +
                    "          <bank name=\"ypos\"   tag=\"8\" num=\"2\" />\n" +
                    "      </bank >\n" +
                    "  </bank >\n" +
                    "  <dictEntry name=\"TaggiesOnly\" tag=\"5\" >\n" +
                    "       <description format=\"My Format\" >tag 5 description</description>" +
                    "  </dictEntry>\n" +
                    "  <dictEntry name=\"Rangy_Small\" tag=\"75 - 76\"  />\n" +
                    "  <dictEntry name=\"Rangy\"       tag=\"75 - 78\"  />\n" +
                    "  <dictEntry name=\"TAG1\"        tag=\"1\" />\n" +
//                    "  <dictEntry name=\"TAG7\"        tag=\"7\"  />\n" +
//                    "  <dictEntry name=\"TAG7\"        tag=\"77\"  />\n" +
                    "  <dictEntry name=\"num=(%t-%n)\"    tag=\"123\" num=\"1-7\" />\n" +
//                    "  <dictEntry name=\"num=(7)\"     tag=\"123\" num=\"7\" />\n" +
                    "  <dictEntry name=\"num=(%n)\"     tag=\"123\" />\n" +
            "</xmlDict>"
            ;


    public static void main(String args[]) {

        EvioXMLDictionary dict = new EvioXMLDictionary(xmlDict7);

        Map<String, EvioDictionaryEntry> map = dict.getMap();
        Set<String> keys = map.keySet();
        for (String key : keys) {
            System.out.println("key = " + key + ", tag = " + dict.getTag(key) +
                               ", tagEnd = " + dict.getTagEnd(key) +
                                       ", num = " + dict.getNum(key));
        }
        System.out.println();

        Integer tag = 8;
        Integer num = 2;
        Integer tagEnd = 0;

        String n = dict.getName(tag, num, tagEnd);
        System.out.println("getName(tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd + ") = " + n);

        EvioDictionaryEntry e = new EvioDictionaryEntry(tag, num, tagEnd, null);
        String name = dict.tagNumMap.get(e);

        System.out.println("1 name = " + name + ", for tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd);

        EvioDictionaryEntry pEntry = new EvioDictionaryEntry(6, 0, 0, null);
        EvioDictionaryEntry newEntry = new EvioDictionaryEntry(tag, num, tagEnd, null, null, null, pEntry);
        name = dict.tagNumMap.get(newEntry);

        System.out.println("2 name = " + name + ", for tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd + " and parent = 6/0/0");




        Integer pTag = 8;
        Integer pNum = 0;
        Integer pTagEnd = 0;
        String nm = dict.getName(tag, num, tagEnd, pTag, pNum, pTagEnd);
        System.out.println("name for tag = " + tag + ", num = " + num +
                           ", tagEnd = " + tagEnd + ", pTag = " + pTag +
                                   ", pNum = " + pNum + ", pTagEnd = " + pTagEnd +
                                   " IS " + nm);


//        System.out.println("description for tag = " + tag + ", num = " + num +
//                           ", tagEnd = " + tagEnd + " IS \"" +
//                           dict.getDescription(tag, num, tagEnd) + "\"");
//        System.out.println("format for tag = " + tag + ", num = " + num +
//                           ", tagEnd = " + tagEnd + " IS \"" +
//                           dict.getFormat(tag, num, tagEnd) + "\"");

        System.out.println();

        int i=0;
        Set<Map.Entry<String, EvioDictionaryEntry>> set = map.entrySet();
        for (Map.Entry<String, EvioDictionaryEntry> entry : set) {
            String entryName =  entry.getKey();
            EvioDictionaryEntry entryData = entry.getValue();
            System.out.println("entry " + (++i) + ": name = " + entryName + ", tag = " +
                                       entryData.getTag() + ", num = " + entryData.getNum());
        }

        System.out.println();
        System.out.println("Dictionary.toString() gives:\n" + dict.toString());
    }

    public static void main1(String args[]) {

        EvioXMLDictionary dict = new EvioXMLDictionary(xmlDict5);

        System.out.println("Getting stuff for tag = 123, num = 456:");
        System.out.println("    type        = " + dict.getType(123,456));
        System.out.println("    name        = " + dict.getName(123,456));
        System.out.println("    format      = " + dict.getFormat(123,456));
        System.out.println("    description = " + dict.getDescription(123,456));


        System.out.println("Getting stuff for name = \"first\":");
        System.out.println("    tag         = " + dict.getTag("first"));
        System.out.println("    num         = " + dict.getNum("first"));
        System.out.println("    type        = " + dict.getType("first"));
        System.out.println("    format      = " + dict.getFormat("first"));
        System.out.println("    description = " + dict.getDescription("first"));

        System.out.println("Getting stuff for tag = 10, num = 0:");
        System.out.println("    type        = " + dict.getType(10,0));
        System.out.println("    name        = " + dict.getName(10,0));
        System.out.println("    format      = " + dict.getFormat(10,0));
        System.out.println("    description = " + dict.getDescription(10,0));


        System.out.println("Getting stuff for name = \"b1\":");
        System.out.println("    tag         = " + dict.getTag("b1"));
        System.out.println("    num         = " + dict.getNum("b1"));
        System.out.println("    type        = " + dict.getType("b1"));
        System.out.println("    format      = " + dict.getFormat("b1"));
        System.out.println("    description = " + dict.getDescription("b1"));


    }


    public static void main6(String args[]) {

        EvioXMLDictionary dict = new EvioXMLDictionary(xmlDict4);
        Map<String, EvioDictionaryEntry> map = dict.getMap();

        Set<String> keys = map.keySet();
        for (String key : keys) {
            System.out.println("key = " + key + ", tag = " + dict.getTag(key) + ", num = " + dict.getTag(key));
        }

        int i=0;
        Set<Map.Entry<String, EvioDictionaryEntry>> set = map.entrySet();
        for (Map.Entry<String, EvioDictionaryEntry> entry : set) {
            String entryName =  entry.getKey();
            EvioDictionaryEntry entryData = entry.getValue();
            System.out.println("entry " + (++i) + ": name = " + entryName + ", tag = " +
                                       entryData.getTag() + ", num = " + entryData.getTag());
        }


        EvioEvent bank20 = new EvioEvent(456, DataType.BANK, 20);

        String dictName = dict.getName(bank20);
        System.out.println("Bank-20 corresponds to dictionary entry, \"" + dictName + "\"");

    }

    public static void main2(String args[]) {

        EvioXMLDictionary dict = new EvioXMLDictionary(xmlDict4);

        EvioEvent bank11 = new EvioEvent(10, DataType.BANK, 10);

        String dictName = dict.getName(bank11);
        System.out.println("Bank-11 corresponds to dictionary entry, \"" + dictName + "\"");

        EventBuilder builder = new EventBuilder(bank11);
        EvioBank bank12 = new EvioBank(20, DataType.SEGMENT, 20);
        try {
            builder.addChild(bank11, bank12);
        }
        catch (EvioException e) {}

        dictName = dict.getName(bank12);
        System.out.println("Bank-12 corresponds to dictionary entry, \"" + dictName + "\"");

        EvioSegment seg30 = new EvioSegment(1, DataType.INT32);
        try {
            builder.addChild(bank12, seg30);
        }
        catch (EvioException e) {}



        dictName = dict.getName(seg30);
        System.out.println("Seg-30 corresponds to dictionary entry, \"" + dictName + "\"");

        EvioEvent ev = new EvioEvent(10, DataType.INT32, 10);
        dictName = dict.getName(ev);
        System.out.println("Event-30 corresponds to dictionary entry, \"" + dictName + "\"");

        EvioSegment seg = new EvioSegment(10, DataType.INT32);
        dictName = dict.getName(seg);
        System.out.println("Seg-10 corresponds to dictionary entry, \"" + dictName + "\"");



        EvioBank bk2 = new EvioBank(2, DataType.INT32, 2);
        dictName = dict.getName(bk2);
        System.out.println("Bank-2 corresponds to dictionary entry, \"" + dictName + "\"");

        EvioBank bk22 = new EvioBank(2, DataType.INT32, 2);
        dictName = dict.getName(bk22);
        System.out.println("Bank-22 corresponds to dictionary entry, \"" + dictName + "\"");




        System.out.println("TEST NEW FEATURE:");
        Integer[] tn = dict.getTagNum("b1.b2.l1");
        if (tn != null) {
            System.out.println("Dict entry of b1.b2.l1 has tag = " + tn[0] +
                            " and num = " + tn[1]);
        }

        tn = dict.getTagNum("a");
        if (tn != null) {
            System.out.println("Dict entry of a has tag = " + tn[0] +
                            " and num = " + tn[1]);
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
        System.out.println("\nTEST NEW toString() method:");
        System.out.println(dict.toString());
    }


}
