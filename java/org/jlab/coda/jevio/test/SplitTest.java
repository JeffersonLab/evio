package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;


/**
 * Test program for file splitting.
 * @author timmer
 * Date: Aug 29, 2013
 */
public class SplitTest {


    /** Look at split names. */
    public static void main(String args[]) {

        try {
            // $(FILE_ENV) demonstrates substituting for environmental variables so define FILE_ENV in your environment.
            // %s substitutes for run type
            // 1st int specifier substitutes for run number
            // 2nd int specifier substitutes for stream id
            // 3rd int specifier substitutes for split number
            String baseName = "my_$(FILE_ENV)_%s_run#%d_streamId#%d_.%06d";
            String runType = "MyRunType";
            int runNumber = 2;
            // Split file every 20 MB
            long split = 20000000;

            // Split file number starts at 0
            int splitCount = 0;
            int streamId = 3;
            int streamCount = 66;

            // The following may not be backwards compatible.
            // Make substitutions in the baseName to create the base file name.
            StringBuilder builder = new StringBuilder(100);
            int specifierCount = Utilities.generateBaseFileName(baseName, runType, builder);
            String baseFileName = builder.toString();

            System.out.println("BaseName = " + baseName);
            System.out.println("BaseFileName = " + baseFileName);
            System.out.println("Specifier Count = " + specifierCount);

            // Also create the file names with more substitutions, one for each of 10 splits
            for (int i=0; i<10; i++) {
                String fileName = Utilities.generateFileName(baseFileName, specifierCount,
                        runNumber, split, splitCount++, streamId, streamCount);
                System.out.println("  Filename for split " + i + " = " + fileName);
            }
        }
        catch (EvioException e) {
            e.printStackTrace();
        }


    }

}
