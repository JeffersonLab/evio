#
# evio programs makefile
#

ifndef BMS_HOME
  $(error "Need to define BMS_HOME")
endif

# Setting TOPLEVEL tells Makefile.common where to find Makefile.local
# and to include it if make is run from this dir or the top-level package
# directory.
TOPLEVEL = ../..

PACKAGES = evio
MODULE_NAME = evio

CFLAGS   += -I../libsrc
CXXFLAGS += -I../libsrc

# Things common to all builds live in Makefile.common
include $(BMS_HOME)/Makefile.common

include $(BMS_HOME)/Makefile.bin
