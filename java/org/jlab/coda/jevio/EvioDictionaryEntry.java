package org.jlab.coda.jevio;

import java.util.StringTokenizer;

/**
 * An entry into the evio dictionary.
 * 
 * @author heddle
 * @deprecated
 * 
 */
public class EvioDictionaryEntry implements Comparable<EvioDictionaryEntry> {

	/**
	 * The tag will have to match the tag field in a segment.
	 */
	private int tag;

	/**
	 * The result of the number field being tokenized, with "." serving as the delimiter. These are tokens.
	 */
	private int numbers[];

    /**
	 * The nice name for this dictionary entry--the description.
	 */
	private String description;

	/**
	 * Constructor.
	 * 
	 * @param tag the string tag, will be converted into an int.
	 * @param num the string num field, which might be <code>null</code>, and which will be tokenized (for possible
	 *            ancestor matching) and converted into an array of ints.
	 * @param description the nice name for this dictionary entry--the description.
	 */
	public EvioDictionaryEntry(String tag, String num, String description) {
		this.description = description;

		// the tag field should clean--no hierarchy
		this.tag = Integer.parseInt(tag);

		// The <code>num</code> value is a string, rather than an int, because it might represent
		// a bank hierarchy. For example, a number of "2.3.1" means that a bank matches this
		// entry if a) its num field in the header is 1, and b) its parent's num field is 3, and
        // c) it's grandparent's num field is 2.
		if (num != null) {
			String tokens[] = tokens(num, ".");

			numbers = new int[tokens.length];
			// flip the order so numbers[0] is primary match.
			int j = 0;
			for (int i = tokens.length - 1; i >= 0; i--) {
				numbers[j] = Integer.parseInt(tokens[i]);
				j++;
			}
		}
	}

    /**
     * Checks if a structure matches this dictionary entry. This is more complicated than at first glance, because a
     * matching of not just the structure but also (for banks) the num fields of its immediate ancestors might be
     * required.
     *
     * @param structure the structure to test.
     * @return <code>true</code> if the structure matches.
     */
    public boolean match(BaseStructure structure) {

        if (structure instanceof EvioEvent) {
            if ((numbers == null) || (numbers.length != 1)) {
                return false;
            }
            BaseStructureHeader header = structure.header;
            return (tag == header.tag) && (numbers[0] == header.number);
        }
        else if (structure instanceof EvioBank) {
            if (structure.header.tag != tag) {
                return false;
            }

            if (numbers == null) {
                return false;
            }

            for (int i = 0; i < numbers.length; i++) {
                BaseStructureHeader header = structure.header;
                if (numbers[i] != header.number) {
                    return false;
                }

                structure = structure.getParent();
                if (structure == null) {
                    // Once we've reached the top event,
                    // parent = null, but match is good. timmer 9/30/2010
                    if (i >= numbers.length-1) {
                        return true;
                    }
                    return false;
                }
                if (structure instanceof EvioSegment) {
                    return false;
                }
                if (structure instanceof EvioTagSegment) {
                    return false;
                }
            }
            return true;
        }
        else {
            BaseStructureHeader header = structure.header;
            return (tag == header.tag);
        }
    }

    /**
     * We sort so that the entries with the most number of ancestors match.
     *
     * @param entry the object to compare against.
     */
    @Override
	public int compareTo(EvioDictionaryEntry entry) {
        if (entry.numbers == null) return -1;
        if (numbers == null) return 1;

        // we want the entrys with the longer ancestor trail up top
		if (numbers.length > entry.numbers.length) {
			return -1;
		}
		if (numbers.length < entry.numbers.length) {
			return 1;
		}
		// same number of tokens
		for (int i = 0; i < numbers.length; i++) {
			if (numbers[i] < entry.numbers[i]) {
				return -1;
			}
			if (numbers[i] > entry.numbers[i]) {
				return 1;
			}
		}

		return 0;
	}

	/**
	 * This method breaks a string into an array of tokens.
	 * 
	 * @param str the string to decompose.
	 * @param token the token character.
	 * @return an array of tokens.
	 */
	private String[] tokens(String str, String token) {
		StringTokenizer t = new StringTokenizer(str, token);
		int num = t.countTokens();
		String tokens[] = new String[num];

		for (int i = 0; i < num; i++) {
			tokens[i] = t.nextToken();
		}

		return tokens;
	}

	/**
	 * Get the nice, hopefully descriptive name for this entry.
	 * 
	 * @return the nice, hopefully descriptive name for this entry.
	 */
	public String getDescription() {
		return description;
	}

    /**
     * Get the tag for this entry.
     *
     * @return the tag for this entry.
     */
    public int getTag() {
        return tag;
    }

    /**
     * Get an xml representation of this entry.
     * @return an xml representation of this entry.
     */
    public StringBuilder toXML() {
        StringBuilder sb = new StringBuilder(100);
        sb.append("<xmldumpDictEntry name=\"");
        sb.append(description);
        sb.append("\"  tag=\"");
        sb.append(tag);
        sb.append("\"");
    
        String ns = null;
        if (numbers != null) {
            for (int i = numbers.length - 1; i >= 0; i--) {
                if (ns == null) {
                    ns = "";
                }
                ns += numbers[i];
                if (i != 0) {
                    ns += ".";
                }
            }
        }

        if (ns != null) {
            sb.append("  num=\"");
            sb.append(ns);
            sb.append("\"");
        }

        sb.append("/>\n");
        return sb;
    }

	/**
	 * Get a string representation of this entry.
	 * 
	 * @return a string representation of this entry.
	 */
	@Override
	public String toString() {
		String ns = null;
		if (numbers != null) {
			for (int i = numbers.length - 1; i >= 0; i--) {
				if (ns == null) {
					ns = "";
				}
				ns += numbers[i];
				if (i != 0) {
					ns += ".";
				}
			}
		}
		if (ns == null) {
			return String.format("tag: %-4d description: %s", tag, description);
		}
		else {
			return String.format("tag: %-4d num: %-10s description: %s", tag, ns, description);
		}
	}

}
