package org.jlab.coda.jevio.test;

import com.sun.scenario.effect.impl.sw.java.JSWBlend_SRC_OUTPeer;
import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.RandomAccessFile;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * In order to test file compression with our daq, we need our fake ROCs to contain
 * real data. This program mines real hall D data files for data and copies it into a local
 * byte array. This array can be used as a source by the emu's simulated ROC to approximate
 * Hall D data for compression test purposes.
 *
 * @author timmer
 * Date: May 24, 2019
 */
public class ExtractHallDdata {

    /**
     * Amount of bytes in hallDdata array.
     */
    static int arrayBytes = 16000000;

    /**
     * Put extracted data in here.
     */
    static byte[] hallDdata = new byte[arrayBytes];


    public static void main(String args[]) {
        //testParse();
        getRealDataFromDataFile();
    }


    /**
     * Method to help test algorithm to pull off last int on name.
     * @return
     */
    public static boolean testParse() {

        String name = "Roc17789";

        // Parse roc name
        Pattern pattern = Pattern.compile("^.+([0-9]{1})$", Pattern.CASE_INSENSITIVE);
        Matcher matcher = pattern.matcher(name);

        // Try to get the last digit at end of name if any
        int num;
        String numStr = null;
            if(matcher.matches()) {
            numStr = matcher.group(1);
        }
        else {
            System.out.println("getRealData: cannot find a single digit at end of ROC's name (" + name + ")");
            return false;
        }

        try {
            num = Integer.parseInt(numStr);
            System.out.println("FOund last int = " + num);
        }
        catch(NumberFormatException e) {
            System.out.println("getRealData: cannot find a single digit at end of ROC's name (" + name + ")");
            return false;
        }

        return true;
    }

    /**
     * Method to get real data from a Hall D data file and save it into
     * 9 files of 16 MB each.
     */
    static private void getRealDataFromDataFile() {

        // Number of files to create
        int fileCount = 9;

        // Amount of real data bytes to end up in each created file.
        arrayBytes = 16000000; // 16M

        // Put extracted real data for a single file in here
        hallDdata = new byte[arrayBytes];

        // File to read
        String fileName  = "/Volumes/USB30FD/hd_rawdata_042560_000.evio";
        File fileIn = new File(fileName);
        System.out.println("read ev file: " + fileName + " size: " + fileIn.length());

        try {
            // Read sequentially
            EvioReader fileReader = new EvioReader(fileName, false, true);
            EvioEvent event;

            // In this file, skip first 3 events, only go up to event #2415
            // since file was abnormally truncated as it was put on thumb drive.
            int j = 4;

            // Want 9 files, each with 16 MB of data, starting count = 1
            for (int k=1; k <= fileCount; k++) {

                // Reset for each file to write
                int bytesWritten = 0;
                boolean finishedFillingArray = false;

                // Output file name
                String destFileName = "/Users/timmer/coda/emu.GIT/hallDdata" + k + ".bin";

                for (; j < 2415; j++) {

                    // Top level bank or event is bank of banks.
                    // Under each data event, skip first bank which is trigger bank.
                    // The next banks are data banks each of which also contain banks.
                    // In most cases, the first sub banks is very small.
                    // Second sub bank contains lots of data. Grab this data and fill our container
                    // with it.

                    event = fileReader.parseEvent(j);
                    int eventKidCount = event.getChildCount();

                    // Start at one to skip trigger bank
                    for (int i = 1; i < eventKidCount; i++) {

                        BaseStructure bs = (BaseStructure) event.getChildAt(i);
                        int subEvKidCount = bs.getChildCount();

                        // Grab second bank under general data bank
                        if (subEvKidCount > 1) {

                            EvioBank bank = (EvioBank) (bs.getChildAt(1));
                            byte[] data = bank.getRawBytes();
                            int dataBytes = 4 * (bank.getHeader().getLength() - 1);

                            // Make sure we quit when array is full
                            int bytesToWrite = dataBytes;
                            if (bytesWritten + dataBytes > arrayBytes) {
                                bytesToWrite = arrayBytes - bytesWritten;
                                finishedFillingArray = true;
                            }

                            // Copy Hall D data into our array
                            System.arraycopy(data, 0, hallDdata, bytesWritten, bytesToWrite);
                            bytesWritten += bytesToWrite;

                            if (finishedFillingArray) break;
                        }
                    }

                    if (finishedFillingArray) break;
                }

                System.out.println("Write " + bytesWritten + " bytes to file: " + destFileName);

                RandomAccessFile file = new RandomAccessFile(destFileName, "rw");
                file.write(hallDdata, 0, bytesWritten);
                file.close();
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



}
