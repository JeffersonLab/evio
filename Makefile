#
# evio top level Makefile
#

MAKEFILE = Makefile

.PHONY : all src env mkdirs install uninstall relink clean distClean execClean java tar doc


all: src

src:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE);
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE);
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE);
	cd src/examples; $(MAKE) -f $(MAKEFILE);
	cd src/test;     $(MAKE) -f $(MAKEFILE);

env:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) env;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) env;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) env;
	cd src/examples; $(MAKE) -f $(MAKEFILE) env;
	cd src/test;     $(MAKE) -f $(MAKEFILE) env;

mkdirs:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) mkdirs;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) mkdirs;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) mkdirs;
	cd src/examples; $(MAKE) -f $(MAKEFILE) mkdirs;
	cd src/test;     $(MAKE) -f $(MAKEFILE) mkdirs;
	ant prepare;

install:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) install;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) install;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) install;
	cd src/examples; $(MAKE) -f $(MAKEFILE) install;
	cd src/test;     $(MAKE) -f $(MAKEFILE) install;

uninstall: 
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) uninstall;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) uninstall;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) uninstall;
	cd src/examples; $(MAKE) -f $(MAKEFILE) uninstall;
	cd src/test;     $(MAKE) -f $(MAKEFILE) uninstall;

relink:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) relink;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) relink;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) relink;
	cd src/examples; $(MAKE) -f $(MAKEFILE) relink;
	cd src/test;     $(MAKE) -f $(MAKEFILE) relink;

clean:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) clean;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) clean;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) clean;
	cd src/examples; $(MAKE) -f $(MAKEFILE) clean;
	cd src/test;     $(MAKE) -f $(MAKEFILE) clean;
	ant clean;

distClean:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) distClean;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) distClean;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) distClean;
	cd src/examples; $(MAKE) -f $(MAKEFILE) distClean;
	cd src/test;     $(MAKE) -f $(MAKEFILE) distClean;
	ant cleanall;

execClean:
	cd src/libsrc;   $(MAKE) -f $(MAKEFILE) execClean;
	cd src/libsrc++; $(MAKE) -f $(MAKEFILE) execClean;
	cd src/execsrc;  $(MAKE) -f $(MAKEFILE) execClean;
	cd src/examples; $(MAKE) -f $(MAKEFILE) execClean;
	cd src/test;     $(MAKE) -f $(MAKEFILE) execClean;

java:
	ant;

doc:
	ant javadoc;

tar:
	-$(RM) tar/evio-2.0.tar.gz;
	tar -X tar/tarexclude -C .. -c -z -f tar/evio-2.0.tar.gz evio
