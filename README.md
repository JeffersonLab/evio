----------------------------
# **EVIO 6.0 SOFTWARE PACKAGE**
----------------------------

EVIO stands for EVent Input/Output and contains libraries which read & write
data in its own unique format. It was created by the Data Acquisition (DAQ) group
and is now maintained by the Experimental Physics Software and Computing Infrastructure
(EPSCI) group at Thomas Jefferson National Accelerator Facility (Jefferson Lab).
It was originally used in the online DAQ as part of the CODA software,
but is now being used offline and in other applications as well.

There is a C library as well as an independent C++ library which
is a port from the Java version. There are a few utility programs included.
The Java version is a library with an extensive, user-friendly
interface. There is an additional package containing a GUI for looking at
evio format data at the byte level.

If you only plan to run C/C++ applications you can skip the Java
installation. If you only plan to use Java applications you can
you can skip the C/C++ installation.

**The home page is:**

  https://coda.jlab.org/drupal/content/event-io-evio/

**All code is contained in the github repository (default evio-6.0 branch):**

  https://github.com/JeffersonLab/evio

**GUI package for viewing data in evio format:**

  https://coda.jlab.org/drupal/content/graphical-data-viewer

  https://github.com/JeffersonLab/JEventViewer
  
# **Trial link**
   https://jeffersonlab.github.io/evio/

-----------------------------
## **Documentation**

Documentation is contained in the repository but may also be accessed at the home site:

Documentation Type | Link
------------ | -------------
PDF User's Guide | https://coda.jlab.org/drupal/content/evio-60-users-guide
Javadoc | https://coda.jlab.org/drupal/content/evio-60-javadoc
Doxygen doc for C | https://coda.jlab.org/drupal/content/evio-60-doxygen-c
Doxygen doc for C++ | https://coda.jlab.org/drupal/content/evio-60-doxygen-c-0

----------------------------
# **C LIBRARY**
----------------------------
The C library is called libevio.
It is a library with limited capabilities. In the past, this was acceptable because the evio
format was fairly simple. However, as its complexity has greatly expanded in this version, the C library will
be of very limited usefullness unless one is knowledgeable about all the intricacies of the format.

To compile it, follow the directions below for the C++ compilation which will include the C as well.
The C++ library is much more extensive in scope.
Having said that, the C library and executables can be compiled without any C++. This can be done in 2 ways:

    scons --C
or

    mkdir build
    cd build
    cmake .. –DC_ONLY=1


----------------------------
# **C++ LIBRARY**
----------------------------
The C++ library is called libeviocc.
The current C++ evio library is entirely different from the previous version (5.2) as it has been ported
from the Java code. This was done for a number of reasons.
First, a much more comprehensive C++ library was desired than was currently existing.
Second, it needed major, new capabilities such as being able to (un)compress data.
Third, it had to use a new format developed from the merging of Java evio version 4 and the Java HIPO library.
Finally, the author and maintainer of the previous code was no longer working at Jefferson Lab.
The simplest solution was to port the well-tested Java code which avoided having to redesign complex software
from scratch. C++ evio is supported on both the MacOS and Linux platforms. C++ version 11 is used,
and gcc version 5 or higher is required.


-----------------------------
## **Prerequisites**


### Disruptor


Evio depends upon the Disruptor-cpp software package available from a fork of the original package at github at

https://github.com/JeffersonLab/Disruptor-cpp
     
In terms of functionality, it is an ingenious, ultrafast ring buffer which was initially developed in Java
and then ported to C++. It’s extremely useful when splitting work among multiple threads and then recombining it.
To build it, do this on the Mac:

    1. git clone https://github.com/JeffersonLab/Disruptor-cpp.git
    2. cd Disruptor-cpp
    3. mkdir build
    4. cd build
    5. cmake .. -DCMAKE_BUILD_TYPE=Release
    6. make
    7. setenv DISRUPTOR_CPP_HOME <../>
    
If using Jefferson Lab’s Redhat Enterprise 7 do:
    
    1. git clone https://github.com/JeffersonLab/Disruptor-cpp.git
    2. cd Disruptor-cpp
    3. mkdir build
    4. cd build
    5. cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/apps/gcc/5.3.0/bin/gcc -DCMAKE_CXX_COMPILER=/apps/gcc/5.3.0/bin/g++
    6. make
    7. setenv DISRUPTOR_CPP_HOME <../>

Note that it requires GCC 5.0 / Clang 3.8 / C++14 or newer and boost.
Its shared library must be installed where evio can find it.
If not compiling on Jefferson Lab’s RHEL7, either your default compilers must meet this criteria
or you must specify the proper ones on the cmake command line.


### Boost


Besides the disruptor library, evio requires the boost libraries: boost_system, boost_thread, and boost_chrono.
   
   
### Lz4


Finally, evio depends on the lz4 library for compressing data in the lz4 and gzip formats.
If it isn’t already available on your machine, it can be obtained from the lz4 repository on github:

    1. git clone https://github.com/lz4/lz4.git
    2. cd lz4
    3. make
    4. make install


-----------------------------
## **Building**


There are 2 different methods to build the C++ library and executables.
The first uses scons, a Python-based build software package which is available at https://scons.org.
The second uses cmake and make. Also, be sure you’ve set the DISRUPTOR_CPP_HOME environmental variable.


### Scons


To get a listing of all the local options available to the scons command,
run _**scons -h**_ in the top-level directory to get this output:

    -D                       build from subdirectory of package
    local scons OPTIONS:
    --C                      compile C code only
    --dbg                    compile with debug flag
    --32bits                 compile 32bit libs & executables on 64bit system
    --prefix=<dir>           use base directory <dir> when doing install
    --incdir=<dir>           copy header files to directory <dir> when doing install
    --libdir=<dir>           copy library files to directory <dir> when doing install
    --bindir=<dir>           copy binary files to directory <dir> when doing install 
    install                  install libs, headers, and binaries
    install -c               uninstall libs, headers, and binaries
    doc                      create doxygen and javadoc (in ./docs)
    undoc                    remove doxygen and javadoc (in ./docs)
    tar                      create tar file (in ./tar)
    
    Use scons -H for help about command-line options.


Although fairly self-explanatory, if on Jefferson Lab's CUE system with Redhat 7, executing:

    1. use gcc/5.3.0
    2. cd <evio dir>
    3. scons install
    
will compile and install all the code.
Note that for C/C++, only Linux and Darwin (Mac OSX) operating systems are supported.
By default, the libraries and executables are placed into the _**$CODA/[arch]/lib**_ and _**bin**_ subdirectories
(eg. Linux-x86_64/lib). If the command line options
–prefix, --incdir, --libdir, or –bindir are used, they take priority.
Be sure to change your LD_LIBRARY_PATH environmental variable to include the correct lib directory.


To compile a debug version, execute:

    scons install --dbg


### Cmake


Evio can also be compiled with cmake using the included CMakeLists.txt file.
To build the C and C++ libraries and executables on the Mac:

    1. cd <evio dir>
    2. mkdir build
    3. cd build
    4. cmake .. –DCMAKE_BUILD_TYPE=Release
    5. make
    
If on Jefferson Lab's redhat 7 linux this will be:
    
    1. cd <evio dir>
    2. mkdir build
    3. cd build
    4. cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/apps/gcc/5.3.0/bin/gcc -DCMAKE_CXX_COMPILER=/apps/gcc/5.3.0/bin/g++
    5. make
    
    
To build only C code, place –DC_ONLY=1 on the cmake command line.
In order to compile all the examples as well, place –DMAKE_EXAMPLES=1 on the cmake command line.
The above commands will place everything in the current _**build**_ directory and will keep generated
files from mixing with the source and config files.


In addition to a having a copy in the build directory, installing the library, binary and include
files can be done by calling cmake in 2 ways:

    1. cmake .. –DCMAKE_BUILD_TYPE=Release –DCODA_INSTALL=<install dir>
    2. make install
    
or
    
    1. cmake .. –DCMAKE_BUILD_TYPE=Release
    2. make install

The first option explicitly sets the installation directory. The second option installs in the directory
given in the CODA environmental variable. If neither are defined, an error is given.
The libraries and executables are placed into the _**build/lib**_ and _**build/bin**_ subdirectories.
When doing an install, they are also placed into the _**[install dir]/[arch]/lib**_ and _**bin**_ subdirectories
(eg. Darwin-x86_64/lib). If cmake was run previously, remove the CMakeCache.txt file so
new values are generated and used.


To uninstall simply do:

    make uninstall


------------------------------
# **JAVA**
------------------------------


The current Java evio package, org.jlab.coda.jevio, was originally written by Dr. Dave Heddle of CNU
and was graciously given to the JLAB DAQ group for maintenance and continued development.
A large amount of additional work has been done since that time. As previously mentioned,
evio now uses a new format developed from the merging of evio version 4 and the HIPO library.
The code will compile using Java version 8 or later.

The jar files necessary to compile an evio jar file are in the java/jars directory.
They are compiled with Java 8. In addition, there are 2 subdirectories:

  * java8, which contains all such jars compiled with Java 8, and
  * java15 which contains all jars compiled with Java 15.
    
If a jar file is not available in Java 15 use the Java 8 version.

A pre-compiled _**jevio-6.0.jar**_ file is found in each of these subdirectories.
Using these allows the user to skip over all the following compilation instructions.

 
-----------------------------
## **Prerequisites**


### Disruptor


Evio depends upon the LMAX-Exchange/disruptor software package available from github whose fork is at:

https://github.com/JeffersonLab/disruptor
        
In terms of functionality, it is an ingenious, ultrafast ring buffer which was initially developed
for use the in the commodities exchange markets. It’s extremely useful when splitting work among
multiple threads and then recombining it. Although there are many branches in this git repository,
only 2 branches have had the necessary changes to be compatible with CODA.
These are the master and v3.4 branches. The v3.4 branch should be compiled with Java 8
(it does not compile with Java 15) while the master requires at least Java 11.

The disruptor software is provided in the _**java/jars/disruptor-3.4.3.jar**_ file (compiled with Java 8).
However, to generate this file yourself, get the disruptor software package by simply doing the following:

    1.	git clone https://github.com/JeffersonLab/disruptor.git
    2.	cd disruptor
    3.	git checkout v3.4
    4.	./gradlew

The resulting disruptor jar file, disruptor-3.4.3.jar, will be found in the disruptor package’s build/libs subdirectory.

One can also use the master branch which needs to be compiled with Java version 11 or greater and produces disruptor-4.0.0.jar.
Currently this has been created with java15 and is in the java/jars/java15 directory. Here is how to generate it:

    1.	git clone https://github.com/JeffersonLab/disruptor.git
    2.	cd disruptor
    3.	./gradlew

The resulting jar will be in build/libs as before.


### Lz4


A lz4 data compression software is provided in the _**java/jars/lz4-java-1.8.0.jar**_ file (compiled with Java 8).
Although this is available in various versions and locations on the web, one can generate this from its source which is the
lz4/lz4-java repository on github:

    1.	git clone https://github.com/lz4/lz4-java.git
    2.	cd lz4-java
    3.	ant ivy-bootstrap
    4.	ant submodule init
    5.	ant submodule update
    6.	ant

Generated jar files will be in dist subdirectory.

Another jar file, AHACompressionAPI.jar, also in the the java/jars directory, is for use in Compressor.java
when using the AHA374 FPGA data compression board for gzip compression in hardware.
This is an effort that never took off since LZ4 compresssion was so much more efficient.
Thus, it may be safely ignored or removed.


-----------------------------
## **Building**


The java evio uses ant to compile. To get a listing of all the options available to the ant command,
run _**ant help**_ in the evio top level directory to get this output:

    help: 
        [echo] Usage: ant [ant options] <target1> [target2 | target3 | ...]
    
        [echo]      targets:
        [echo]      help        - print out usage
        [echo]      env         - print out build file variables' values
        [echo]      compile     - compile java files
        [echo]      clean       - remove class files
        [echo]      cleanall    - remove all generated files
        [echo]      jar         - compile and create jar file
        [echo]      install     - create jar file and install into 'prefix'
        [echo]                    if given on command line by -Dprefix=dir',
        [echo]                    else install into CODA if defined
        [echo]      uninstall   - remove jar file previously installed into 'prefix'
        [echo]                    if given on command line by -Dprefix=dir',
        [echo]                    else installed into CODA if defined
        [echo]      all         - clean, compile and create jar file
        [echo]      javadoc     - create javadoc documentation
        [echo]      developdoc  - create javadoc documentation for developer
        [echo]      undoc       - remove all javadoc documentation
        [echo]      prepare     - create necessary directories


Although this is fairly self-explanatory, executing _**ant**_ is the same as ant compile.
That will compile all the java. All compiled code is placed in the generated _**build**_ directory.
If the user wants a jar file, execute _**ant jar**_ to place the resulting file in the _**build/lib**_ directory.
The java command in the user’s path will be the one used to do the compilation.
