#  **Java JARs Folder**

This folder contains jar files used for executing EVIO code using the Java API. The main jar file was made using Java 17.

It also includes dependencies required by the EVIO Java library that are not retrieved directly from the Maven repository. At present, only the disruptor dependency needs to be added from here. Note that the disruptor 4.0.0 jar file included comes from a [JeffersonLab fork](https://github.com/JeffersonLab/disruptor) of the original lmax Java library. This forked repository adds an additional function (SpinCountBackoffWaitStrategy) that is required in the evio Java Library.