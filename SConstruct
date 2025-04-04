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

# Get useful python functions we wrote
import coda

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

if path == '':
    print
    print ("Error: set PATH environmental variable")
    print
    raise SystemExit

if ldLibPath == '':
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
is64bits = coda.is64BitMachine(env, platform, machine)
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
if disruptorHome == "":
    if not onlyC:
        print('Disruptor-cpp must be installed by defining DISRUPTOR_CPP_HOME')
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

# code for newlib++ written in C++11
env.Append(CXXFLAGS = ['-std=c++11'])

# Take care of 64/32 bit issues
if is64bits:
    # Setup 64 bit machine to compile either 32 or 64 bit libs & executables
    coda.configure32bits(env, use32bits, platform, machine)
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
    print ("\Call getInstallationDirs with prefix = ", prefix)
    installDirs = coda.getInstallationDirs(osname, prefix, incdir, libdir, bindir)
    
    mainInstallDir    = installDirs[0]
    osDir             = installDirs[1]
    incInstallDir     = installDirs[2]
    archIncInstallDir = installDirs[3]
    libInstallDir     = installDirs[4]
    binInstallDir     = installDirs[5]
    
    # Create the include directories (make symbolic link if possible)
    coda.makeIncludeDirs(incInstallDir, archIncInstallDir, osDir, archIncLocalLink)

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
        coda.generateDocs(env, False, True)
    else:
        coda.generateDocs(env, True, True)

if 'undoc' in COMMAND_LINE_TARGETS:
    coda.removeDocs(env)


# use "doc" on command line to create tar file
Help('doc                 create doxygen and javadoc (in ./doc)\n')

# use "undoc" on command line to create tar file
Help('undoc               remove doxygen and javadoc (in ./doc)\n')


#########################
# Tar file
#########################

if 'tar' in COMMAND_LINE_TARGETS:
    coda.generateTarFile(env, 'evio', versionMajor, versionMinor)

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
