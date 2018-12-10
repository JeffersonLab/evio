package org.jlab.coda.jevio;

/**
* This <code>enum</code> denotes the status of a read. <br>
* SUCCESS indicates a successful read. <br>
* END_OF_FILE indicates that we cannot read because an END_OF_FILE has occurred. Technically this means that what
* ever we are trying to read is larger than the buffer's unread bytes.<br>
* EVIO_EXCEPTION indicates that an EvioException was thrown during a read, possibly due to out of range values,
* such as a negative start position.<br>
* UNKNOWN_ERROR indicates that an unrecoverable error has occurred.
*/
public enum ReadStatus {
SUCCESS, END_OF_FILE, EVIO_EXCEPTION, UNKNOWN_ERROR
}

