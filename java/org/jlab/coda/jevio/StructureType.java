package org.jlab.coda.jevio;

/**
 * This an enum used to convert structure type numerical values to a more meaningful name. For example, the structure
 * type with value 0xe corresponds to the enum BANK. Mostly this is used for printing.
 * 
 * @author heddle
 * 
 */
public enum StructureType {

	UNKNOWN32(0x0), 
	TAGSEGMENT(0xc), 
	SEGMENT(0xd), 
	BANK(0xe);

	private int value;

	private StructureType(int value) {
		this.value = value;
	}

	/**
	 * Get the enum's value.
	 * 
	 * @return the value, e.g., 0xe for a BANK
	 */
	public int getValue() {
		return value;
	}

	/**
	 * Obtain the name from the value.
	 * 
	 * @param value the value to match.
	 * @return the name, or "UNKNOWN".
	 */
	public static String getName(int value) {
		StructureType structuretypes[] = StructureType.values();
		for (StructureType dt : structuretypes) {
			if (dt.value == value) {
				return dt.name();
			}
		}
		return "UNKNOWN";
	}

	/**
	 * Obtain the enum from the value.
	 * 
	 * @param value the value to match.
	 * @return the matching enum, or <code>null</code>.
	 */
	public static StructureType getStructureType(int value) {
		StructureType structuretypes[] = StructureType.values();
		for (StructureType dt : structuretypes) {
			if (dt.value == value) {
				return dt;
			}
		}
		return null;
	}
}
