#  **Java JARs Folder**

This folder contains any jar file dependencies required by the EVIO Java library that are not retrieved directly from the Maven repository. At present, only the disruptor library needs to be added from here.

Note that the disruptor 4.0.0 jar file included comes from a [JeffersonLab fork](https://github.com/JeffersonLab/disruptor) of the original lmax Java library. This forked repository adds an additional function (SpinCountBackoffWaitStrategy) that is required in the evio Java Library.