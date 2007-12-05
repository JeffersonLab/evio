from util import subdirsContaining 
from util import unique 
from util import scanFiles
from util import recursiveDirs 
from util import cmloptions
from util import loadoptions

# Command line Options
opts = Options()
cmloptions(opts)
    
env = Environment(tools=['default'], options = opts)

# evio Software
from loadevio import loadevio
loadevio(env)


# General Options
loadoptions(env)

# Generate help for cmd line options
Help(opts.GenerateHelpText(env))

# Include Path
env.Append(LIBPATH = ['lib'])


# Build libraries
env.Library('lib/evio',  scanFiles('src/libsrc', accept=[ "*.c"]) )
env.Library('lib/evioxx', 'src/libsrc++/evioUtil.cc')


# Executables:
env.Program('bin/evio2xml', 'src/execsrc/evio2xml.c')
env.Program('bin/eviocopy', 'src/execsrc/eviocopy.c')
env.Program('bin/xml2evio', 'src/execsrc/xml2evio.c')
env.Program('bin/etst1',    'src/examples/etst1.cc')
env.Program('bin/etst2',    'src/examples/etst2.cc')
env.Program('bin/etst3',    'src/examples/etst3.cc')
env.Program('bin/etst4',    'src/examples/etst4.cc')
env.Program('bin/etst5',    'src/examples/etst5.cc')
env.Program('bin/etst6',    'src/examples/etst6.cc')
env.Program('bin/etst7',    'src/examples/etst7.cc')

