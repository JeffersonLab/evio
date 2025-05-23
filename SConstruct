################################
# scons build system file
################################
#
# Much of this file can be used as a boilerplate build file.
#
# The parts that are dependent on the details of the software
# package being compiled are:
#
#   1) the name and version of this software package
#   2) the needed header files and libraries
#   3) what type of documentation needs to be installed
#   4) the scons files to be run in the source directories
#
################################

# Get various python modules
import re, sys, os, string, subprocess, SCons.Node.FS
from subprocess import Popen, PIPE
from os import sep, symlink

# Useful python functions (previously in py)
def recursiveDirs(root) :
    """Return a list of all subdirectories of root, top down,
       including root, but without .svn and .<arch> directories"""
    return filter( (lambda a : (a.rfind( ".svn")==-1) and \
                               (a.rfind( ".Linux")==-1) and \
                               (a.rfind( ".SunOS")==-1) and \
                               (a.rfind( ".Darwin")==-1) and \
                               (a.rfind( ".vxworks")==-1)),  [ a[0] for a in os.walk(root)]  )



def unique(list) :
    """Remove duplicates from list"""
    return dict.fromkeys(list).keys()



def scanFiles(dir, accept=["*.cpp"], reject=[]) :
    """Return a list of selected files from a directory & its subdirectories"""
    sources = []
    paths = recursiveDirs(dir)
    for path in paths :
        for pattern in accept :
            sources+=glob.glob(path+"/"+pattern)
    for pattern in reject :
        sources = filter( (lambda a : a.rfind(pattern)==-1 ),  sources )
    return unique(sources)



def subdirsContaining(root, patterns):
    """Return a list of subdirectories containing files of the given pattern"""
    dirs = unique(map(os.path.dirname, scanFiles(root, patterns)))
    dirs.sort()
    return dirs



###################
# Operating System
###################

def CheckHas64Bits(context, flags):
    """Define configure-type test function to
       see if operating system is 64 or 32 bits"""
       
    context.Message( 'Checking for 64/32 bits ...' )
    lastCCFLAGS = context.env['CCFLAGS']
    lastLFLAGS  = context.env['LINKFLAGS']
    context.env.Append(CCFLAGS = flags, LINKFLAGS = flags)
    # (C) program to run to check for bits
    ret = context.TryRun("""
#include <stdio.h>
int main(int argc, char **argv) {
  printf(\"%d\", 8*sizeof(0L));
  return 0;
}
""", '.c')
    # Restore original flags
    context.env.Replace(CCFLAGS = lastCCFLAGS, LINKFLAGS = lastLFLAGS)
    # If program successfully ran ...
    if ret[0]:
        context.Result(ret[1])
        if ret[1] == '64':
            return 64
        return 32
    # Else if program did not run ...
    else:
        # Don't know if it's a 32 or 64 bit operating system
        context.Result('failed')
        return 0



def is64BitMachine(env, platform, machine):
    """Determine if machine has 64 or 32 bit architecture"""
    if platform == 'Linux' and machine == 'x86_64':
            return True
    else:
        ccflags = ''
        if platform == 'SunOS':
            ccflags = '-xarch=amd64'
        
        # Run the test
        conf = Configure( env, custom_tests = { 'CheckBits' : CheckHas64Bits } )
        ret = conf.CheckBits(ccflags)
        env = conf.Finish()
        if ret < 1:
            print ('Cannot run test, assume 64 bit system')
            return True
        elif ret == 64:
            # Test shows 64 bit system'
            return True
        else:
            # Test shows 32 bit system'
            return False



def configure32bits(env, use32bits, platform, machine):
    """Setup environment on 64 bit machine to handle 32 or 64 bit libs and executables."""
    if platform == 'SunOS':
        if not use32bits:
            if machine == 'sun4u':
                env.Append(CCFLAGS =   ['-xarch=native64', '-xcode=pic32'],
                           LINKFLAGS = ['-xarch=native64', '-xcode=pic32'])
            else:
                env.Append(CCFLAGS =   ['-xarch=amd64'],
                           LINKFLAGS = ['-xarch=amd64'])

    elif platform == 'Darwin':
        if not use32bits:
            env.Append(CCFLAGS =   [],
                       LINKFLAGS = ['-Wl', '-bind_at_load'])

    elif platform == 'Linux':
        if use32bits:
            env.Append(CCFLAGS = ['-m32'], LINKFLAGS = ['-m32'])

    return



###########################
# Installation Directories
###########################

def getInstallationDirs(osname, prefix, incdir, libdir, bindir):
    """Determine installation directories."""
    
    # Get CODA env variable if any         
    codaHomeEnv = os.getenv('CODA','')
    
    # The installation directory is the user-specified "prefix"
    # by first choice, "CODA" secondly.
    # Any user specified command line installation path overrides default
    if (prefix == None) or (prefix == ''):
        # prefix not defined try CODA env var
        if codaHomeEnv == "":
            if (incdir == None) or (libdir == None) or (bindir == None):
                print ("\nNeed to define CODA, or use the --prefix option,")
                print ("or all the --incdir, --libdir, and --bindir options.\n")
                raise SystemExit
        else:
            prefix = codaHomeEnv
    
    osDir = prefix + "/" + osname
    
    # Set our install directories
    if incdir != None:
        incDir = incdir
        archIncDir = incdir
    else:
        incDir = prefix + '/common/include'
        archIncDir = osDir + '/include'
    
    if libdir != None:
        libDir = libdir
    else:
        libDir = osDir + '/lib'
    
    if bindir != None:
        binDir = bindir
    else:
        binDir = osDir + '/bin'
    
    # Return absolute paths in list
    return [os.path.abspath(prefix),
            os.path.abspath(osDir),
            os.path.abspath(incDir),
            os.path.abspath(archIncDir),
            os.path.abspath(libDir),
            os.path.abspath(binDir)]



###########################
# Include File Directories
###########################

def makeIncludeDirs(includeDir, archIncludeDir, archDir, archIncLocalLink):
    """Function to make include directories"""
    #
    # Create main include dir if it doesn't exist
    #
    if not os.path.exists(includeDir):
        os.makedirs(includeDir)
    # Make sure it's a directory (if we didn't create it)
    elif not os.path.isdir(includeDir):
        print
        print ("Error:", includeDir, "is NOT a directory")
        print
        raise SystemExit

    if includeDir == archIncludeDir:
        return

    #
    # If an install has never been done, the arch dir needs
    # to be made first so the symbolic link can be created.
    #
    if not os.path.exists(archDir):
        try:
            os.makedirs(archDir)
        except OSError:
            return
    elif not os.path.isdir(archDir):
        print
        print ("Error:", archDir, "is NOT a directory")
        print
        raise SystemExit

    #
    # If the architecture include dir does NOT exist,
    # make link to include dir
    #
    if not os.path.exists(archIncludeDir):
        # Create symbolic link: symlink(source, linkname)
        try:
            if (archIncLocalLink == None) or (archIncLocalLink == ''):
                symlink(includeDir, archIncludeDir)
            else:
                symlink(archIncLocalLink, archIncludeDir)
        except OSError:
            # Failed to create symbolic link, so
            # just make it a regular directory
            os.makedirs(archIncludeDir)
    elif not os.path.isdir(archIncludeDir):
        print
        print ("Error:", archIncludeDir, "is NOT a directory")
        print
        raise SystemExit

    return


 
###########
# JAVA JNI
###########

def configureJNI(env):
    """Configure the given environment for compiling Java Native Interface
       c or c++ language files."""

    # first look for a shell variable called JAVA_HOME
    java_base = os.environ.get('JAVA_HOME')
    if not java_base:
        if sys.platform == 'darwin':
            # Apple's OS X has its own special java base directory
            java_base = '/System/Library/Frameworks/JavaVM.framework'
        else:
            # Search for the java compiler
            print ("JAVA_HOME environment variable not set. Searching for javac to find jni.h ...")
            if not env.get('JAVAC'):
                print ("The Java compiler must be installed and in the current path, exiting")
                return 0
            jcdir = os.path.dirname(env.WhereIs('javac'))
            if not jcdir:
                print ("   not found, exiting")
                return 0
            # assuming the compiler found is in some directory like
            # /usr/jdkX.X/bin/javac, java's home directory is /usr/jdkX.X
            java_base = os.path.join(jcdir, "..")
            print ("  found, dir = " + java_base)
        
    if sys.platform == 'darwin':
        # Apple does not use Sun's naming convention
        java_headers = [os.path.join(java_base, 'Headers')]
        java_libs = [os.path.join(java_base, 'Libraries')]
    else:
        # linux
        java_headers = [os.path.join(java_base, 'include')]
        java_libs = [os.path.join(java_base, 'lib')]
        # Sun's windows and linux JDKs keep system-specific header
        # files in a sub-directory of include
        if java_base == '/usr' or java_base == '/usr/local':
            # too many possible subdirectories. Just use defaults
            java_headers.append(os.path.join(java_headers[0], 'linux'))
            java_headers.append(os.path.join(java_headers[0], 'solaris'))
        else:
            # add all subdirs of 'include'. The system specific headers
            # should be in there somewhere
            java_headers = recursiveDirs(java_headers[0])

    # add Java's include and lib directory to the environment
    env.Append(CPPPATH = java_headers)
    # (only need the headers right now so comment out next line)
    #env.Append(LIBPATH = java_libs)

    # add any special platform-specific compilation or linking flags
    # (only need the headers right now so comment out next lines)
    #if sys.platform == 'darwin':
    #    env.Append(SHLINKFLAGS = '-dynamiclib -framework JavaVM')
    #    env['SHLIBSUFFIX'] = '.jnilib'

    # Add extra potentially useful environment variables
    #env['JAVA_HOME']   = java_base
    #env['JNI_CPPPATH'] = java_headers
    #env['JNI_LIBPATH'] = java_libs

    return 1




###################
# Documentation
###################

def generateDocs(env, doCPP=False, doC=False, doJava=False):
    """Generate and install generated documentation (doxygen & javadoc)."""

    if doCPP:
        print ('Call generateDocs() for C++')
        # remove target files so documentation always gets rebuilt
        rmcmd = 'rm -fr doc/doxygen/CC'
        os.popen(rmcmd).read()

        def docGeneratorCC(target, source, env):
            cmd = 'doxygen doc/doxygen/DoxyfileCC'
            Popen(cmd, shell=True, env={"TOPLEVEL": "./"}, stdout=None)
            return
        
        docBuildCC = Builder(action = docGeneratorCC)
        env.Append(BUILDERS = {'DocGenCC' : docBuildCC})
        
        env.Alias('doc', env.DocGenCC(target = ['#/doc/doxygen/CC/html/index.html'],
                                      source = scanFiles("src/libsrc++", accept=["*.cpp", "*.h"])))


    if doC:
        rmcmd = 'rm -fr doc/doxygen/C'
        os.popen(rmcmd).read()

        def docGeneratorC(target, source, env):
            cmd = 'doxygen doc/doxygen/DoxyfileC'
            Popen(cmd, shell=True, env={"TOPLEVEL": "./"}, stdout=None)
            return

        docBuildC = Builder(action = docGeneratorC)
        env.Append(BUILDERS = {'DocGenC' : docBuildC})

        env.Alias('doc', env.DocGenC(target = ['#/doc/doxygen/C/html/index.html'],
                                      source = scanFiles("src/libsrc", accept=["*.c", "*.h"])))


    if doJava:
        rmcmd = 'rm -fr doc/javadoc'
        os.popen(rmcmd).read()

        def docGeneratorJava(target, source, env):
            cmd = 'ant javadoc'
            os.popen(cmd).read()
            return
        
        docBuildJava = Builder(action = docGeneratorJava)
        env.Append(BUILDERS = {'DocGenJava' : docBuildJava})
        
        env.Alias('doc', env.DocGenJava(target = ['#/doc/javadoc/index.html'],
            source = scanFiles("java/org/jlab/coda", accept=["*.java"]) ))
    
    return 1




def removeDocs(env):
    """Remove all generated documentation (doxygen & javadoc)."""

    def docRemover(target, source, env):
        cmd = 'rm -fr doc/javadoc doc/doxygen/CC doc/doxygen/C'
        output = os.popen(cmd).read()
        return
    
    docRemoveAll = Builder(action = docRemover)
    env.Append(BUILDERS = {'DocRemove' : docRemoveAll})
    
    # remove documentation
    env.Alias('undoc', env.DocRemove(target = ['#/.undoc'], source = None))
    
    return 1



###################
# Tar file
###################

def generateTarFile(env, baseName, majorVersion, minorVersion):
    """Generate a gzipped tar file of the current directory."""
                
    # Function that does the tar. Note that tar on Solaris is different
    # (more primitive) than tar on Linux and MacOS. Solaris tar has no -z option
    # and the exclude file does not allow wildcards. Thus, stick to Linux for
    # creating the tar file.
    def tarballer(target, source, env):
        dirname = os.path.basename(os.path.abspath('.'))
        cmd = 'tar -X tar/tarexclude -C .. -c -z -f ' + str(target[0]) + ' ./' + dirname
        pipe = Popen(cmd, shell=True, stdin=PIPE).stdout
        return pipe

    # name of tarfile (software package dependent)
    tarfile = 'tar/' + baseName + '-' + majorVersion + '.' + minorVersion + '.tgz'

    # Use a Builder instead of env.Tar() since I can't make that work.
    # It runs into circular dependencies since we copy tar file to local
    # ./tar directory
    tarBuild = Builder(action = tarballer)
    env.Append(BUILDERS = {'Tarball' : tarBuild})
    env.Alias('tar', env.Tarball(target = tarfile, source = None))

    return 1


class color:
   PURPLE = '\033[95m'
   CYAN = '\033[96m'
   DARKCYAN = '\033[36m'
   BLUE = '\033[94m'
   GREEN = '\033[92m'
   YELLOW = '\033[93m'
   RED = '\033[91m'
   BOLD = '\033[1m'
   UNDERLINE = '\033[4m'
   END = '\033[0m'

# Created files & dirs will have this permission
os.umask(0o2)

# Software version
versionMajor = '6'
versionMinor = '0'

# Determine the os and machine names
uname    = os.uname();
platform = uname[0]
machine  = uname[4]
osname   = os.getenv('CODA_OSNAME', platform + '-' +  machine)

# Create an environment while importing the user's PATH & LD_LIBRARY_PATH.
# This allows us to get to other compilers for example.
path = os.getenv('PATH', '')
ldLibPath = os.getenv('LD_LIBRARY_PATH', '')

if path == '' or path == None:
    print
    print ("Error: set PATH environmental variable")
    print
    raise SystemExit

if ldLibPath == '' or ldLibPath == None:
    print
    print ("Warning: LD_LIBRARY_PATH environmental variable not defined")
    print
    env = Environment(ENV = {'PATH' : os.environ['PATH']})
else:
    env = Environment(ENV = {'PATH' : os.environ['PATH'], 'LD_LIBRARY_PATH' : os.environ['LD_LIBRARY_PATH']})


################################
# 64 or 32 bit operating system?
################################
    
# How many bits is the operating system?
# For Linux 64 bit x86 machines, the "machine' variable is x86_64,
# but for Darwin or Solaris there is no obvious check so run
# a configure-type test.
is64bits = is64BitMachine(env, platform, machine)
if is64bits:
    print ("We're on a 64-bit machine")
else:
    print ("We're on a 32-bit machine")


#############################################
# Command line options (view with "scons -h")
#############################################

Help('\n-D                  build from subdirectory of package')
Help('\n-c                  clean dir\n')
Help('\nlocal scons OPTIONS:\n')

# C compilation only option
AddOption('--C', dest='onlyC', default=False, action='store_true')
onlyC = GetOption('onlyC')
if onlyC: print ("Compile C code only")
Help('--C                 compile C code only\n')

# debug option
AddOption('--dbg', dest='ddebug', default=False, action='store_true')
debug = GetOption('ddebug')
if debug: print ("Enable debugging")
Help('--dbg               compile with debug flag\n')

# 32 bit option
AddOption('--32bits', dest='use32bits', default=False, action='store_true')
use32bits = GetOption('use32bits')
if use32bits: print ("use 32-bit libs & executables even on 64 bit system")
Help('--32bits            compile 32bit libs & executables on 64bit system\n')

# install directory option
AddOption('--prefix', dest='prefix', nargs=1, default='', action='store')
prefix = GetOption('prefix')
Help('--prefix=<dir>      use base directory <dir> when doing install\n')

# include install directory option
AddOption('--incdir', dest='incdir', nargs=1, default=None, action='store')
incdir = GetOption('incdir')
Help('--incdir=<dir>      copy header  files to directory <dir> when doing install\n')

# library install directory option
AddOption('--libdir', dest='libdir', nargs=1, default=None, action='store')
libdir = GetOption('libdir')
Help('--libdir=<dir>      copy library files to directory <dir> when doing install\n')

# binary install directory option
AddOption('--bindir', dest='bindir', nargs=1, default=None, action='store')
bindir = GetOption('bindir')
Help('--bindir=<dir>      copy binary  files to directory <dir> when doing install\n')

#########################
# System checks
#########################

conf = Configure(env)
if not conf.CheckCHeader('lz4.h'):
    if not onlyC:
        print('lz4 must be installed!')
        Exit(1)
else:
    print('lz4 was found')

env = conf.Finish()

# location of C++ version of disruptor
disruptorHome = os.getenv('DISRUPTOR_CPP_HOME')
if disruptorHome == "" or not os.path.exists(str(disruptorHome)):
    if not onlyC:
        print ("Error: set DISRUPTOR_CPP_HOME environmental variable")
        Exit(1)
else:
    print('Disruptor-cpp = ' + str(disruptorHome))

#########################
# Compile flags
#########################

print (subprocess.call(["gcc", "-v"]) )

# Debug/optimization flags
debugSuffix = ''
if debug:
    debugSuffix = '-dbg'
# Compile with -g and add debugSuffix to all executable names
    env.Append(CCFLAGS = ['-g'], PROGSUFFIX = debugSuffix)

# code for newlib++ written in C++17
env.Append(CXXFLAGS = ['-std=c++17'])

# Take care of 64/32 bit issues
if is64bits:
    # Setup 64 bit machine to compile either 32 or 64 bit libs & executables
    configure32bits(env, use32bits, platform, machine)
elif not use32bits:
    use32bits = True


execLibs = ['']


# Platform dependent quantities. Default to standard Linux libs.
# -lstdc++fs is a library needed for the experimental <filesystem> header in C++17
if onlyC:
    execLibs = ['pthread', 'expat', 'z', 'dl', 'm']
else:
    execLibs = ['stdc++fs', 'pthread', 'expat', 'z', 'lz4', 'dl', 'm']
    #execLibs = ['pthread', 'expat', 'z', 'dl', 'm']

env.AppendUnique(CPPDEFINES = ['USE_GZIP'])

if platform == 'Darwin':
    if onlyC:
        execLibs = ['pthread', 'expat', 'z']
    else:
        execLibs = ['pthread', 'expat', 'z', 'lz4']

    env.Append(CPPDEFINES = ['Darwin'], SHLINKFLAGS = ['-undefined','dynamic_lookup'])
    #env.Append(CPPDEFINES = ['Darwin'], SHLINKFLAGS = ['-multiply_defined', '-undefined', '-flat_namespace'])
    env.Append(CCFLAGS = ['-fmessage-length=0'])


if is64bits and use32bits:
    osname = osname + '-32'

print ("OSNAME = "+osname)

# hidden sub directory into which variant builds go
archDir = '.' + osname + debugSuffix


#########################
# Install stuff
#########################

libInstallDir     = ""
binInstallDir     = ""
incInstallDir     = ""
archIncInstallDir = ""

# If a specific installation dir for includes is not specified
# on the command line, then the architecture specific include
# dir is a link to local ../common/include directory.
archIncLocalLink = None
if (incdir == None):
    archIncLocalLink = '../common/include'

# If we going to install ...
if 'install' in COMMAND_LINE_TARGETS:
    # Determine installation directories
    print ("\nCall getInstallationDirs with prefix = ", prefix)
    installDirs = getInstallationDirs(osname, prefix, incdir, libdir, bindir)
    
    mainInstallDir    = installDirs[0]
    osDir             = installDirs[1]
    incInstallDir     = installDirs[2]
    archIncInstallDir = installDirs[3]
    libInstallDir     = installDirs[4]
    binInstallDir     = installDirs[5]
    
    # Create the include directories (make symbolic link if possible)
    makeIncludeDirs(incInstallDir, archIncInstallDir, osDir, archIncLocalLink)

    print ('Main install dir  = '+mainInstallDir)
    print ('bin  install dir  = '+binInstallDir)
    print ('lib  install dir  = '+libInstallDir)
    print ('inc  install dirs = '+incInstallDir+", "+archIncInstallDir)

else:
    print ('No installation being done')

print

# use "install" on command line to install libs & headers
Help('install             install libs, headers, and binaries\n')

# uninstall option
Help('install -c          uninstall libs, headers, and binaries\n')


###############################
# Documentation (un)generation
###############################

if 'doc' in COMMAND_LINE_TARGETS:
    if onlyC:
        generateDocs(env, False, True)
    else:
        generateDocs(env, True, True)

if 'undoc' in COMMAND_LINE_TARGETS:
    removeDocs(env)


# use "doc" on command line to create tar file
Help('doc                 create doxygen and javadoc (in ./doc)\n')

# use "undoc" on command line to create tar file
Help('undoc               remove doxygen and javadoc (in ./doc)\n')


#########################
# Tar file
#########################

if 'tar' in COMMAND_LINE_TARGETS:
    generateTarFile(env, 'evio', versionMajor, versionMinor)

# use "tar" on command line to create tar file
Help('tar                 create tar file (in ./tar)\n')


######################################################
# Lower level scons files (software package dependent)
######################################################

# Make available to lower level scons files
Export('env platform archDir incInstallDir libInstallDir binInstallDir archIncInstallDir execLibs debugSuffix disruptorHome')

# Run lower level build files
env.SConscript('src/libsrc/SConscript', variant_dir='src/libsrc/'+archDir,   duplicate=0)

if not onlyC:
    env.SConscript('src/libsrc++/SConscript',  variant_dir='src/libsrc++/'+archDir, duplicate=0)
    #env.SConscript('src/execsrc/SConscript',   variant_dir='src/execsrc/'+archDir,  duplicate=0)
    #env.SConscript('src/examples/SConscript',  variant_dir='src/examples/'+archDir, duplicate=0)
    env.SConscript('src/test/SConscript',      variant_dir='src/test/'+archDir,     duplicate=0)

env.SConscript('src/testC/SConscript',     variant_dir='src/testC/'+archDir,     duplicate=0)

print (color.BOLD + color.RED + "WARNING: using scons for EVIO C/C++ libraries is deprecated! \n"\
                                "    option to build via `scons install` will be removed in future updates \n"\
                                "    (use cmake instead)" + color.END)

