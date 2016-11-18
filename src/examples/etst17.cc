//  example program reads and dumps EVIO buffers

//  ejw, 20-sep-2011
// Carl Timmer 27-oct-2016




#include "evioUtil.hxx"


using namespace evio;



//-------------------------------------------------------------------------------


int main(int argc, char **argv) {

    // note:  getTagNumMap is map<string,tagnum>

    try {

        string dictXML1 =
                "<dict>\n"
                        "<bank name=\"flintstone\" tag=\"1 -  5  \" >\n"
                        "  <leaf name=\"fred\" tag=\"1\" num=\"1\"/>\n"
                        "  <leaf name=\"wilma\" tag=\"1\" num=\"2\"/>\n"
                        "      <description junk=\"myJunk\" att=\"myAtt\" format=\"FORMAT_STRING\" >blah blah blah</description>\n"
                        "</bank>\n"
                        "<bank name=\"rubble\" tag=\"2\" num=\"0\">\n"
                        "  <leaf name=\"barney\" tag=\"2\" num=\"1\"/>\n"
                        "  <leaf name=\"betty\"  tag=\"2\" num=\"2\"/>\n"
                        "</bank>\n"
                        "</dict>\n";
        evioDictionary dict1(dictXML1);

        size_t size1 = dict1.getTagNumMap.size();
        cout << "dict1 size is " << size1 << endl;
        cout << dict1.toString() << endl;




        string dictXML2 =
                "<xmlDict>\n"
                        "  <bank name=\"HallD(%t)\"          tag=\"6-8\"  >\n"
                        "      <description format=\"range\" >tag 6-8</description>"
                        "      <bank name=\"DC(%t)\"     tag=\"6\" num=\"0\"  >\n"
                        "          <description format=\"tag/num\" >tag 6 num 0</description>"
                        "          <leaf name=\"xpos(%n)\"   tag=\"6\" num=\"1\" />\n"
                        "          <bank name=\"ypos(%n)\"   tag=\"6\" num=\"2\" />\n"
                        "          <bank name=\"zpos(%n)\"   tag=\"6\" num=\"3\" />\n"
                        "          <bank name=\"zpos(%n)\"   tag=\"8\" num=\"2\" />\n"
                        "      </bank >\n"
                        "      <bank name=\"TOF\"        tag=\"8\" num=\"0\" >\n"
                        "          <leaf name=\"xpos\"   tag=\"8\" num=\"1\" />\n"
                        "          <bank name=\"ypos\"   tag=\"8\" num=\"2\" />\n"
                        "      </bank >\n"
                        "  </bank >\n"
                        "  <dictEntry name=\"TaggiesOnly\" tag=\"5\" >\n"
//                "       <description format=\"My Format\" >tag 5 description</description>"
                        "  </dictEntry>\n"
                        "  <dictEntry name=\"Rangy_Small\" tag=\"75 - 76\"  />\n"
                        "  <dictEntry name=\"Rangy\"       tag=\"75 - 78\"  />\n"
                        "  <dictEntry name=\"TAG1\"        tag=\"1\" />\n"
                        "  <dictEntry name=\"TAG7\"        tag=\"7\"  />\n"
                        "  <bank name=\"TAG8ONLY\"        tag=\"8\"  >\n"
                        "       <description format=\"tag only\" >tag 8 only</description>"
                        "  </bank >\n"
                        //"  <dictEntry name=\"TAG7\"        tag=\"77\"  />\n"
                        "  <dictEntry name=\"num=(%t-%n)\"    tag=\"123\" num=\"1-7\" />\n"
                        //"  <dictEntry name=\"num=(7)\"     tag=\"123\" num=\"7\" />\n"
                        "  <dictEntry name=\"num=(%n)\"     tag=\"123\" />\n"
                        "</xmlDict>\n"
        ;

        evioDictionary dict2(dictXML2);

        size_t size2 = dict2.getTagNumMap.size();
        cout << "dict2 size is " << size2 << endl;

        cout << dict2.toString() << endl;

        bool printMaps = true;

        if (printMaps) {
            cout << "getNameMap (everything <entry,string>: count = " << dict2.getNameMap.size() << endl;
            map<evioDictEntry, string>::const_iterator pos;
            for (pos = dict2.getNameMap.begin(); pos != dict2.getNameMap.end(); pos++) {
                cout << "key: " << pos->second;
                evioDictEntry entry = pos->first;

                cout << ", val: " << (int)entry.getTag() << "," << (int)entry.getNum() << "," << (int)entry.getTagEnd();
                string desc = entry.getDescription();
                if (!desc.empty()) {
                    cout << ", description: " << desc << ", format = " << entry.getFormat();
                }
                cout  << endl;
            }

            cout << endl << endl << "getTagNumMap (everything <string,entry>: count = " << dict2.getTagNumMap.size() << endl;
            map<string, evioDictEntry>::const_iterator pos2;
            for (pos2 = dict2.getTagNumMap.begin(); pos2 != dict2.getTagNumMap.end(); pos2++) {
                cout << "key: " << pos2->first;
                evioDictEntry entry = pos2->second;

                cout << "value: " << (int)entry.getTag() << "," << (int)entry.getNum() << "," << (int)entry.getTagEnd();
                string desc = entry.getDescription();
                if (!desc.empty()) {
                    cout << ", description: " << desc << ", format = " << entry.getFormat();
                }
                cout  << endl;
            }

// Make maps public for this test to work:
//
//            cout << endl << endl << "tagNumMap: count = " << dict2.tagNumMap.size() << endl;
//            for (pos = dict2.tagNumMap.begin(); pos != dict2.tagNumMap.end(); pos++) {
//                cout << "key:   " << pos->second;
//                evioDictEntry entry = pos->first;
//
//                cout << ", val: " << (int)entry.getTag() << "," << (int)entry.getNum() << "," << (int)entry.getTagEnd();
//                string desc = entry.getDescription();
//                if (!desc.empty()) {
//                    cout << ", description: " << desc << ", format = " << entry.getFormat();
//                }
//                cout  << endl;
//            }
//
//
//            cout << endl << endl << "TagNumReverseMap (everything <string,entry>: count = " << dict2.tagNumReverseMap.size() << endl;
//            for (pos2 = dict2.tagNumReverseMap.begin(); pos2 != dict2.tagNumReverseMap.end(); pos2++) {
//                cout << "key: " << pos2->first;
//                evioDictEntry entry = pos2->second;
//
//                cout << "value: " << (int)entry.getTag() << "," << (int)entry.getNum() << "," << (int)entry.getTagEnd();
//                string desc = entry.getDescription();
//                if (!desc.empty()) {
//                    cout << ", description: " << desc << ", format = " << entry.getFormat();
//                }
//                cout  << endl;
//            }
//
//
//            cout << endl << endl <<"tagOnlyMap: count = " << dict2.tagOnlyMap.size() << endl;
//            for (pos = dict2.tagOnlyMap.begin(); pos != dict2.tagOnlyMap.end(); pos++) {
//                cout << "key: " << pos->second;
//                evioDictEntry entry = pos->first;
//
//                cout << ", val: " << (int)entry.getTag() << "," << (int)entry.getNum() << "," << (int)entry.getTagEnd();
//                string desc = entry.getDescription();
//                if (!desc.empty()) {
//                    cout << ", description: " << desc << ", format = " << entry.getFormat();
//                }
//                cout  << endl;
//            }
//
//            cout << endl << endl <<"tagRangMap: count = " << dict2.tagRangeMap.size() << endl;
//            for (pos = dict2.tagRangeMap.begin(); pos != dict2.tagRangeMap.end(); pos++) {
//                cout << "key: " << pos->second;
//                evioDictEntry entry = pos->first;
//
//                cout << ", val: " << (int)entry.getTag() << "," << (int)entry.getNum() << "," << (int)entry.getTagEnd();
//                string desc = entry.getDescription();
//                if (!desc.empty()) {
//                    cout << ", description: " << desc << ", format = " << entry.getFormat();
//                }
//                cout  << endl;
//            }

            cout  << endl;
        }


        cout  << endl;
        cout << "getNameMap non-exact match:" << endl;
        evioDictEntry e = evioDictEntry(8, 2, 0);
        cout << "tag,num,tagEnd (8,2, 0) has name = " << dict2.getName(e) << endl;


        e = evioDictEntry(8, 2, 0, true, 6, 0, 0, EVIO_UNKNOWN32);
        cout << "tag,num,tagEnd (8,2,0, true, 6,0,0,bank) has name = " << dict2.getName(e) << endl;

        e = evioDictEntry(8, 2, 0, true, 8, 0, 0, EVIO_UNKNOWN32);
        cout << "tag,num,tagEnd (8,2,0, true, 8,0,0,bank) has name = " << dict2.getName(e) << endl;

        cout  << endl;
        cout  << endl;

        // check if something exists
        string name = dict2.getName(6, 0, 0);
        cout << "tag,num,tagEnd (6,0,0) has name = " << name << endl;

        name = dict2.getName(6, 5, 0);
        cout << "tag,num (6,5) has name = " << name << endl;

        name = dict2.getName(75, 12, 0);
        cout << "tag,num (75,12) has name = " << name << endl;

        name = dict2.getName(78, 0, 75);
        cout << "tag,num (78,0, 75) has name = " << name << endl;

        name = dict2.getName(8, 12, 0);
        cout << "tag,num (8,12) has name = " << name << endl;
        name = dict2.getName(8, 1, 0);
        cout << "tag,num (8,1) has name = " << name << endl;

        name = dict2.getName(7, 1, 0);
        cout << "tag,num (7,1) has name = " << name << endl;


    } catch (evioException e) {
        cerr << e.toString() << endl;

    } catch (...) {
        cerr << "?unknown exception" << endl;
    }

}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
