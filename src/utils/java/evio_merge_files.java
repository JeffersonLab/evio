import org.jlab.coda.jevio.*;
import org.jlab.coda.hipo.*;
import java.io.File;
import java.io.IOException;
import java.nio.ByteOrder;

public class evio_merge_files {
    public static void main(String[] args) throws IOException, EvioException {
        // Parse command-line arguments
        if (args.length < 1) {
            System.err.println("You must specify at least one input file!");
            printUsage();
            return;
        }
        String outputFile = "./merged.evio";  // default output name
        java.util.List<String> inputFiles = new java.util.ArrayList<>();
        for (String arg : args) {
            if (arg.startsWith("-h")) {
                printUsage();
                return;
            } 
			else if (arg.startsWith("-o")) {
                // If option is "-oOutputName", set output file name
                outputFile = arg.substring(2);
                System.out.println("Output file: " + outputFile);
                // prepend with "./" if directory not specified
                if (!outputFile.startsWith("./") && !outputFile.startsWith("/")) {
                    // System.out.println("Using current directory");
                    outputFile = "./" + outputFile;
                }
            } 
			else inputFiles.add(arg);
        }
        if (inputFiles.isEmpty()) {
            System.err.println("No input files specified.");
            printUsage();
            return;
        }

        // Print input files and their sizes
        for (String infile : inputFiles) {
            long sizeBytes = new File(infile).length();
            System.out.println("Input file: " + infile + " (size: " + sizeBytes + " bytes)");
        }

        // Retrieve dictionary from the first file (if any)
        String dictionaryXML = "";
        EvioReaderV4 dictReader = new EvioReaderV4(inputFiles.get(0));
        String xml = dictReader.getDictionaryXML();              // get dictionary if present:contentReference[oaicite:24]{index=24}
        if (xml != null) {
            dictionaryXML = xml;
            System.out.println("Dictionary found in first input file.");
        } 
        else System.out.println("No dictionary found in first input file.");
        dictReader.close();  // close dictionary reader

        // Set up EVIO EventWriter for the merged output
        int maxRecordBytes     = 1000000;
        int maxEventsPerRecord = 1000;
        int bufferBytes        = 1000000;
        EventWriterV4 writer = new EventWriterV4(
            outputFile, null, "", 1, 0L,
            maxRecordBytes, maxEventsPerRecord,
            bufferBytes,
            ByteOrder.nativeOrder(), null,
            null, true, false,
            null, 0, 1, 1, 1
        );

        // Process each input file: read all events and write to output
        int eventsRead = 0;
        int eventsWritten = 0;
        for (String filename : inputFiles) {
            EvioReaderV4 reader = new EvioReaderV4(filename);
                System.out.println("Opening input file: " + filename);
                EvioEvent event;
                while ((event = reader.parseNextEvent()) != null) {      // sequentially read events:contentReference[oaicite:25]{index=25}
                    // System.out.printf("Read event %d from %s%n", event.getEventNumber(), filename);
                    writer.writeEvent(event);                            // write event to output:contentReference[oaicite:26]{index=26}
                    eventsRead++;
                    eventsWritten++;
                }
                reader.close();  // close reader for this file
        }

        // Close writer and report
        writer.close();  // finalize output file:contentReference[oaicite:27]{index=27}
        System.out.println("Done. " + eventsRead + " events read, " + eventsWritten + " written.");
    }

    private static void printUsage() {
        System.out.println("\nUsage:");
        System.out.println("  java EvioMergeFiles [-oOutputfile] file1.evio file2.evio ...\n");
        System.out.println("Options:");
        System.out.println("  -oOutputfile   Set output filename (default: merged.evio)\n");
        System.out.println("This tool merges multiple EVIO files into one output file.");
    }
}
