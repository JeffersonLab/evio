package org.jlab.coda.hipo;

/**
 * This is a general exception used to indicate a problem in the HIPO package.
 * @author timmer
 */
@SuppressWarnings("serial")
public class HipoException extends Exception {

    /**
     * Create a HIPO Exception indicating an error specific to the HIPO system.
     *
     * @param message the detail message. The detail message is saved for
     *                later retrieval by the {@link #getMessage()} method.
     */
    public HipoException(String message) {
        super(message);
    }

    /**
     * Create an HIPO Exception with the specified message and cause.
     *
     * @param  message the detail message. The detail message is saved for
     *         later retrieval by the {@link #getMessage()} method.
     * @param  cause the cause (which is saved for later retrieval by the
     *         {@link #getCause()} method).  (A <tt>null</tt> value is
     *         permitted, and indicates that the cause is nonexistent or
     *         unknown.)
     */
    public HipoException(String message, Throwable cause) {
        super(message, cause);
    }

    /**
     * Create an HIPO Exception with the specified cause.
     *
     * @param  cause the cause (which is saved for later retrieval by the
     *         {@link #getCause()} method).  (A <tt>null</tt> value is
     *         permitted, and indicates that the cause is nonexistent or
     *         unknown.)
     */
    public HipoException(Throwable cause) {
        super(cause);
    }

}
