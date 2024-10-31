
# Java Hipo Test Program Guide

---------------------------

    Short, minimal guide to test programs that were collected over the years and left as is.
    
    NO GUARANTEES as to functionality or effectiveness, just dig in and parse it yourself.

---------------------------
| File                                               | Function                                                                                                                                                                                                                                                                                                                              |
|----------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [ReadWriteTest.java](ReadWriteTest.java)           | Methods to generate byte array of short/int data, generate CODA prestart event. Also write file containing user header with Writer and WriterMT adding events as buffers & EvioNodes and writing entire records. Read back file.                                                                                                      |
| [RecordSupplyTester.java](RecordSupplyTester.java) | Test behavior of the RecordSupply and RecordRingItem classes.                                                                                                                                                                                                                                                                         |
| [TestBuilding.java](TestBuilding.java)             | Test performance of **internally** used RecordOutputStream. Read hall D data from hallDdataN.bin files in emu-3.3 or wherever else they are into a byte array, and use RecordOutputStream to add and format events. None of this code is relevant to users.                                                                           |
| [TestWriter.java](TestWriter.java)                 | Measure performance of using Writer to write LZ4 compressed data. Also write indentical data with Writer and WriterMT, read back and compare for accuracy. Also write 3 files with WriterMT with using 1, 2, or 3 compression threads. Caller can do **diff** on output files. Test RecordOutputStream's build() and reset() methods. |
|                                                    |                                                                                                                                                                                                                                                                                                                                       |
