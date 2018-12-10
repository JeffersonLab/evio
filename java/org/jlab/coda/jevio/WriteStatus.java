package org.jlab.coda.jevio;

/**
 * This <code>enum</code> denotes the status of a write.<br>
 * SUCCESS indicates a successful write. <br>
 * CANNOT_OPEN_FILE indicates that we cannot write because the destination file cannot be opened.<br>
 * EVIO_EXCEPTION indicates that an EvioException was thrown during a write.<br>
 * UNKNOWN_ERROR indicates that an unrecoverable error has occurred.
 */
public enum WriteStatus {
	SUCCESS, CANNOT_OPEN_FILE, EVIO_EXCEPTION, UNKNOWN_ERROR
}
