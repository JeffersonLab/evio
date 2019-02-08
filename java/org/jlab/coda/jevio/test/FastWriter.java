package org.jlab.coda.jevio.test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.time.Duration;
import java.time.Instant;
import java.util.EnumSet;


/**
 * 
 */

/**
 * @author heyes
 *
 */
public class FastWriter {
	
	private FileChannel[] the_channel = new FileChannel[2];
	
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
	
	MappedByteBuffer buffer;

	public FileChannel createChan(String name) {
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
			return (FileChannel) Files
					  .newByteChannel(path, EnumSet.of(
							    StandardOpenOption.READ, 
							    StandardOpenOption.WRITE, 
							    StandardOpenOption.TRUNCATE_EXISTING)); //, StandardOpenOption.SYNC);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			return null;
		}
	}
	
	public FastWriter() {
		int bufferSize = 2000000000;     // 2GB
		self = Thread.currentThread();

        System.out.println("Startup: buffer size = " + bufferSize);

		ByteBuffer dataBuffer = ByteBuffer.allocateDirect(bufferSize);

		// Fill buffer with buffer number as a marker
		int data = 0xc0da1234;

		while (dataBuffer.remaining() >= 4) {
			dataBuffer.putInt(data);
		}
		// flip puts the buffer in a state where it can be read from
		dataBuffer.flip();

		the_channel[0] = createChan("test-write.dat");
		// the_channel[1] = createChan("test-write1.dat");
		
		for (int ix = 0; ix < 50; ix++) {
			
			try {
				buffer = the_channel[0].map(FileChannel.MapMode.READ_WRITE,
                                            ((long)ix)*bufferSize,
                                            bufferSize);
			} catch (IOException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}
			System.out.println("Start " + ix);
			// allocate and fill buffers
			Instant start = Instant.now();
			
			dataBuffer.rewind();
			
			buffer.put(dataBuffer);
			
			long duration = Duration.between(start, Instant.now()).toMillis();
			System.out.println("filling took " + duration + " ms");

			start = Instant.now();
			buffer.force();

			duration = Duration.between(start, Instant.now()).toMillis();
			System.out.println("force to disk took " + duration + " ms, = " + bufferSize/(1000*duration) + " MB/s");

			buffer.clear();
		}
		try {

			the_channel[0].close();
			// the_channel[1].close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		System.out.println("Done");
	}
			
		
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		
		new FastWriter();

	}

}
