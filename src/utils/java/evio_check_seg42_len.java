import org.jlab.coda.jevio.BaseStructure;
import org.jlab.coda.jevio.BaseStructureHeader;
import org.jlab.coda.jevio.DataType;
import org.jlab.coda.jevio.EvioEvent;
import org.jlab.coda.jevio.EvioReader;
import org.jlab.coda.jevio.StructureType;

public class evio_check_seg42_len {
    private static final int EXPECTED_TAG = 0x42;
    private static final int WARMUP_EVENTS = 20;

    private static final class Stats {
        long events;
        long anomalies;
        long malformed;
        int maxLen;
    }

    private static boolean isExpectedBankOfSegments(BaseStructure s) {
        return s != null &&
               s.getStructureType() == StructureType.BANK &&
               s.getHeader() != null &&
               s.getHeader().getDataType() != null &&
               DataType.isSegment(s.getHeader().getDataTypeValue());
    }

    private static void scanFile(String path) {
        Stats stats = new Stats();
        System.out.println("FILE " + path);

        EvioReader reader = null;
        try {
            reader = new EvioReader(path);
            EvioEvent event;
            while ((event = reader.parseNextEvent()) != null) {
                stats.events++;
                boolean show = stats.events <= WARMUP_EVENTS;

                try {
                    BaseStructure first = event.getChildAt(0);
                    if (!isExpectedBankOfSegments(first) || first.getChildCount() < 2) {
                        stats.malformed++;
                        System.out.println("  ev " + stats.events + " malformed layout");
                        continue;
                    }

                    BaseStructure second = first.getChildAt(1);
                    BaseStructureHeader header = second == null ? null : second.getHeader();
                    if (second == null || header == null ||
                        second.getStructureType() != StructureType.SEGMENT ||
                        header.getTag() != EXPECTED_TAG) {
                        stats.malformed++;
                        System.out.printf("  ev %d malformed seg2 tag=0x%x%n",
                                          stats.events, header == null ? 0 : header.getTag());
                        continue;
                    }

                    int len = header.getDataLength();
                    stats.maxLen = Math.max(stats.maxLen, len);
                    if (len >= 4) {
                        stats.anomalies++;
                        show = true;
                    }

                    if (show) {
                        System.out.println("  ev " + stats.events + " seg2(tag=0x42) dataLen=" + len);
                    }
                }
                catch (Exception e) {
                    stats.malformed++;
                    System.out.println("  ev " + stats.events + " malformed layout");
                }
            }
        }
        catch (Exception e) {
            System.err.println("  error: " + e.getMessage());
            return;
        }
        finally {
            if (reader != null) {
                try {
                    reader.close();
                }
                catch (Exception e) {
                    System.err.println("  close error: " + e.getMessage());
                }
            }
        }

        System.out.println("SUMMARY events=" + stats.events +
                           " anomalies=" + stats.anomalies +
                           " malformed=" + stats.malformed +
                           " maxDataLen=" + stats.maxLen);
        System.out.println();
    }

    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("Usage: evio_check_seg42_len file1.evio [file2.evio ...]");
            System.exit(1);
        }

        for (String path : args) {
            scanFile(path);
        }
    }
}
