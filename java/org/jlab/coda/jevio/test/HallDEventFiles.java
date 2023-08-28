package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;

/**
 * Test program.
 * @author timmer
 * Date: Aug 24, 2023
 */
public class HallDEventFiles {



    /** For writing out a 5GByte file. */
    public static void main(String args[]) {


        boolean debug = false;

        String fileName = "/daqfs/home/timmer/evioDataFiles/clas_006586.evio.00001";
        String basename = "ev_";

        System.out.println("Create reading object for file = " + fileName);


        EvioEvent event;
        long totalBytes = 0;


            System.out.println("read ev file: " + fileName);
            try {
                EvioReader fileReader = new EvioReader(fileName, false, false);

                int counter = 1;

                while ( (event = fileReader.nextEvent()) != null) {
                     int evBytes = event.getTotalBytes();
                     totalBytes += evBytes;
                     System.out.println("Ev " + counter + " is " + evBytes);

                     if (false) {
                         byte[] b = new byte[evBytes];
                         ByteBuffer buf = ByteBuffer.wrap(b);

                         // Write out event, first header, then data
                         BankHeader bhead = (BankHeader)event.getHeader();
                         byte[] dataArray = event.getRawBytes();

                         // first header
                         bhead.write(buf);
                         // then data
                         buf.put(dataArray);
                         buf.flip();

                         String fname = basename + counter;
                         Utilities.bufferToFile(fname, buf, true, false);
                     }

                    if (counter >= 2696) {
                      //  break;
                    }

                    counter++;
                }
            }
            catch (IOException e) {
                e.printStackTrace();
            }
            catch (EvioException e) {
                e.printStackTrace();
            }


        System.out.println("Total bytes = " + totalBytes);

//        reader.rewind();

//        avgBufBytes = totalBytes / counter;
//        std::cerr << "Data byte total = " << totalBytes << ", processed events = " << counter << ", avg buf size = " << avgBufBytes << std::endl;
//        std::cerr << "Big ev data byte total = " << totalBigEvBytes << ", big events = " << bigEventCnt << std::endl;

//        bufsSent = 0;



//        while (false) {

//            if (bufsSent >= counter) {
//                break;
//            }

//            auto ev = reader.nextEvent();
//            if (ev == nullptr) {
//                break;
//            }
//            std::vector <uint8_t> dataVec = ev->getRawBytes();


//            uint32_t bytes = ev->getTotalBytes();
//            bufsSent++;


//            char *buf = (char *)(&dataVec[0]);

            // Write out file with all times and levels
//            std::string fileName = basename + std::to_string(bufsSent);
//            FILE *fp = fopen (fileName.c_str(), "w");
//            fwrite(buf,bytes,1,fp);
//            fclose(fp);

            // Do this for the /daqfs/java/clas_005038.1231.hipo  file
////            if (bufsSent > 100) {
//                break;
//            }
//            fprintf(stderr, "wrote event %d, size %u to file %s\n", bufsSent, bytes, fileName.c_str());
//
//        }




    }


}
