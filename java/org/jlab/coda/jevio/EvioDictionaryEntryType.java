/*
 * Copyright (c) 2016, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio;

/**
 * Each dictionary entry may have different values of tag, tagEnd, and num specified.
 * This enum assigns a type to each combination of these values.
 *
 * (10/4/16).
 * @author timmer.
 */
enum EvioDictionaryEntryType {
    /** Valid tag &amp; num, with or without a tagEnd. */
    TAG_NUM,
    /** Valid tag, but no num or tagEnd. */
    TAG_ONLY,
    /** Valid tag and tagEnd, but no num. */
    TAG_RANGE;
}
