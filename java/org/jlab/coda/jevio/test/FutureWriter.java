package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.EvioException;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.channels.CompletionHandler;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;
import java.util.concurrent.Future;


/**
 * 
 */

/**
 * @author heyes
 *
 */
public class FutureWriter {

    private AsynchronousFileChannel asyncFileChannel;

	boolean done = false;

	Thread self;

	public static byte[] intToByteArray(int a)
	{
	    byte[] ret = new byte[4];
	    ret[0] = (byte) (a & 0xFF);
	    ret[1] = (byte) ((a >> 8) & 0xFF);
	    ret[2] = (byte) ((a >> 16) & 0xFF);
	    ret[3] = (byte) ((a >> 24) & 0xFF);
	    return ret;
	}


	int nbuf = 10;

    ByteBuffer[] buffer = new ByteBuffer[nbuf];
    /** First element is buffer used in future1, the second in future2. */
    ByteBuffer[] usedBuffers = new ByteBuffer[2];


	int buf_in_ix = 0;
	int buf_out_ix = 0;

    int buffers_written = 0;
    int bufferSize = 2000000000;  // 2 GB


    long fileWritingPosition = 0L;
    long bytesWrittenToFile = 0L;
    long splitSize = 10000000000L;   // 10GB file size
    int  splitCount = 0;
    int  maxFiles = 4;

    /** Objects to allow efficient, asynchronous file writing. */
    private Future<Integer> future1, future2;

    /** Index for selecting which future (1 or 2) to use for next file write. */
    private int futureIndex;

    /** Path object corresponding to file currently being written. */
    private Path currentFilePath;

    private String fileName = "test-write.dat.";




    /**
     * Write everything in buffer to file.
     *
     * @param force     if true, force it to write event to the disk.
     * @param buffer    buffer of data to write to file.
     *
     * @throws IOException   if error writing file
     */
    private void flushToFile(boolean force, ByteBuffer buffer)
            throws IOException {

        // This actually creates the file. Do it only once.
        if (bytesWrittenToFile < 1L) {
            asyncFileChannel = createChan(fileName + splitCount);
//System.out.println("\n*******\nOPENED NEW FILE " + currentFilePath.toFile().getName());
            fileWritingPosition = 0L;
            splitCount++;
        }

        // Write everything in buffer out to file
        int bytesWritten = buffer.remaining();

        // We need one of the 2 future jobs to be completed in order to proceed

        // If 1st time thru, proceed without waiting
        if (future1 == null) {
            futureIndex = 0;
        }
        // If 2nd time thru, proceed without waiting
        else if (future2 == null) {
            futureIndex = 1;
        }
        // After first 2 times, wait until one of the
        // 2 futures is finished before proceeding
        else {
            // If future1 is finished writing, proceed
            if (future1.isDone()) {
                futureIndex = 0;
            }
            // If future2 is finished writing, proceed
            else if (future2.isDone()) {
                futureIndex = 1;
            }
            // Neither are finished, so wait for one of them to finish
            else {
                // If the last write to be submitted was future2, wait for 1
                if (futureIndex == 0) {
                    try {
                        // Wait for write to end before we continue
                        future1.get();
                        freeBuffer(usedBuffers[0]);
                    }
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }
                // Otherwise, wait for 2
                else {
                    try {
                        future2.get();
                        freeBuffer(usedBuffers[1]);
                    }
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }
            }
        }

        if (futureIndex == 0) {
            future1 = asyncFileChannel.write(buffer, fileWritingPosition);
            usedBuffers[0] = buffer;
            futureIndex = 1;
        }
        else {
            future2 = asyncFileChannel.write(buffer, fileWritingPosition);
            usedBuffers[1] = buffer;
            futureIndex = 0;
        }

        fileWritingPosition += bytesWritten;
        // Keep track of what is written to this, one, file
        bytesWrittenToFile += bytesWritten;

        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
        // but don't bother writing the metadata (arg to force()) since that slows it
        // down even more.
        // TODO: This may not work since data may NOT have been written yet!
        if (force) asyncFileChannel.force(false);
    }


	public synchronized ByteBuffer getBuffer() {
		ByteBuffer the_buffer = buffer[buf_out_ix];
		buffer[buf_out_ix] = null;
		buf_out_ix++;
		if (buf_out_ix >= nbuf) buf_out_ix = 0;
		return the_buffer;
	}


	public synchronized void freeBuffer(ByteBuffer theBuffer) {
		theBuffer.rewind();
        buffer[buf_in_ix++] = theBuffer;
        if (buf_in_ix >= nbuf) buf_in_ix = 0;
	}


	public AsynchronousFileChannel createChan(String name) throws IOException {
		currentFilePath = Paths.get(name);

		if (!Files.exists(currentFilePath)) {
		    Files.createFile(currentFilePath);
		}

		// Create our file
	    return AsynchronousFileChannel.open(currentFilePath,
                                             StandardOpenOption.WRITE,
                                             StandardOpenOption.TRUNCATE_EXISTING);
			                                 //, StandardOpenOption.SYNC);
	}


	public FutureWriter() {

		self = Thread.currentThread();
		boolean force = false;

        System.out.println("Startup: buffer size = " + bufferSize);

		// Allocate and fill buffers
		buffer = new ByteBuffer[nbuf];

		for (int ix = 0; ix < buffer.length; ix++) {
			buffer[ix] = ByteBuffer.allocateDirect(bufferSize);

            // Fill buffer with buffer number as a marker
			byte[] data = intToByteArray(ix);

			while (buffer[ix].remaining() >= 4) {
				buffer[ix].put(data);
			}
			// flip puts the buffer in a state where it can be read from
			buffer[ix].flip();
            System.out.println("Filling is " + (100*(ix+1)/nbuf) + "% done");
		}

		Instant start;

        // Write fixed number of files
        for (int i=0; i < maxFiles; i++) {
            
            start = Instant.now();

            // Write into file until it's time to split
            while (bytesWrittenToFile < splitSize) {

                ByteBuffer to_write = getBuffer();
                to_write.putInt(0, buffers_written++);

                try {
                    flushToFile(force, to_write);
                }
                catch (Exception e) {
                    e.printStackTrace();
                }
            }

            // Close file when all writing is finished
            try {
                // Finish writing to current file
                if (future1 != null) {
                    try {
                        // Wait for last write to end before we continue
                        future1.get();
                    }
                    catch (Exception e) {}
                }

                if (future2 != null) {
                    try {
                        future2.get();
                    }
                    catch (Exception e) {}
                }

                asyncFileChannel.close();

                long duration = Duration.between(start, Instant.now()).toMillis();
                System.out.println("bytes written: " + bytesWrittenToFile +
                                           ", time taken: " + duration + " mS, "
                                           + bytesWrittenToFile / (duration * 1000) + " MB/s");

            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }

            // Next file
            bytesWrittenToFile = 0L;
        }



		System.out.println("Done");
	}
			
		
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		new FutureWriter();
	}

}
