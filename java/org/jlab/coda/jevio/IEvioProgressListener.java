package org.jlab.coda.jevio;

/**
 * Interface used for listening for progress.
 */
public interface IEvioProgressListener {

	/**
	 * Something is listening for progress, e.g. the number of events that has been written to XML.
	 * @param num the current number,
	 * @param total the total number, i.e, we have completed num out of total.
	 */
	void completed(int num, int total);
}
