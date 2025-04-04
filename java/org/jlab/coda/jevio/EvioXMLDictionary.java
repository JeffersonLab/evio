/*
 * Copyright (c) 2015, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio;

import java.io.File;
import java.io.ByteArrayInputStream;
import java.io.StringWriter;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;



/**
 * This was developed to read the xml dictionary that Maurizio uses for GEMC.
 * It implements INameProvider, just like all other dictionary readers.<p>
 *
 * <b>An assumption in the following class is that each unique tag/num/tagEnd group
 * corresponds to an equally unique name. In other words, 2 different
 * groups cannot have the same name. And 2 different names cannot map to the
 * same group.</b><p>
 *
 * An entry with only a tag value and no num is allowed. It will match
 * a tag/num pair if no exact match exists but the tag matches. For such an
 * entry, no additional existence of type, format, or description is allowed.<p>
 *
 * Similarly, an entry with a range of tags is also allowed. In this case,
 * no num &amp; type is allowed. It will match
 * a tag/num pair if no exact match exists but the tag is in the range
 * (inclusive).
 * 
 * @author heddle
 * @author timmer
 */
@SuppressWarnings("serial")
public class EvioXMLDictionary implements INameProvider {

    /** Element containing entire dictionary. */
    private static String DICT_TOP_LEVEL = "xmlDict";

    /** There is only one type of element which directly defines an entry (strange name). */
    private static String ENTRY = "xmldumpDictEntry";

    /**
     * New, alternate, shortened form of ENTRY.
     * @since 4.0
     */
    private static String ENTRY_ALT = "dictEntry";

    /**
     * Hierarchical container element.
     * @since 4.0
     */
    private static String ENTRY_BANK = "bank";

    /**
     * Hierarchical leaf element.
     * @since 4.0
     */
    private static String ENTRY_LEAF = "leaf";

    /**
     * Description element.
     * @since 4.0
     */
    private static String DESCRIPTION = "description";

    /**
     * The "format" attribute string.
     * @since 4.0
     */
    private static String FORMAT = "format";

    /**
     * The "type" attribute string.
     * @since 4.0
     */
    private static String TYPE = "type";

	/** The "name" attribute string. */
	private static String NAME = "name";

	/** The "tag" attribute string. */
	private static String TAG = "tag";

    /** The "num" attribute string. */
    private static String NUM = "num";

    /**
     * Use regular expressions to parse a tag since it may be of the form:
     * tag="num" or tag="num1 - num2".
     * Allows for int - int pattern with whitespace before and after ints, but nothing else.
     * @since 5.2
     */
    private static Pattern pattern = Pattern.compile("^\\s*(\\d+)\\s*(-\\s*(\\d+))?\\s*$");


    /**
     * The character used to separate hierarchical parts of names.
     * @since 4.0
     */
    private String delimiter = ".";

    /**
     * This is the heart of the dictionary in which a key is composed of a tag/num
     * pair and other entry data and its corresponding value is a name.
     * Using a hashmap ensures entries are unique.
     * @since 4.0
     */
    public LinkedHashMap<EvioDictionaryEntry,String> tagNumMap = new LinkedHashMap<>(100);

    /**
     * Some dictionary entries have only a tag and no num.
     * It matches a tag/num pair if there is no exact match in tagNumMap,
     * but does match a tag in this map.
     * @since 4.1
     */
    public LinkedHashMap<EvioDictionaryEntry,String> tagOnlyMap = new LinkedHashMap<>(20);

    /**
     * Some dictionary entries have only a tag range and no num.
     * It matches a tag/num pair if there is no exact match in tagNumMap
     * or in the tagOnlyMap but the tag is within the specified range of an entry.
     * @since 5.2
     */
    public LinkedHashMap<EvioDictionaryEntry,String> tagRangeMap = new LinkedHashMap<>(20);

    /**
     * This is a hashmap in which the key is a name and the value is its
     * corresponding dictionary entry. This map contains all entries whether
     * tag/num, tag-only, or tag-range.
     * @since 5.2
     */
    private LinkedHashMap<String,EvioDictionaryEntry> reverseMap = new LinkedHashMap<>(100);

    /**
     * This is a hashmap in which the key is a name and the value is the entry
     * of a corresponding tag/num pair. It's the reverse of the tagNumMap map.
     * @since 4.0
     */
    private LinkedHashMap<String,EvioDictionaryEntry> tagNumReverseMap = new LinkedHashMap<>(100);

    /**
     * Top level xml Node object of xml DOM representation of dictionary.
     * @since 4.0
     */
    private Node topLevelDoc;

    /**
     * Keep a copy of the string representation around so toString() only does hard work once.
     * @since 4.1
     */
    private String stringRepresentation;

    /**
     * Keep a copy of the XML string around so toXML() only does hard work once.
     * @since 4.1
     */
    private String xmlRepresentation;



    /**
     * Create an EvioXMLDictionary from an xml file.
     *
     * @param file file containing xml.
     */
    public EvioXMLDictionary(File file) {
        this(file, null, false);
    }

    /**
     * Create an EvioXMLDictionary from an xml file.
     *
     * @param file file containing xml.
     * @param delimiter character used to separate hierarchical parts of names
     *                  where null defaults to period(.)
     */
    public EvioXMLDictionary(File file, String delimiter) {
        this(file, delimiter, false);
    }

    /**
     * Create an EvioXMLDictionary from an xml file.
     *
     * @param file file containing xml.
     * @param delimiter character used to separate hierarchical parts of names
     *                  where null defaults to period(.)
     * @param debug     if true, print out debug messages.
     */
    public EvioXMLDictionary(File file, String delimiter, boolean debug) {
        this(getDomObject(file), delimiter, debug);
    }


    /**
     * Create an EvioXMLDictionary from an xml string.
     *
     * @param xmlString string containing xml.
     */
    public EvioXMLDictionary(String xmlString) {
        this(xmlString, null, false);
    }

    /**
     * Create an EvioXMLDictionary from an xml string.
     *
     * @param xmlString string containing xml.
     * @param delimiter character used to separate hierarchical parts of names
     *                  where null defaults to period(.)
     */
    public EvioXMLDictionary(String xmlString, String delimiter) {
        this(xmlString, delimiter, false);
    }

    /**
     * Create an EvioXMLDictionary from an xml string.
     *
     * @param xmlString string containing xml.
     * @param delimiter character used to separate hierarchical parts of names
     *                  where null defaults to period(.)
     * @param debug     if true, print out debug messages.
     */
    public EvioXMLDictionary(String xmlString, String delimiter, boolean debug) {
        this(getDomObject(xmlString), delimiter, debug);
    }

    /**
     * Create an EvioXMLDictionary from an xml Document object.
	 *
	 * @param domDocument DOM object representing xml dictionary.
	 */
	public EvioXMLDictionary(Document domDocument) {
        this(domDocument, null, false);
    }

    /**
     * Create an EvioXMLDictionary from an xml Document object.
     *
     * @param domDocument DOM object representing xml dictionary.
     * @param delimiter character used to separate hierarchical parts of names
     *                  where null defaults to period(.)
     */
    public EvioXMLDictionary(Document domDocument, String delimiter) {
        this(domDocument, delimiter, false);
    }

    /**
     * Create an EvioXMLDictionary from an xml Document object.
     *
     * @param domDocument DOM object representing xml dictionary.
     * @param delimiter character used to separate hierarchical parts of names
     *                  where null defaults to period(.)
     * @param warn     if true, print out warning debug messages.
     */
    public EvioXMLDictionary(Document domDocument, String delimiter, boolean warn) {

        if (domDocument == null) return;
        if (delimiter   != null) this.delimiter = delimiter;

        // Find top level of dictionary inside xml file being parsed (case sensitive!)
        NodeList list = domDocument.getElementsByTagName(DICT_TOP_LEVEL);

        // If no dictionary, just return
        if (list.getLength() < 1) return;

        // Otherwise, pick the first one
        Node topNode = list.item(0);
        topLevelDoc = topNode;

        // Look at all the children, if any
        NodeList kidList = topNode.getChildNodes();
        if (kidList.getLength() < 1) return;

        boolean debug = false;
        Integer tag, tagEnd, num, numEnd;
        boolean badTagEntry, badNumEntry, badTagEndEntry, badNameEntry, badTypeEntry;
        boolean isTagRange, isNumRange;
        String name, tagStr, tagEndStr, numStr, numEndStr, typeStr, format, description;

        // Pick out elements that are both old & new direct entry elements
        for (int index = 0; index < kidList.getLength(); index++) {
            Node node = kidList.item(index);
            if (node == null) continue;

            // Only looking for "xmldumpDictEntry" and "dictEntry" nodes
            if (!node.getNodeName().equalsIgnoreCase(ENTRY) &&
                !node.getNodeName().equalsIgnoreCase(ENTRY_ALT)) {
if (debug)  System.out.println("dictionary: rejecting node, " + node.getNodeName());
                continue;
            }

            tag = tagEnd = num = numEnd = null;
            badTagEntry = badNumEntry = badTagEndEntry = badNameEntry = badTypeEntry = false;
            isTagRange = isNumRange = false;
            name = numStr = tagStr = typeStr = format = description = null;
            DataType type = DataType.UNKNOWN32;

            if (node.hasAttributes()) {
                NamedNodeMap map = node.getAttributes();

                // Get the name
                Node attrNode = map.getNamedItem(NAME);
                if (attrNode != null) {
                    name = attrNode.getNodeValue();
                }

                // Check to see if name conflicts with strings set aside to
                // describe evio as xml. There can be substitutions in the name
                // such as %n for num and %t for tag which will be done later,
                // but these will not affect the following check for conflicts
                // with reserved names.
                if (Utilities.getDataType(name) != null ||
                    name.equalsIgnoreCase("event") ||
                    name.equalsIgnoreCase("evio-data")) {
if (warn) System.out.println("dictionary: ignore entry whose name conflicts with reserved strings: " + name);
                    continue;
                }

                // Get the num or num range as the case may be
                attrNode = map.getNamedItem(NUM);
                if (attrNode != null) {
                    // Use regular expressions to parse num
                    Matcher matcher = pattern.matcher(attrNode.getNodeValue());

                    if (matcher.matches()) {
                        // First num may be null, is always >= 0 if existing.
                        // Since we're here, it does exist.
                        numStr = matcher.group(1);
                        try {
                            num = Integer.decode(numStr);
                            if (num > 255) {
if (warn) System.out.println("dictionary: num = " + num + " is too large must be < 256, for " + name);
                                badNumEntry = true;
                            }
                        }
                        catch (NumberFormatException e) {
                            // Since it matches pattern, we should never get here
                            badNumEntry = true;
                        }
if (debug) System.out.println("dictionary: num = " + num);

                        // Ending num
                        if (!badNumEntry) {
                            numEndStr = matcher.group(3);
                            if (numEndStr != null) {
                                try {
                                    numEnd = Integer.decode(numEndStr);
                                    if (numEnd > 255) {
                                        if (warn) System.out.println("dictionary: numEnd = " + numEnd + " is too large must be < 256, for " + name);
                                        badNumEntry = true;
                                    }

                                    // The regexp matching only allows values >= 0 for tagEnd.
                                    // When tagEnd == 0 or tag == tagEnd, no range is defined.
                                    else if (numEnd > 0 && !numEnd.equals(num)) {
                                        isNumRange = true;

                                        // Since a num range is defined, the name MUST contain at least one %n
                                        if (!name.contains("%n")) {
                                            badNameEntry = true;
                                            if (debug)
                                                System.out.println("dictionary: num range defined so name must contain at least one %n, but = " + name);
                                        }
                                    }
                                }
                                catch (NumberFormatException e) {
                                    // Since it matches pattern, we should never get here
                                    badNumEntry = true;
                                }
                                if (debug) System.out.println("dictionary: numEnd = " + numEnd);
                            }
                            else {
                                // Set for later convenience in for loop
                                numEnd = num;
                            }
                        }

                    }
                    else {
                        badNumEntry = true;
if (debug) System.out.println("dictionary: num must be a valid non-negative integer or range, so ignore entry for " + name);
                    }
                }

                // If no num defined, substitute "" for each %n
                if (numStr == null) {
                    name = name.replaceAll("%n", "");
                }

                // Get the tag or tag range as the case may be
                attrNode = map.getNamedItem(TAG);
                if (attrNode != null) {
                    // Use regular expressions to parse tag
                    Matcher matcher = pattern.matcher(attrNode.getNodeValue());

                    if (matcher.matches()) {
                        // First tag, never null, always >= 0, or no match occurs
                        tagStr = matcher.group(1);
                        try {
                            tag = Integer.decode(tagStr);
                            if (tag > 65535) {
if (warn) System.out.println("dictionary: tag = " + tag + " is too large must be < 65536, for " + name);
                                badTagEntry = true;
                            }
                            //System.out.println("Tag, dec = " + tag);
                        }
                        catch (NumberFormatException e) {
                            // Since it matches pattern, we should never get here
                            badTagEntry = true;
                        }

                        // Ending tag
                        if (!badTagEntry) {
                            tagEndStr = matcher.group(3);
                            if (tagEndStr != null) {
                                try {
                                    tagEnd = Integer.decode(tagEndStr);
                                    if (tagEnd > 65535) {
if (warn) System.out.println("dictionary: tagEnd = " + tagEnd + " is too large must be < 65536, for " + name);
                                        badTagEndEntry = true;
                                    }
                                    // The regexp matching only allows values >= 0 for tagEnd.
                                    // When tagEnd == 0 or tag == tagEnd, no range is defined.
                                    else if (tagEnd > 0 && !tagEnd.equals(tag)) {
                                        isTagRange = true;
                                        //System.out.println("Tag end, dec = " + tagEnd);
                                    }
                                }
                                catch (NumberFormatException e) {
                                    // Since it matches pattern, we should never get here
                                    badTagEndEntry = true;
                                }
                            }
                        }

                    }
                    else {
                        badTagEntry = true;
if (debug) System.out.println("dictionary: tag must be a valid non-negative integer or range, so ignore entry for " + name);
                    }

                    if (debug) {
                        System.out.println("dictionary: tag =  " + tag);
                        System.out.println("dictionary: tagEnd =  " + tagEnd);
                    }
                }

                // Scan name for the string "%t".
                // If the tag exists, substitute it for the %t.
                // If tag range defined, substitute "".
                if (isTagRange) {
                    if (num != null) {
                        // Cannot define num (or num range) and tag range at the same time ...
                        badNumEntry = true;
if (debug) System.out.println("dictionary: cannot define num (or num range) and tag range simultaneously for " + name);
                    }
                    else {
                        name = name.replaceAll("%t", "");
                    }
                }
                else {
                    name = name.replaceAll("%t", tagStr);
                }

                // Get the type, if any
                attrNode = map.getNamedItem(TYPE);
                if (attrNode != null) {
                    typeStr = attrNode.getNodeValue();
                    typeStr = typeStr.toUpperCase();
                    try {
                        type = DataType.valueOf(typeStr);
                    }
                    catch (Exception e) {
                        badTypeEntry = true;
                        if (warn) System.out.println("dictionary: ignore invalid type (" + typeStr + ") for " + name);
                    }
                }

                // Look for description node (xml element) as child of entry node
                NodeList children = node.getChildNodes();
                if (children.getLength() > 0) {

                    // Pick out first description element only
                    for (int i = 0; i < children.getLength(); i++) {
                        Node childNode = children.item(i);
                        if (childNode == null) continue;

                        // Only looking for "description" node
                        if (!childNode.getNodeName().equalsIgnoreCase(DESCRIPTION)) {
                            continue;
                        }

                        description = childNode.getTextContent();
//System.out.println("dictionary: found description = " + description);

                        // See if there's a format attribute
                        if (childNode.hasAttributes()) {
                            map = childNode.getAttributes();

                            // Get the format
                            attrNode = map.getNamedItem(FORMAT);
                            if (attrNode != null) {
                                format = attrNode.getNodeValue();
//System.out.println("dictionary: found format = " + format);
                            }
                        }
                        break;
                    }
                }

                // Skip invalid entries
                if (name == null || badNameEntry) {
if (warn) System.out.println("dictionary: ignore name that is empty or does not contain \"%n\" for num range, name = " + name);
                    continue;
                }
                else if (tagStr == null || badTagEntry || badTagEndEntry) {
if (warn) System.out.println("dictionary: ignore empty or invalid tag or tagEnd for name = " + name);
                    continue;
                }
                else if (badNumEntry) {
if (warn) System.out.println("dictionary: ignore invalid num/num-range entry for name = " + name);
                    continue;
                }

                if (badTypeEntry) {
if (warn) System.out.println("dictionary: ignore invalid type (" + typeStr + ") for " + name + ", must be valid evio type, num not defined?");
                    typeStr = null;
                }


                // If the num or num range is defined ...
                if (numStr != null) {
                    // Make sure num < numEnd
                    if (isNumRange && (num > numEnd)) {
                        int tmp = num;
                        num = numEnd;
                        numEnd = tmp;
                    }
if (debug) System.out.println("dictionary: num or num range is DEFINED => num = " + num + ", numEnd = " + numEnd);

                    String nameOrig = name;

                    // Range of nums (num == numEnd for no range)
                    for (int n = num; n <= numEnd; n++) {
                        // Scan name for the string "%n" and substitute num for it
                        name = nameOrig.replaceAll("%n", n + "");

                        EvioDictionaryEntry key = new EvioDictionaryEntry(tag, n, tagEnd, type.toString(),
                                                                          description, format);
                        boolean entryAlreadyExists = true;

                        if (reverseMap.containsKey(name)) {
if (warn) System.out.println("dictionary: 1 ignore duplicate dictionary entry for " + name);
                        }
                        else {
                            // Only add to dictionary if both name and tag/num pair are unique
                            // This works due to overloaded equals method.
                            if (!tagNumMap.containsKey(key) && !tagNumReverseMap.containsKey(name)) {
                                 tagNumMap.put(key, name);
                                 tagNumReverseMap.put(name, key);
                                 entryAlreadyExists = false;
                            }
                            else {
if (warn) System.out.println("dictionary: 2 ignore duplicate dictionary entry for " + name);
                            }

                            if (!entryAlreadyExists) {
                                reverseMap.put(name, key);
                            }
                        }
                    }
                }
                // If no num defined ...
                else {
//System.out.println("dictionary: make dictEntry name = " + name + " with description = " + description + ", format = " + format);
                    EvioDictionaryEntry key = new EvioDictionaryEntry(tag, null, tagEnd,
                                                                     type.toString(), description, format);
                    boolean entryAlreadyExists = true;

                    if (reverseMap.containsKey(name)) {
if (warn) System.out.println("dictionary: 3 ignore duplicate dictionary entry for " + name);
                    }
                    else {
                        if (isTagRange) {
                            if (!tagRangeMap.containsKey(key)) {
                                tagRangeMap.put(key, name);
                                entryAlreadyExists = false;
                            }
                            else {
if (warn) System.out.println("dictionary: 4 ignore duplicate dictionary entry for " + name);
                            }
                        }
                        else {
                            if (!tagOnlyMap.containsKey(key)) {
                                tagOnlyMap.put(key, name);
                                entryAlreadyExists = false;
                            }
                            else if (warn) {
System.out.println("dictionary: 5 ignore duplicate dictionary entry for " + name);
                            }
                        }

                        if (!entryAlreadyExists) {
                            reverseMap.put(name, key);
                        }
                    }
                }
            }
        }

        // Look at the (new) hierarchical entry elements,
        // recursively, and add all existing entries.
        addHierarchicalDictEntries(kidList, null, warn);

	} // end Constructor


    /**
     * Get the number of entries in this dictionary.
     * @return number of entries in this dictionary.
     */
    public int size() {
        return tagNumMap.size();
    }


    /**
     * Get the map in which the key is the entry name and the value is an object
     * containing its data (tag, num, type, etc.).
     * @return  map in which the key is the entry name and the value is an object
     *          containing its data (tag, num, type, etc.).
     */
    public Map<String, EvioDictionaryEntry> getMap() {
        return Collections.unmodifiableMap(reverseMap);
    }


    /**
     * Takes a list of the children of an xml node, selects the new
     * hierarchical elements and converts them into a number of dictionary
     * entries which are added to this object.
     * This method acts recursively since any node may contain children.
     *
     * @param kidList a list of the children of an xml node.
     * @param parentName name of the parent xml node.
     * @param warn     if true, print out warning debug messages.
     */
    private void addHierarchicalDictEntries(NodeList kidList, String parentName, boolean warn) {

        if (kidList == null || kidList.getLength() < 1) return;

        boolean debug = false;
        Integer tag, tagEnd, num, numEnd;
        boolean badTagEntry, badNumEntry, badTagEndEntry, badNameEntry, badTypeEntry;
        boolean isTagRange, isNumRange, isLeaf;
        String name, tagStr, tagEndStr, numStr, numEndStr, typeStr, format, description;

        for (int i = 0; i < kidList.getLength(); i++) {

            Node node = kidList.item(i);
            if (node == null) continue;

            isLeaf = node.getNodeName().equalsIgnoreCase(ENTRY_LEAF);

            // Only looking for "bank" and "leaf" nodes
            if (!node.getNodeName().equalsIgnoreCase(ENTRY_BANK) && !isLeaf) {
                continue;
            }

            tag = tagEnd = num = numEnd = null;
            badTagEntry = badNumEntry = badTagEndEntry = badNameEntry = badTypeEntry = false;
            isTagRange = isNumRange = false;
            name = numStr = tagStr = typeStr = format = description = null;
            DataType type = DataType.UNKNOWN32;

            if (node.hasAttributes()) {
                NamedNodeMap map = node.getAttributes();

                // Get the name
                Node attrNode = map.getNamedItem(NAME);
                if (attrNode != null) {
                    name = attrNode.getNodeValue();
                }

                // Get the num or num range as the case may be
                attrNode = map.getNamedItem(NUM);
                if (attrNode != null) {
                    // Use regular expressions to parse num
                    Matcher matcher = pattern.matcher(attrNode.getNodeValue());

                    if (matcher.matches()) {
                        // First num may be null, is always >= 0 if existing.
                        // Since we're here, it does exist.
                        numStr = matcher.group(1);
                        try {
                            num = Integer.decode(numStr);
                            if (num > 255) {
                                if (warn) System.out.println("dictionary: H num = " + num + " is too large must be < 256, for " + name);
                                badNumEntry = true;
                            }
                        }
                        catch (NumberFormatException e) {
                            badNumEntry = true;
                        }

                        // Ending num
                        if (!badNumEntry) {
                            numEndStr = matcher.group(3);
                            if (numEndStr != null) {
                                try {
                                    numEnd = Integer.decode(numEndStr);
                                    if (numEnd > 255) {
                                        if (warn) System.out.println("dictionary: H numEnd = " + numEnd + " is too large must be < 256, for " + name);
                                        badNumEntry = true;
                                    }

                                    // The regexp matching only allows values >= 0 for tagEnd.
                                    // When tagEnd == 0 or tag == tagEnd, no range is defined.
                                    if (numEnd > 0 && !numEnd.equals(num)) {
                                        isNumRange = true;

                                        // Since a num range is defined, the name MUST contain at least one %n
                                        if (!name.contains("%n")) {
                                            badNameEntry = true;
                                            if (debug)
                                                System.out.println("dictionary: H num range defined so name must contain at least one %n, but = " + name);
                                        }
                                    }
                                }
                                catch (NumberFormatException e) {
                                    badNumEntry = true;
                                }
                            }
                            else {
                                // Set for later convenience in for loop
                                numEnd = num;
                            }
                        }

                    }
                    else {
                        badNumEntry = true;
if (debug) System.out.println("dictionary: H num must be a valid non-negative integer or range, so ignore entry for " + name);
                    }
                }

                // If no num defined, substitute "" for each %n
                if (numStr == null) {
                    name = name.replaceAll("%n", "");
                }

                // Get the tag or tag range as the case may be
                attrNode = map.getNamedItem(TAG);
                if (attrNode != null) {
                    // Use regular expressions to parse tag
                    Matcher matcher = pattern.matcher(attrNode.getNodeValue());

                    if (matcher.matches()) {
                        // First tag, never null, always >= 0, or no match occurs
                        tagStr = matcher.group(1);
                        try {
                            tag = Integer.decode(tagStr);
                            if (tag > 65535) {
                                if (warn) System.out.println("dictionary: H tag = " + tag + " is too large must be < 65536, for " + name);
                                badTagEntry = true;
                            }
                            //System.out.println("Tag, dec = " + tag);
                        }
                        catch (NumberFormatException e) {
                            badTagEntry = true;
                        }

                        // Ending tag
                        if (!badTagEntry) {
                            tagEndStr = matcher.group(3);
                            if (tagEndStr != null) {
                                try {
                                    tagEnd = Integer.decode(tagEndStr);
                                    if (tagEnd > 65535) {
                                        if (warn) System.out.println("dictionary: H tagEnd = " + tagEnd + " is too large must be < 65536, for " + name);
                                        badTagEndEntry = true;
                                    }

                                    // The regexp matching only allows values >= 0 for tagEnd.
                                    // Value of 0 means no range defined.
                                    if (tagEnd > 0) {
                                        isTagRange = true;
                                        //System.out.println("Tag end, dec = " + tagEnd);
                                    }
                                }
                                catch (NumberFormatException e) {
                                    badTagEndEntry = true;
                                }
                            }
                        }

                    }
                    else {
                        badTagEndEntry = true;
if (debug) System.out.println("dictionary: H tag must be a valid non-negative integer or range, so ignore entry for " + name);
                    }
                }

                // Scan name for the string "%t".
                // If the tag exists, substitute it for the %t.
                // If tag range defined, substitute "".
                if (isTagRange) {
                    if (num != null) {
                        // Cannot define num and tag range at the same time ...
                        badNumEntry = true;
if (debug) System.out.println("dictionary: H cannot define num (or num range) and tag range simultaneously for " + name);
                    }
                    else {
                        name = name.replaceAll("%t", "");
                    }
                }
                else {
                    name = name.replaceAll("%t", tagStr);
                }

                // Get the type, if any
                attrNode = map.getNamedItem(TYPE);
                if (attrNode != null) {
                    typeStr = attrNode.getNodeValue();
                    typeStr = typeStr.toUpperCase();
                    try {
                        type = DataType.valueOf(typeStr);
                    }
                    catch (Exception e) {
if (warn) System.out.println("dictionary: ignore invalid hierarchical type (" + typeStr + ") for " + name);
                        typeStr = null;
                    }
                }

                // Look for description node (xml element) as child of entry node
                NodeList children = node.getChildNodes();
                if (children.getLength() > 0) {

                    // Pick out first description element only
                    for (int j = 0; j < children.getLength(); j++) {
                        Node childNode = children.item(j);
                        if (childNode == null) continue;

                        // Only looking for "description" node
                        if (!childNode.getNodeName().equalsIgnoreCase(DESCRIPTION)) {
                            continue;
                        }

                        description = childNode.getTextContent();
//System.out.println("dictionary: found description = " + description);

                        // See if there's a format attribute
                        if (childNode.hasAttributes()) {
                            map = childNode.getAttributes();

                            // Get the format
                            attrNode = map.getNamedItem(FORMAT);
                            if (attrNode != null) {
                                format = attrNode.getNodeValue();
//System.out.println("dictionary: found format = " + format);
                            }
                        }
                        break;
                    }
                }

                // Skip invalid entries
                if (name == null || badNameEntry) {
                    if (warn) System.out.println("dictionary: H ignore name that is empty or does not contain \"%n\" for num range, name = " + name);
                    continue;
                }
                else if (tagStr == null || badTagEntry || badTagEndEntry) {
                    if (warn) System.out.println("dictionary: H ignore empty or invalid tag or tagEnd for name = " + name);
                    continue;
                }
                else if (badNumEntry) {
                    if (warn) System.out.println("dictionary: H ignore invalid num/num-range entry for name = " + name);
                    continue;
                }

                if (badTypeEntry) {
                    if (warn) System.out.println("dictionary: H ignore invalid type (" + typeStr + ") for " + name + ", must be valid evio type, num not defined?");
                    typeStr = null;
                }


                // If the num or num range is defined ...
                if (numStr != null) {
                    // Make sure num < numEnd
                    if (isNumRange && (num > numEnd)) {
                        int tmp = num;
                        num = numEnd;
                        numEnd = tmp;
                    }

                    String nameOrig = name;

                    // Range of nums (num == numEnd for no range)
                    for (int n = num; n <= numEnd; n++) {
                        // Scan name for the string "%n" and substitute num for it
                        name = nameOrig.replaceAll("%n", n + "");

                        // Create hierarchical name
                        if (parentName != null) name = parentName + delimiter + name;

                        // Find the parent entry if any
                        EvioDictionaryEntry parentEntry = null;
                        Node parentNode = node.getParentNode();
                        // A genuine parent can only be a "bank" xml element
                        if (parentNode != null && parentNode.getNodeName().equals("bank")) {
                            parentEntry = (EvioDictionaryEntry)(parentNode.getUserData("entry"));
                        }
//if (debug) System.out.println("dictionary: parent entry for " + name + " = " + parentEntry);

                        EvioDictionaryEntry key = new EvioDictionaryEntry(tag, n, tagEnd, type.toString(),
                                                                          description, format,
                                                                          parentEntry);

                        boolean entryAlreadyExists = true;

                        if (reverseMap.containsKey(name)) {
if (warn) System.out.println("dictionary: 4 ignore duplicate dictionary entry for " + name);
                        }
                        else {
                            // Only add to dictionary if both name and tag/num pair are unique
                            // This works due to overloaded equals method.
                            if (!tagNumMap.containsKey(key) && !tagNumReverseMap.containsKey(name)) {
                                 tagNumMap.put(key, name);
                                 tagNumReverseMap.put(name, key);
                                 entryAlreadyExists = false;
                            }
                            else {
if (warn) System.out.println("dictionary: 5 ignore duplicate dictionary entry for " + name);
                            }

                            if (!entryAlreadyExists) {
                                // Store info here so children can get at tag/num/tagEnd
                                node.setUserData("entry", key, null);
                                reverseMap.put(name, key);
                            }
                        }
                    }
                }
                // If no num defined ...
                else {
                    if (parentName != null) name = parentName + delimiter + name;

                    // Find the parent entry if any
                    EvioDictionaryEntry parentEntry = null;
                    Node parentNode = node.getParentNode();
                    // A genuine parent can only be a "bank" xml element
                    if (parentNode != null && parentNode.getNodeName().equals("bank")) {
                        parentEntry = (EvioDictionaryEntry)(parentNode.getUserData("entry"));
                    }

//System.out.println("  make bank/leaf name = " + name + " with description = " + description + ", format = " + format);
                    EvioDictionaryEntry key = new EvioDictionaryEntry(tag, null, tagEnd, type.toString(),
                                                                      description, format,
                                                                      parentEntry);

                   boolean entryAlreadyExists = true;

                    if (reverseMap.containsKey(name)) {
if (warn) System.out.println("dictionary: 6 ignore duplicate dictionary entry for " + name);
                    }
                    else {
                        if (isTagRange) {
                            if (!tagRangeMap.containsKey(key)) {
                                tagRangeMap.put(key, name);
                                entryAlreadyExists = false;
                            }
                            else {
if (warn) System.out.println("dictionary: 7 ignore duplicate dictionary entry for " + name);
                            }
                        }
                        else {
                            if (!tagOnlyMap.containsKey(key)) {
                                tagOnlyMap.put(key, name);
                                entryAlreadyExists = false;
                            }
                            else {
if (warn) System.out.println("dictionary: 8 ignore duplicate dictionary entry for " + name);
                            }
                        }

                        if (!entryAlreadyExists) {
                            node.setUserData("entry", key, null);
                            reverseMap.put(name, key);
                        }
                    }
                }

                // Look at this node's children recursively but skip a leaf's kids
                if (!isLeaf) {
                    addHierarchicalDictEntries(node.getChildNodes(), name, warn);
                }
                else if (node.hasChildNodes()) {
if (warn) System.out.println("dictionary: ignore children of \"leaf\" for " + name);
                }
            }
        }
    }


    /**
     * Returns the name of a given evio structure.
     * This is the method used in BaseStructure.toString() (and therefore
     * also in JEventViewer), to assign a dictionary entry to a particular
     * evio structure.
     *
     * @param structure the structure to find the name of.
     * @return a descriptive name or ??? if none found
     */
    public String getName(BaseStructure structure) {
        Integer tag = structure.getHeader().getTag();
        Integer num = structure.getHeader().getNumber();

        // Make sure dictionary knows this is a segment or tagsegment
        if (structure instanceof EvioSegment ||
            structure instanceof EvioTagSegment) {
            num = null;
        }

        return getName(tag, num);
	}


    /**
     * Returns the name associated with the given tag, num, and tagEnd.
     * If a valid tag and num are given (&gt;= 0) a search is made for:
     * <ol>
     * <li>an entry of a tag/num pair. If that fails,
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     * If only a valid tag is given, a search is made for:
     * <ol>
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     * All other argument values result in ??? being returned.
     *
     * @param tag  tag of dictionary entry to find
     * @param num  num of dictionary entry to find
     * @return descriptive name or ??? if none found
     */
    public String getName(Integer tag, Integer num) {
        return getName(tag, num, null);
	}


    /**
     * Returns the name associated with the given tag, num, and tagEnd.
     * If a valid tag and num are given (&gt;= 0) a search is made for:
     * <ol>
     * <li>an entry of a tag/num pair. If that fails,
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     * If only a valid tag is given, a search is made for:
     * <ol>
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     * If a valid tag range is given (different valid tag and tagEnd with no num),
     * a search is made for an entry of a tag range. Note: tag and tagEnd being the
     * same value or tagEnd being 0 mean that no range is defined - it's equivalent
     * to only specifying a tag.<p>
     *
     * Argument values which have no match result in "???" being returned.<p>
     *
     * Things are actually more complicated due to parent structures. Duplicate
     * entries (same tag, num, and tagEnd) are permitted only as long their
     * parent entries are different. Say, for example, that this dictionary is
     * defined as follows:
     * <pre><code>
     *
     *      &lt;bank name="B1" tag="1" num="1" &gt;
     *           &lt;bank name="sub1" tag="5" num="5" /&gt;
     *           &lt;bank name="sub2" tag="5" num="5" /&gt;
     *           &lt;leaf name="tagNum"   tag="10" num="10" /&gt;
     *           &lt;leaf name="tagOnly"  tag="20" /&gt;
     *           &lt;leaf name="tagRange" tag="30-40" /&gt;
     *      &lt;/bank&gt;
     *      &lt;bank name="B2" tag="2" num="2" &gt;
     *           &lt;leaf name="tagNum"   tag="10" num="10" /&gt;
     *           &lt;leaf name="tagOnly"  tag="20" /&gt;
     *           &lt;leaf name="tagRange" tag="30-40" /&gt;
     *      &lt;/bank&gt;
     *
     * </code></pre>
     *
     * You can see that the leaf entries under bank "B1" are identical to those under "B2".
     * This is permitted since B1 and B2 have different tag &amp; num values so there
     * is a way to tell the difference between the 2 instances of tagNum, tagOnly and
     * tagRange.<p>
     *
     * It is not possible to specify parents using the "dictEntry" XML element and
     * consequently duplicates are not allowed if using this form of dictionary
     * definition. Think of things like this: no parents = no duplicates.
     *
     *
     * @param tag       tag of dictionary entry to find
     * @param num       num of dictionary entry to find
     * @param tagEnd tagEnd of dictionary entry to find
     *
     * @param pTag       tag of dictionary entry's parent
     * @param pNum       num of dictionary entry's parent
     * @param pTagEnd tagEnd of dictionary entry's parent
     * @return descriptive name or "???" if none found
     */
    public String getName(Integer tag,  Integer num,  Integer tagEnd,
                          Integer pTag, Integer pNum, Integer pTagEnd) {
        // Check tag arg
        if (tag == null) return INameProvider.NO_NAME_STRING;

        // Need to at least define parent's tag. If not defined,
        // we cannot use parent info at all.
        if (pTag == null) {
            return getName(tag, num, tagEnd);
        }

        // The generated key below is equivalent (equals() overridden)
        // to the key existing in the map. Use it to find the value.
        EvioDictionaryEntry parentKey = new EvioDictionaryEntry(pTag, pNum, pTagEnd, DataType.UNKNOWN32.toString());
        //System.out.println("getName: parent key = " + parentKey);
        EvioDictionaryEntry key = new EvioDictionaryEntry(tag, num, tagEnd,
                                                          null, null, null, parentKey);

        return getName(key);
	}


    /**
     * Returns the name associated with the given tag, num, and tagEnd.
     * If a valid tag and num are given (&gt;= 0) a search is made for:
     * <ol>
     * <li>an entry of a tag/num pair. If that fails,
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     * If only a valid tag is given, a search is made for:
     * <ol>
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     * If a valid tag range is given (different valid tag and tagEnd with no num),
     * a search is made for an entry of a tag range. Note: tag and tagEnd being the
     * same value or tagEnd being 0 mean that no range is defined - it's equivalent
     * to only specifying a tag.<p>
     *
     * All other argument values result in ??? being returned.
     *
     * @param tag       tag of dictionary entry to find
     * @param num       num of dictionary entry to find
     * @param tagEnd tagEnd of dictionary entry to find
     * @return descriptive name or ??? if none found
     */
    public String getName(Integer tag, Integer num, Integer tagEnd) {
        // Check tag arg
        if (tag == null) return INameProvider.NO_NAME_STRING;

        // The generated key below is equivalent (equals() overridden)
        // to the key existing in the map. Use it to find the value.
        EvioDictionaryEntry key = new EvioDictionaryEntry(tag, num, tagEnd, null);
        return getName(key);
	}


    /**
     * Implementation of getName().
     * @param key dictionary entry to look up name for.
     * @return name associated with key or "???" if none.
     */
    private String getName(EvioDictionaryEntry key) {
        int tag = key.getTag();
        EvioDictionaryEntryType entryType = key.getEntryType();

        // name = ???
        String name = INameProvider.NO_NAME_STRING;

        outer:
        switch (entryType) {
            case TAG_NUM:
                // If a tag/num pair was specified ...
                // There may be multiple entries with the same tag/tagEnd/num values
                // but having parents with differing values. Since we don't specify
                // the parent info, we just get the first match found in the map.
                name = tagNumMap.get(key);
                if (name != null) {
                    break;
                }
                // Create tag-only key and try to find tag-only match
                key = new EvioDictionaryEntry(tag);

            case TAG_ONLY:
                // If only a tag was specified or a tag/num pair was specified
                // but there was no exact match for the pair ...
                name = tagOnlyMap.get(key);
                if (name != null) {
                    break;
                }
                // Create tag-range key and try to find tag-range match
                key = new EvioDictionaryEntry(tag, null, key.getTagEnd(), null);

            case TAG_RANGE:
                // If a range was specified in the args, check to see if
                // there's an exact match first ...
                name = tagRangeMap.get(key);
                if (name != null) {
                    break;
                }
                // If a tag/num pair or only a tag was specified in the args,
                // see if either falls in a range of tags.
                else if (entryType != EvioDictionaryEntryType.TAG_RANGE) {
                    // Additional check to see if tag fits in a range
                    for (Map.Entry<EvioDictionaryEntry, String> item : tagRangeMap.entrySet()) {
                        EvioDictionaryEntry entry = item.getKey();
                        if (entry.inRange(tag)) {
                            name = item.getValue();
                            break outer;
                        }
                    }
                }

            default:
//                System.out.println("no dictionary entry for tag = " + tag +
//                                   ", tagEnd = " + key.getTagEnd() + ", num = " + key.getNum());
        }

        return name;
	}




    /**
     * Returns the dictionary entry, if any, associated with the given tag, num, and tagEnd.
     *
     * @param tag       tag of dictionary entry to find
     * @param num       num of dictionary entry to find
     * @param tagEnd tagEnd of dictionary entry to find
     * @return entry or null if none found
     */
    private EvioDictionaryEntry entryLookupByData(Integer tag, Integer num, Integer tagEnd) {
        // Given data, find the entry in dictionary that corresponds to it.
        //
        // The generated key below is equivalent (equals() overridden) to the key existing
        // in the map. Use it to find the value, then use the value to find the
        // original key which contains other data besides tag, tagEnd, and num.
        EvioDictionaryEntry key = new EvioDictionaryEntry(tag, num, tagEnd, DataType.UNKNOWN32.toString());
        EvioDictionaryEntryType entryType = key.getEntryType();
//System.out.println("entryLookup, entryType = " + key.getEntryType() + ", key = " + key);

        String name;
        EvioDictionaryEntry entry = null;

        outer:
        switch (entryType) {
            case TAG_NUM:
                name = tagNumMap.get(key);
                if (name != null) {
                    entry = tagNumReverseMap.get(name);
                    //System.out.println("entryLookup, with key, found name = " + name);
                    //System.out.println("                       entry = " + entry);
                    break;
                }
//                else {
//                    System.out.println("entryLookup, found nothing in tagNumMap");
//                }

                // Create tag-only key and try to find tag-only match
                key = new EvioDictionaryEntry(tag);

            case TAG_ONLY:
                name = tagOnlyMap.get(key);
                if (name != null) {
                    // Found value, but now use value (name) to get the original key.
                    for (Map.Entry<EvioDictionaryEntry, String> item : tagOnlyMap.entrySet()) {
                        String n = item.getValue();
                        if (n.equals(name)) {
                            entry = item.getKey();
                            break outer;
                        }
                    }
                }

                // Create tag-range key and try to find tag-range match
                key = new EvioDictionaryEntry(tag, null, tagEnd, null);

            case TAG_RANGE:
                name = tagRangeMap.get(key);
                if (name != null) {
                    for (Map.Entry<EvioDictionaryEntry, String> item : tagRangeMap.entrySet()) {
                        String n = item.getValue();
                        if (n.equals(name)) {
                            entry = item.getKey();
                            break outer;
                        }
                    }
                }
                // If a tag/num pair or only a tag was specified in the args,
                // see if either falls in a range of tags.
                else if (entryType != EvioDictionaryEntryType.TAG_RANGE) {
                    // See if tag fits in a range
                    for (Map.Entry<EvioDictionaryEntry, String> item : tagRangeMap.entrySet()) {
                        EvioDictionaryEntry entry2 = item.getKey();
                        if (entry2.inRange(tag)) {
                            entry = entry2;
                            break outer;
                        }
                    }
                }

            default:
//                System.out.println("no dictionary entry for tag = " + tag +
//                                   ", tagEnd = " + tagEnd + ", num = " + num);
        }

        return entry;
    }


    /**
     * Returns the dictionary entry, if any, associated with the given name.
     *
     * @param name name associated with entry
     * @return entry or null if none found
     */
    private EvioDictionaryEntry entryLookupByName(String name) {
        // Check all entries
        EvioDictionaryEntry entry = reverseMap.get(name);
        if (entry != null) {
            return entry;
        }

        return null;
    }


    /**
     * Returns the description, if any, associated with the given tag and num.
     *
     * @param tag    to find the description of
     * @param num    to find the description of
     * @return description or null if none found
     */
    public String getDescription(Integer tag, Integer num) {
        return getDescription(tag, num, null);
	}


    /**
     * Returns the description, if any, associated with the given tag, num, and tagEnd.
     *
     * @param tag    to find the description of
     * @param num    to find the description of
     * @param tagEnd to find the description of
     * @return description or null if none found
     */
    public String getDescription(Integer tag, Integer num, Integer tagEnd) {
        EvioDictionaryEntry entry = entryLookupByData(tag, num, tagEnd);
        if (entry == null) {
            return null;
        }

        return entry.getDescription();
	}


    /**
     * Returns the description, if any, associated with the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return description; null if name or is unknown or no description is associated with it
     */
    public String getDescription(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return null;
        }

        return entry.getDescription();
	}


    /**
     * Returns the format, if any, associated with the given tag and num.
     *
     * @param tag to find the format of
     * @param num to find the format of
     * @return the format or null if none found
     */
    public String getFormat(int tag, int num) {
        return getFormat(tag, num, null);
    }


    /**
     * Returns the format, if any, associated with the given tag, num, and tagEnd.
     *
     * @param tag    to find the format of
     * @param num    to find the format of
     * @param tagEnd to find the format of
     * @return  format or null if none found
     */
    public String getFormat(Integer tag, Integer num, Integer tagEnd) {
        EvioDictionaryEntry entry = entryLookupByData(tag, num, tagEnd);
        if (entry == null) {
            return null;
        }

        return entry.getFormat();
    }


    /**
     * Returns the format, if any, associated with the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return format; null if name or is unknown or no format is associated with it
     */
    public String getFormat(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return null;
        }

        return entry.getFormat();
	}


    /**
     * Returns the type, if any, associated with the given tag and num.
     *
     * @param tag to find the type of
     * @param num to find the type of
     * @return type or null if none found
     */
    public DataType getType(Integer tag, Integer num) {
        return getType(tag, num, null);
	}


    /**
     * Returns the type, if any, associated with the given tag, num, and tagEnd.
     *
     * @param tag    to find the type of
     * @param num    to find the type of
     * @param tagEnd to find the type of
     * @return type or null if none found
     */
    public DataType getType(Integer tag, Integer num, Integer tagEnd) {
        EvioDictionaryEntry entry = entryLookupByData(tag, num, tagEnd);
        if (entry == null) {
            return null;
        }

        return entry.getType();
	}


    /**
     * Returns the type, if any, associated with the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return type; null if name is unknown or no type is associated with it
     */
    public DataType getType(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return null;
        }

        return entry.getType();
	}


    /**
     * Does the given dictionary entry exist?
     * @param name dictionary entry name.
     * @return true if this is a valid dictionary entry, else false.
     */
    public boolean exists(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return false;
        }
        return true;
    }


    /**
     * Does the given dictionary entry (if any) represent a range of tags?
     * @param name dictionary entry name.
     * @return true if this is a valid dictionary entry and it represents a range of tags, else false.
     */
    public boolean isTagRange(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return false;
        }

        return (entry.getEntryType() == EvioDictionaryEntryType.TAG_RANGE);
    }


    /**
     * Does the given dictionary entry (if any) represent only a single tag without a num?
     * @param name dictionary entry name.
     * @return true if this is a valid dictionary entry and it represents only a single tag without a num, else false.
     */
    public boolean isTagOnly(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return false;
        }

        return (entry.getEntryType() == EvioDictionaryEntryType.TAG_ONLY);
    }


    /**
     * Does the given dictionary entry (if any) represent a single tag and num pair?
     * @param name dictionary entry name.
     * @return true if this is a valid dictionary entry and it represents a single tag and num pair, else false.
     */
    public boolean isTagNum(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return false;
        }

        return (entry.getEntryType() == EvioDictionaryEntryType.TAG_NUM);
    }


    /**
     * Returns the tag/num/tagEnd values, in an Integer object array,
     * corresponding to the name of a dictionary entry.<p>
     *
     * If there is an exact match with a tag and num pair, it is returned
     * (last element is null).
     * If not, but there is a match with a tag-only entry, that is returned
     * (last 2 elements are null).
     * If no tag-only match exits, but there is a match with a tag range,
     * that is returned (i.e. second element, the num, is null).
     *
     * @since 5.2 now returns 3 Integer objects instead of
     *            2 ints (tag, num) as in previous versions.
     * @param name dictionary name
     * @return an Integer object array in which the first element is the tag,
     *         the second is the num, and the third is the end of a tag range;
     *         null if name is unknown
     */
    public Integer[] getTagNum(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry != null) {
            return new Integer[] {entry.getTag(), entry.getNum(), entry.getTagEnd()};
        }

        return null;
    }


    /**
     * Returns the tag corresponding to the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return tag or null if name unknown
     */
    public Integer getTag(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return null;
        }

        return entry.getTag();
	}


    /**
     * Returns the tagEnd corresponding to the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return tagEnd or null if name unknown
     */
    public Integer getTagEnd(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return null;
        }

        return entry.getTagEnd();
	}


    /**
     * Returns the num corresponding to the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return num or null if name unknown
     */
    public Integer getNum(String name) {
        EvioDictionaryEntry entry = entryLookupByName(name);
        if (entry == null) {
            return null;
        }

        return entry.getNum();
	}


	/**
	 * Return a dom object corresponding to an xml file.
	 * 
	 * @param file the XML File object
	 * @return the dom object (or <code>null</code>.)
	 */
	private static Document getDomObject(File file) {
		// get the factory
		DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
		Document dom = null;

		try {
			// Using factory get an instance of document builder
			DocumentBuilder db = dbf.newDocumentBuilder();

			// parse using builder to get DOM representation of the XML file
			dom = db.parse(file);
		}
        catch (Exception e) {
            e.printStackTrace();
        }

		return dom;
	}


    /**
     * Return a dom object corresponding to an xml string.
     *
     * @param xmlString XML string representing dictionary
     * @return the dom object (or <code>null</code>.)
     */
    private static Document getDomObject(String xmlString) {
        // get the factory
        DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
        Document dom = null;

        try {
            // Using factory get an instance of document builder
            DocumentBuilder db = dbf.newDocumentBuilder();

            // parse using builder to get DOM representation of the XML string
            ByteArrayInputStream bais = new ByteArrayInputStream(xmlString.getBytes());
            dom = db.parse(bais);
        }
        catch (Exception e) {
            e.printStackTrace();
        }

        return dom;
    }


    /**
     * Get an xml representation of the dictionary.
     * @return an xml representation of the dictionary.
     */
    public String toXML() {
        return nodeToString(topLevelDoc);
    }


    /**
     * This method converts an xml Node object into an xml string.
     * @return an xml representation of an xml Node object; null if conversion problems
     */
    private String nodeToString(Node node) {
        if (xmlRepresentation != null) return xmlRepresentation;

        StringWriter sw = new StringWriter();
        try {
            TransformerFactory transformerFactory = TransformerFactory.newInstance();
            transformerFactory.setAttribute("indent-number", 3);
            Transformer t = transformerFactory.newTransformer();
            t.setOutputProperty(OutputKeys.OMIT_XML_DECLARATION, "yes");
            t.setOutputProperty(OutputKeys.INDENT, "yes");
            t.transform(new DOMSource(node), new StreamResult(sw));
        } catch (TransformerException te) {
            return null;
        }

        xmlRepresentation = sw.toString();
        return xmlRepresentation;
    }


    /**
     * Get a string representation of the dictionary.
     * @return a string representation of the dictionary.
     */
    @Override
    public String toString() {
        if (stringRepresentation != null) return stringRepresentation;

        int row=1;
        EvioDictionaryEntry entry;
        StringBuilder sb = new StringBuilder(4096);
        sb.append("-- Dictionary --\n\n");

        for (String name : reverseMap.keySet()) {
            // Get the entry
            entry = reverseMap.get(name);
            Integer num = entry.getNum();
            Integer tag = entry.getTag();
            Integer tagEnd = entry.getTagEnd();

            switch (entry.getEntryType()) {
                case TAG_RANGE:
                    sb.append(String.format("%-30s: tag range %d-%d\n", name, tag, tagEnd));
                    break;
                case TAG_ONLY:
                    sb.append(String.format("%-30s: tag %d\n", name, tag));
                    break;
                case TAG_NUM:
                    sb.append(String.format("%-30s: tag %d, num %d\n", name, tag, num));
                    break;
                default:
            }

            if (row++ % 4 == 0) {
                sb.append("\n");
            }

        }

        stringRepresentation = sb.toString();
        return stringRepresentation;
    }
}
