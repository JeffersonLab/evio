----------------------------
# **EVIO 5.2 SOFTWARE PACKAGE**
----------------------------

EVIO stands for EVent Input/Output and contains libraries which read & write
data in its own unique format. It was created by the Data Acquisition (DAQ) group
and is now maintained by the Experimental Physics Software and Computing Infrastructure
(EPSCI) group at Thomas Jefferson National Accelerator Facility (Jefferson Lab).
It was originally used in the online DAQ as part of the CODA software,
but is now being used offline and in other applications as well.

There is a C library as well as a C++ library which is primarily a
wrapper around the C. There are a few utility programs included.
The Java library's capabilities extend far beyond those of
C and C++. It has a more extensive, user-friendly interface.
There is an additional package containing a GUI for looking at
evio format data at the byte level.

If you only plan to run C/C++ applications you can skip the Java
installation. If you only plan to use Java applications you can
you can skip the C/C++ installation.

**The home page is:**

  https://coda.jlab.org/drupal/content/event-io-evio/

**All code is contained in the github repository linked below.**
**_BE SURE TO CHECKOUT THE evio-5.2 BRANCH (git checkout evio-5.2):_**

  https://github.com/JeffersonLab/evio

**GUI package for viewing data in evio format:**

  https://coda.jlab.org/drupal/content/graphical-data-viewer

  https://github.com/JeffersonLab/JEventViewer
  
-----------------------------
## **Documentation**

Documentation is contained in the repository but may also be accessed at the home site:

Documentation Type | Link
------------ | -------------
PDF User's Guide | https://coda.jlab.org/drupal/content/evio-52-users-guide
Javadoc | https://coda.jlab.org/drupal/content/evio-52-javadoc
Doxygen doc for C++ | https://coda.jlab.org/drupal/content/evio-52-doxygen-c
Doxygen doc for C | https://coda.jlab.org/drupal/content/evio-52-doxygen-c-0

----------------------------
# **C LIBRARY**
----------------------------
The C library is called libevio.
It is a library with limited capabilities.

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
C++ evio is supported on both the MacOS and Linux platforms. C++ version 11 is used.

There are 2 different methods to build the C++ library and executables.
The first uses scons, a Python-based build software package which is available at https://scons.org.
The second uses cmake and make.


### Scons


To get a listing of all the local options available to the scons command,
run scons -h in the top-level directory to get this output:

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
    doc                      create doxygen and javadoc (in ./doc)
    undoc                    remove doxygen and javadoc (in ./doc)
    tar                      create tar file (in ./tar)
    
    Use scons -H for help about command-line options.


Although fairly self-explanatory executing:

    1. cd <evio dir>
    2. scons install
    
will compile and install all the code.
Note that for C/C++, only Linux and Darwin (Mac OSX) operating systems are supported.
By default, the libraries and executables are placed into the $CODA/<arch>/lib and bin subdirectories
(eg. Linux-x86_64/lib). If the command line options
–prefix, --incdir, --libdir, or –bindir are used, they take priority.
Be sure to change your LD_LIBRARY_PATH environmental variable to include the correct lib directory.


To compile a debug version, execute:

    scons install --dbg


### Cmake


Evio can also be compiled with cmake using the included CMakeLists.txt file.
To build the C and C++ libraries and executables:

    1. cd <evio dir>
    2. mkdir build
    3. cd build
    4. cmake .. –DCMAKE_BUILD_TYPE=Release
    5. make
       
    
To build only C code, place –DC_ONLY=1 on the cmake command line.
In order to compile all the examples as well, place –DMAKE_EXAMPLES=1 on the cmake command line.
The above commands will place everything in the current “build” directory and will keep generated
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
The libraries and executables are placed into the build/lib and build/bin subdirectories.
When doing an install, they are also placed into the [install dir]/[arch]/lib and bin subdirectories
(eg. Darwin-x86_64/lib). If cmake was run previously, remove the CMakeCache.txt file so
new values are generated and used.


To uninstall simply do:

    make uninstall


------------------------------
# **JAVA**
------------------------------


The current Java evio package, org.jlab.coda.jevio, was originally written by Dr. Dave Heddle of CNU
and was graciously given to the JLAB DAQ group for maintenance and continued development.
A large amount of additional work has been done since that time.
The code will compile using Java version 8 or later.

A pre-compiled jevio-5.2.jar file is found in each of these subdirectories:

  * java8, which contains a jar compiled with Java 8, and
  * java15 which contains a jar compiled with Java 15.
    
Using these allows the user to skip over all the following compilation instructions.


-----------------------------
## **Building**


The java evio uses ant to compile. To get a listing of all the options available to the ant command,
run ant help in the evio top level directory to get this output:

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


Although this is fairly self-explanatory, executing ant is the same as ant compile.
That will compile all the java. All compiled code is placed in the generated ./build directory.
If the user wants a jar file, execute ant jar to place the resulting file in the ./build/lib directory.
The java command in the user’s path will be the one used to do the compilation.
