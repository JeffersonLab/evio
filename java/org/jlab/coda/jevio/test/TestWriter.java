package org.jlab.coda.jevio.test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.channels.CompletionHandler;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.time.Duration;
import java.time.Instant;
import java.util.concurrent.Future;


/**
 * 
 */

/**
 * @author heyes
 *
 */
public class TestWriter {
	
	private AsynchronousFileChannel[] the_channel = new AsynchronousFileChannel[2];
	
	private int use_one = 0;
	
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
	
	public static int byteArrayToInt(byte[] b) 
	{
	    return   b[0] & 0xFF |
	            (b[1] & 0xFF) << 8 |
	            (b[2] & 0xFF) << 16 |
	            (b[3] & 0xFF) << 24;
	}
	
	int nbuf = 10;
	
	ByteBuffer[] buffer = new ByteBuffer[nbuf];

	
	volatile int being_written =0;
	
	int buf_in_ix = 0;
	int buf_out_ix = 0;

    int buffers_written = 0;
    volatile int callBacks_run = 0;

	int writesPerFile = 20;

    int bufferSize = 2000000000;  // 2 GB
    Instant[] starts = new Instant[300];
    volatile long totalDuration = 0L;

    // File writing handler
    CompletionHandler<Integer, ByteBuffer> handler = new CompletionHandler<Integer, ByteBuffer>() {
        @Override
        public void completed(Integer result, ByteBuffer attachment) {
            int buf_ix = attachment.getInt(0);
            long duration = Duration.between(starts[buf_ix], Instant.now()).toMillis();
            System.out.println("bytes written: " + result + ", index: " + buf_ix);

            System.out.println("index: " + buf_ix + ", time taken: " + duration + " mS, "
                                       + bufferSize / (duration * 1000) + " MB/s");
            freeBuffer(attachment);
            totalDuration += duration;
            callBacks_run++;
        }

        @Override
        public void failed(Throwable exc, ByteBuffer attachment) {
            System.out.println("Write failed");
            exc.printStackTrace();
        }
    };




    public synchronized boolean doWrite() {
		return (buffer[buf_out_ix] != null) && (being_written <= 3);
	}
	
	public synchronized ByteBuffer getBuffer() {
		being_written++;
		ByteBuffer the_buffer = buffer[buf_out_ix];
		buffer[buf_out_ix] = null;
		buf_out_ix++;
		if (buf_out_ix >= nbuf)buf_out_ix = 0;
		return the_buffer;
	}
	
	public synchronized void freeBuffer(ByteBuffer theBuffer) {
		theBuffer.rewind();
		being_written--;
        buffer[buf_in_ix++] = theBuffer;
        if (buf_in_ix >= nbuf) buf_in_ix = 0;
	}
	
	public AsynchronousFileChannel createChan(String name) {
		Path path = Paths.get(name);
		
		if(!Files.exists(path)){
		    try {
				Files.createFile(path);
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
				return null;
			}
		}
		
		// Create our file
		 try {
			return
			    AsynchronousFileChannel.open(path,
                                             StandardOpenOption.WRITE,
                                             StandardOpenOption.TRUNCATE_EXISTING);
			                                 //, StandardOpenOption.SYNC);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			return null;
		}
	}
	
	public TestWriter() {

		self = Thread.currentThread();

		System.out.println("Startup: buffer size = " + bufferSize);

		the_channel[0] = createChan("test-write.dat");
		the_channel[1] = createChan("test-write1.dat");

		// allocate and fill buffers

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

		// position in file to write next block to.
		long[] position = { 0, 0 };

		// Do 200 writes per file...
		while (buffers_written < writesPerFile) {
			ByteBuffer to_write;

			// Is there a buffer to write and is it OK to write it?
			if (doWrite()) {
				starts[buffers_written] = Instant.now();

				to_write = getBuffer();
				int write_size = to_write.limit();
				// System.out.println("bytes to write: " + write_size);

				// System.out.println("call write # : "+ buffers_written);

				to_write.putInt(0, buffers_written++);

				the_channel[1 & use_one].write(to_write, position[1 & use_one], to_write, handler);
				position[1 & use_one++] += (long) write_size;

			} else {
				try {
					Thread.sleep(2);
				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
		}

		// Wait until the rest of the writes finish
        while (callBacks_run < buffers_written) {
            try {
                Thread.sleep(1000);
            }
            catch (InterruptedException e) {
            }
            System.out.println("Wait for callbacks to finish ...");
        }

        long avgDuration = (totalDuration/writesPerFile);
        System.out.println("\n\nAvg time taken: " + avgDuration + " mS, "
                                   + bufferSize / (avgDuration * 1000) + " MB/s");

		System.out.println("Done");

		try {
			the_channel[0].close();
			the_channel[1].close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

	}
			
		
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		
		new TestWriter();

	}

}
