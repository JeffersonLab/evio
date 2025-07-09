# **EVIO 6 SOFTWARE PACKAGE**

EVIO stands for EVent Input/Output, a unique data format developed by Jefferson Lab. 
It was created by the Data Acquisition (DAQ) group and is maintained by the 
Experimental Physics Software and Computing Infrastructure (EPSCI) group at Thomas 
Jefferson National Accelerator Facility (JLab).

This software repository allows one to read & write `.evio` and `.ev` format data, 
within either a C/C++ or Java programming environment.

# **Getting Started**

## **C/C++ Library**

To build C/C++ code from this repository:

    git clone https://github.com/JeffersonLab/evio/
    cd evio; mkdir build
    cmake -S . -B build
    cmake --build build --parallel

Note that during the cmake configure step (first of two `cmake` commands above), one can
toggle the following special flags:

* `C_ONLY` : build C lib only, skip C++ (default `-DC_ONLY=0`)
* `MAKE_EXAMPLES`: build example/test programs (default `-DMAKE_EXAMPLES=0`)
* `USE_FILESYSTEMLIB`: ue C++17 <filesystem> instead of Boost (default `-DUSE_FILESYSTEMLIB=0`)
* `DISRUPTOR_FETCH`: allow CMake to download Disruptor if not found (default `-DDISRUPTOR_FETCH=1`)

One can still also use `scons` instead of cmake to build the evio C/C++ library, though this feature
will not be supported in future releases.

### Prerequisites

C++ 17 or higher, `cmake`, `lz4`, `boost_system`, `boost_thread`, and `boost_chrono`. If LZ4 is not
already configured, it can be installed from [LZ4 on github](https://github.com/lz4/lz4). The boost
libraries are typically system-specific. 

## **Java Library**

The Java version of evio (internally `org.jlab.coda.jevio`) can also be used for reading & writing
`.evio` format files. A "fat" jar file with all dependencies is included in the `java/jars` folder,
which should be all most users need to run and execute Java code utilizing the evio library.

Java 17 is the default version used, however the Java evio library should be compatible with all
java versions 8 and higher (note this has not been rigorously tested). If one wants to create a 
new jar file for any reason (e.g. to modify Java versions), do:

    git clone https://github.com/JeffersonLab/evio/
    cd evio
    mvn clean install

### Prerequisites

Requires Maven (`mvn`) and an installation of Java on your system. 

**Running on "ifarm" at JLab will not work unless you install java yourself**. Note that the default java versions on the farm will be too old to 
work. See downloads from [OpenJDK](https://openjdk.org/install/) or [Oracle](https://www.oracle.com/java/technologies/javase/jdk17-archive-downloads.html).

-----------------------------

# **Useful Links**

----------------------------

Documentation on GitHub:

* [All Documentation](https://jeffersonlab.github.io/evio)
* [User's Guide PDF](https://jeffersonlab.github.io/evio/doc-6.0/users_guide/evio_Users_Guide.pdf)
* [EVIO Data Format Reference](https://jeffersonlab.github.io/evio/doc-6.0/format_guide/evio_Formats.pdf)

Software Library Documentation:

* [Javadoc for Java Library](https://jeffersonlab.github.io/evio/doc-6.0/javadoc/index.html)
* [Doxygen for C Library](https://jeffersonlab.github.io/evio/doc-6.0/doxygen/C/html/index.html)
* [Doxygen for C++ Libary](https://jeffersonlab.github.io/evio/doc-6.0/doxygen/CC/html/index.html)

Other Links:
* [EVIO Event Viewer on GitHub](https://github.com/JeffersonLab/JEventViewer)


The EVIO-6 data format is closely related to the HIPO data format, following the same data
framework to the record level. Beyond this level, they differ in the way data is stored as
well as the software used to read and write to these respective `.evio` and `.hipo` formats.
More information on the HIPO data format can be found at https://github.com/gavalian/hipo,
or from the CLAS12 Software Project Coordinator.

----------------------------

# **Copyright**

----------------------------

For any issues regarding use and copyright, read the [license](LICENSE.txt) file.
