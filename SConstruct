################################
# scons build system file
################################
#
# Most of this file can be used as a boilerplate build file.
# The only parts that are dependent on the details of the
# software package being compiled are the name of the tar file
# and the list of scons files to be run in the lower level
# directories.
# NOTE: to include other scons files, use the python command
#       execfile('fileName')
#
################################

# get operating system info
import re
import sys
import glob
import os
import string
import subprocess
import SCons.Node.FS
from os import access, F_OK, sep, symlink, lstat
from subprocess import Popen, PIPE

os.umask(022)

# Software version
versionMajor = '4'
versionMinor = '1'

# determine the os and machine names
uname    = os.uname();
platform = uname[0]
machine  = uname[4]
osname   = os.getenv('CODA_OSNAME', platform + '-' +  machine)

# Create an environment while importing the user's PATH.
# This allows us to get to the vxworks compiler for example.
# So for vxworks, make sure the tools are in your PATH
env = Environment(ENV = {'PATH' : os.environ['PATH']})

def recursiveDirs(root) :
    return filter( (lambda a : a.rfind( ".svn")==-1 ),  [ a[0] for a in os.walk(root)]  )

def unique(list) :
    return dict.fromkeys(list).keys()

def scanFiles(dir, accept=["*.cpp"], reject=[]) :
    sources = []
    paths = recursiveDirs(dir)
    for path in paths :
        for pattern in accept :
            sources+=glob.glob(path+"/"+pattern)
    for pattern in reject :
        sources = filter( (lambda a : a.rfind(pattern)==-1 ),  sources )
    return unique(sources)

def subdirsContaining(root, patterns):
    dirs = unique(map(os.path.dirname, scanFiles(root, patterns)))
    dirs.sort()
    return dirs

####################################################################################
# Create a Builder to install symbolic links, where "source" is list of node objects
# of the existing files, and "target" is a list of node objects of link files.
# NOTE: link file must have same name as its source file.
####################################################################################
def buildSymbolicLinks(target, source, env):    
    # For each file to create a link for ...
    for s in source:
        filename = os.path.basename(str(s))

        # is there a corresponding link to make?
        makeLink = False
        for t in target:
            linkname = os.path.basename(str(t))
            if not linkname == filename:
                continue
            else :
                makeLink = True
                break

        # go to next source since no corresponding link
        if not makeLink:
            continue
        
        # If link exists don't recreate it
        try:
            # Test if the symlink exists
            lstat(str(t))
        except OSError:
            # OK, symlink doesn't exist so create it
            pass
        else:
            continue

        # remember our current working dir
        currentDir = os.getcwd()

        # get directory of target
        targetDirName = os.path.dirname(str(t))

        # change dirs to target directory
        os.chdir(targetDirName)

        # create symbolic link: symlink(source, linkname)
        symlink("../../include/"+linkname, linkname)

        # change back to original directory
        os.chdir(currentDir)

    return None


symLinkBuilder = Builder(action = buildSymbolicLinks)
env.Append(BUILDERS = {'CreateSymbolicLinks' : symLinkBuilder})

################################
# 64 or 32 bit operating system?
################################
    
# Define configure-type test function to
# see if operating system is 64 or 32 bits
def CheckHas64Bits(context, flags):
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
    # restore original flags
    context.env.Replace(CCFLAGS = lastCCFLAGS, LINKFLAGS = lastLFLAGS)
    # if program successfully ran ...
    if ret[0]:
        context.Result(ret[1])
        if ret[1] == '64':
            return 64
        return 32
    # else if program did not run ...
    else:
        # Don't know if it's a 32 or 64 bit operating system
        context.Result('failed')
        return 0

# How many bits is the operating system?
# For Linux 64 bit x86 machines, the "machine' variable is x86_64,
# but for Darwin or Solaris there is no obvious check so run
# a configure-type test.
ccflags = ''
is64bits = False
if platform == 'Linux':
    if machine == 'x86_64':
        is64bits = True
elif platform == 'Darwin' or platform == 'SunOS':
    ccflags = '-xarch=amd64'
    if platform == 'Darwin':
        ccflags = '-arch x86_64'
    # run the test
    conf = Configure( env, custom_tests = { 'CheckBits' : CheckHas64Bits } )
    ret = conf.CheckBits(ccflags)
    env = conf.Finish()
    if ret < 1:
        print 'Cannot run test, assume 32 bit system'
    elif ret == 64:
        print 'Found 64 bit system'
        is64bits = True;
    else:
        print 'Found 32 bit system'


#########################################
# Command line options (try scons -h)
#########################################

Help('\n-D                  build from subdirectory of package\n')

# debug option
AddOption('--dbg',
           dest='ddebug',
           default=False,
           action='store_true')
debug = GetOption('ddebug')
print "debug =", debug
Help('\nlocal scons OPTIONS:\n')
Help('--dbg               compile with debug flag\n')

# vxworks option
AddOption('--vx',
           dest='doVX',
           default=False,
           action='store_true')
useVxworks = GetOption('doVX')
print "useVxworks =", useVxworks
Help('--vx                cross compile for vxworks\n')

# 32 bit option
AddOption('--32bits',
           dest='use32bits',
           default=False,
           action='store_true')
use32bits = GetOption('use32bits')
print "use32bits =", use32bits
Help('--32bits            compile 32bit libs & executables on 64bit system\n')

# install directory option
AddOption('--prefix',
           dest='prefix',
           nargs=1,
           default='',
           action='store')
prefix = GetOption('prefix')
Help('--prefix=<dir>      use base directory <dir> when doing install\n')

AddOption('--incdir',
           dest='incdir',
           nargs=1,
           default=None,
           action='store')
incdir = GetOption('incdir')
Help('--incdir=<dir>      copy header  files to directory <dir> when doing install\n')

AddOption('--libdir',
           dest='libdir',
           nargs=1,
           default=None,
           action='store')
libdir = GetOption('libdir')
Help('--libdir=<dir>      copy library files to directory <dir> when doing install\n')

AddOption('--bindir',
          dest='bindir',
          nargs=1,
          default=None,
          action='store')
bindir = GetOption('bindir')
Help('--bindir=<dir>      copy binary  files to directory <dir> when doing install\n')



#########################
# Compile flags
#########################

# debug/optimization flags
debugSuffix = ''
if debug:
    debugSuffix = '-dbg'
    # compile with -g and add debugSuffix to all executable names
    env.Append(CCFLAGS = '-g', PROGSUFFIX = debugSuffix)

elif useVxworks:
    # no optimization for vxworks
    junk = 'rt'

elif platform == 'SunOS':
    env.Append(CCFLAGS = '-xO3')

else:
    env.Append(CCFLAGS = '-O3')

vxInc = ''
execLibs = ['']

# If using vxworks
if useVxworks:
    print "\nDoing vxworks\n"
    osname = 'vxworks-ppc'

    if platform == 'Linux':
        vxbase = os.getenv('WIND_BASE', '/site/vxworks/5.5/ppc')
        vxbin = vxbase + '/x86-linux/bin'
    elif platform == 'SunOS':
        vxbase = os.getenv('WIND_BASE', '/site/vxworks/5.5/ppc')
        print "WIND_BASE = ", vxbase
        vxbin = vxbase + '/sun4-solaris/bin'
        if machine == 'i86pc':
            print '\nVxworks compilation not allowed on x86 solaris\n'
            raise SystemExit
    else:
        print '\nVxworks compilation not allowed on ' + platform + '\n'
        raise SystemExit
                    
    env.Replace(SHLIBSUFFIX = '.o')
    # get rid of -shared and use -r
    env.Replace(SHLINKFLAGS = '-r')
    # redefine SHCFLAGS/SHCCFLAGS to get rid of -fPIC (in Linux)
    env.Replace(SHCFLAGS = '-fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -mlongcall -mcpu=604')
    env.Replace(SHCCFLAGS = '-fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -mlongcall -mcpu=604')
    env.Append(CFLAGS = '-fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -mlongcall -mcpu=604')
    env.Append(CCFLAGS = '-fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -mlongcall -mcpu=604')
    env.Append(CPPPATH = vxbase + '/target/h')
    env.Append(CPPDEFINES = ['CPU=PPC604', 'VXWORKS', '_GNU_TOOL', 'VXWORKSPPC', 'POSIX_MISTAKE'])
    env['CC']     = 'ccppc'
    env['CXX']    = 'g++ppc'
    env['SHLINK'] = 'ldppc'
    env['AR']     = 'arppc'
    env['RANLIB'] = 'ranlibppc'
    use32bits = True

# else if NOT using vxworks
else:
    # platform dependent quantities
    execLibs = ['pthread', 'expat', 'z', 'dl', 'm']  # default to standard Linux libs
    env.AppendUnique(CPPPATH = ['/usr/include', '/usr/local/include'])
    if is64bits:
        env.AppendUnique(LIBPATH = ['/usr/lib64', '/usr/local/lib64', '/usr/lib', '/usr/local/lib'])
    else:
        env.AppendUnique(LIBPATH = ['/usr/lib', '/usr/local/lib'])
  
    if platform == 'SunOS':
        env.Append(CCFLAGS = '-mt')
        env.Append(CPPDEFINES = ['_GNU_SOURCE', '_REENTRANT', '_POSIX_PTHREAD_SEMANTICS', 'SunOS'])
        execLibs = ['m', 'posix4', 'pthread', 'dl', 'expat', 'z']
        if is64bits and not use32bits:
            if machine == 'sun4u':
                env.Append(CCFLAGS = '-xarch=native64 -xcode=pic32',
                           #LIBPATH = '/lib/64', # to do this we need to pass LIBPATH to lower level
                           LINKFLAGS = '-xarch=native64 -xcode=pic32')
            else:
                env.Append(CCFLAGS = '-xarch=amd64',
                           #LIBPATH = ['/lib/64', '/usr/ucblib/amd64'],
                           LINKFLAGS = '-xarch=amd64')

    elif platform == 'Darwin':
        execLibs = ['pthread', 'dl', 'expat', 'z']
        env.Append(CPPDEFINES = 'Darwin', SHLINKFLAGS = '-multiply_defined suppress -flat_namespace -undefined suppress')
        env.Append(CCFLAGS = '-fmessage-length=0')
        if is64bits and not use32bits:
            env.Append(CCFLAGS = '-arch x86_64',
                       LINKFLAGS = '-arch x86_64 -Wl,-bind_at_load')

    elif platform == 'Linux':
        if is64bits and use32bits:
            env.Append(CCFLAGS = '-m32', LINKFLAGS = '-m32')

    if not is64bits and not use32bits:
        use32bits = True

if is64bits and use32bits and not useVxworks:
    osname = osname + '-32'

print "OSNAME = ", osname

# hidden sub directory into which variant builds go
archDir = '.' + osname + debugSuffix

#########################
# Install stuff
#########################
codaHomeEnv = ''

# Are we going to install anything?
installingStuff = False
if 'install' in COMMAND_LINE_TARGETS or 'examples' in COMMAND_LINE_TARGETS  or 'tests' in COMMAND_LINE_TARGETS:
    installingStuff = True


# It's possible no installation is being done
if not installingStuff:
    libInstallDir     = "dummy"
    incInstallDir     = "dummy"
    binInstallDir     = "dummy"
    archIncInstallDir = "dummy2"

else:
# The installation directory is the user-specified "prefix"
# by first choice, "CODA" secondly.
# Any user specified command line installation path overrides default
    if (prefix == None) or (prefix == ''):
# prefix not defined try CODA env var
        codaHomeEnv = os.getenv('CODA',"")
        if codaHomeEnv == "":
            if (incdir == None) or (libdir == None) or (bindir == None):
                print
                print "Need to define CODA, or use the --prefix option,"
                print "or all the --incdir, --libdir, and --bindir options."
                print
                raise SystemExit
        else:
            prefix = codaHomeEnv
            print "Default install directory = ", prefix
    else:
        print 'Cmdline install directory = ', prefix


# set our install directories
    if incdir != None:
        incDir = incdir
        archIncDir = incdir
    else:
        archIncDir = prefix + "/" + osname + '/include'
        incDir = prefix + '/include'

    if libdir != None:
        libDir = libdir
    else:
        libDir = prefix + "/" + osname + '/lib'

    if bindir != None:
        binDir = bindir
    else:
        binDir = prefix + "/" + osname + '/bin'


# func to determine absolute path
    def make_abs_path(d):
        if not d[0] in [sep,'#','/','.']:
            if d[1] != ':':
                d = '#' + d
        if d[:2] == '.'+sep:
            d = os.path.join(os.path.abspath('.'), d[2:])
        return d


    incInstallDir     = make_abs_path(incDir)
    archIncInstallDir = make_abs_path(archIncDir)
    libInstallDir     = make_abs_path(libDir)
    binInstallDir     = make_abs_path(binDir)

# print our install directories
    print 'bin install dir  = ', binInstallDir
    print 'lib install dir  = ', libInstallDir
    print 'inc install dirs = ', incInstallDir, ", ", archIncInstallDir


# use "install" on command line to install libs & headers
Help('install             install libs and headers\n')

# uninstall option
Help('-c  install         uninstall libs and headers\n')

# use "examples" on command line to install executable examples
Help('examples            install executable examples\n')

# use "tests" on command line to install executable tests
Help('tests               install executable tests\n')

# not necessary to create install directories explicitly
# (done automatically during install)

###########################
# Documentation generation
###########################

if 'doc' in COMMAND_LINE_TARGETS:
    # Functions that do the documentation creation
    def docGeneratorC(target, source, env):
        cmd = 'doxygen doc/doxygen/DoxyfileC'
        pipe = Popen(cmd, shell=True, env={"TOPLEVEL": "./"}, stdout=PIPE).stdout
        return

    def docGeneratorCC(target, source, env):
        cmd = 'doxygen doc/doxygen/DoxyfileCC'
        pipe = Popen(cmd, shell=True, env={"TOPLEVEL": "./"}, stdout=PIPE).stdout
        return

    # doc files builders
    docBuildC = Builder(action = docGeneratorC)
    env.Append(BUILDERS = {'DocGenC' : docBuildC})

    docBuildCC = Builder(action = docGeneratorCC)
    env.Append(BUILDERS = {'DocGenCC' : docBuildCC})

    # generate documentation
    env.Alias('doc', env.DocGenC(target = ['#/doc/doxygen/C/html/index.html'],
              source = scanFiles("src/libsrc", accept=["*.[ch]"]) ))

    env.Alias('doc', env.DocGenCC(target = ['#/doc/doxygen/CC/html/index.html'],
              source = scanFiles("src/libsrc++", accept=["*.[ch]", "*.cc", "*.hxx"]) ))


# use "doc" on command line to create tar file
Help('doc                 create doxygen docs (in ./doc)\n')


#########################
# Tar file
#########################

# Function that does the tar. Note that tar on Solaris is different
# (more primitive) than tar on Linux and MacOS. Solaris tar has no -z option
# and the exclude file does not allow wildcards. Thus, stick to Linux for
# creating the tar file.
def tarballer(target, source, env):
    if platform == 'SunOS':
        print '\nMake tar file from Linux or MacOS please\n'
        return
    dirname = os.path.basename(os.path.abspath('.'))
    cmd = 'tar -X tar/tarexclude -C .. -c -z -f ' + str(target[0]) + ' ./' + dirname
    p = os.popen(cmd)
    return p.close()

# name of tarfile (software package dependent)
tarfile = 'tar/evio-' + versionMajor + '.' + versionMinor + '.tgz'

# tarfile builder
tarBuild = Builder(action = tarballer)
env.Append(BUILDERS = {'Tarball' : tarBuild})
env.Alias('tar', env.Tarball(target = tarfile, source = None))

# use "tar" on command line to create tar file
Help('tar                 create tar file (in ./tar)\n')

######################################################
# Lower level scons files (software package dependent)
######################################################

# make available to lower level scons files
Export('env incInstallDir libInstallDir binInstallDir archIncInstallDir archDir execLibs tarfile debugSuffix')

# run lower level build files or doc target

if useVxworks:
    env.SConscript('src/libsrc/SConscript.vx',   variant_dir='src/libsrc/'+archDir,   duplicate=0)
    env.SConscript('src/libsrc++/SConscript.vx', variant_dir='src/libsrc++/'+archDir, duplicate=0)
#elif doc:
#    cmd = 'doxygen doc/doxygen/Doxyfile'
#    p = os.popen(cmd)
#    p.close()
else:
    env.SConscript('src/libsrc/SConscript',   variant_dir='src/libsrc/'+archDir,   duplicate=0)
    env.SConscript('src/libsrc++/SConscript', variant_dir='src/libsrc++/'+archDir, duplicate=0)
#    if 'examples' in COMMAND_LINE_TARGETS:
    env.SConscript('src/examples/SConscript', variant_dir='src/examples/'+archDir, duplicate=0)
#    if 'execsrc' in COMMAND_LINE_TARGETS:
    env.SConscript('src/execsrc/SConscript',  variant_dir='src/execsrc/'+archDir,  duplicate=0)
#    if 'tests' in COMMAND_LINE_TARGETS:
    env.SConscript('src/test/SConscript',     variant_dir='src/test/'+archDir,     duplicate=0)
