#
# $Id: Makefile,v 1.9 1999/05/10 18:58:27 mast Exp $
#
# Meta Makefile
#

VPATH=.
MAKE=make
prefix=/usr/local
OS=`uname -srm|sed -e 's/ /-/g'|tr '[A-Z]' '[a-z]'|tr '/' '_'`
BUILDDIR=build/$(OS)
METATARGET=

# Use this to pass arguments to configure. Leave empty to keep previous args.
CONFIGUREARGS=

# Used to avoid make compatibility problems.
BIN_TRUE=":"

all: bin/pike compile
	-@$(BIN_TRUE)

force:
	-@$(BIN_TRUE)

src/configure: src/configure.in
	cd src && ./run_autoconfig . 2>&1 | grep -v warning
	-@(cd "$(BUILDDIR)" 2>/dev/null && rm -f Makefile .configureargs || :)

force_configure:
	cd src && ./run_autoconfig . 2>&1 | grep -v warning
	-@(cd "$(BUILDDIR)" 2>/dev/null && rm -f Makefile .configureargs || :)

builddir:
	@builddir="$(BUILDDIR)"; \
	( \
	  IFS='/'; dir=""; \
	  for d in $$builddir; do \
	    dir="$$dir$$d"; \
	    test -z "$$dir" -o -d "$$dir" || mkdir "$$dir" || exit 1; \
	    dir="$$dir/"; \
	  done \
	)

configure: src/configure builddir
	@builddir="$(BUILDDIR)"; \
	srcdir=`pwd`/src; \
	cd "$$builddir" && ( \
	  if test -f .configureargs -a -z "$(CONFIGUREARGS)"; then \
	    configureargs="`cat .configureargs`"; \
	  else \
	    configureargs="--prefix=$(prefix) $(CONFIGUREARGS)"; \
	  fi; \
	  if test -f Makefile -a -f config.cache -a -f .configureargs && \
	     test "`cat .configureargs`" = "$$configureargs"; then :; \
	  else \
	    echo Running "$$srcdir"/configure $$configureargs in "$$builddir"; \
	    CONFIG_SITE=x "$$srcdir"/configure $$configureargs && ( \
	      echo "$$configureargs" > .configureargs; \
	      $(MAKE) "MAKE=$(MAKE)" clean > /dev/null; \
	      : \
	    ) \
	  fi \
	)

compile: configure
	@builddir="$(BUILDDIR)"; \
	metatarget="$(METATARGET)"; \
	test -f "$$builddir"/pike || metatarget="new_peep_engine pike $$metatarget"; \
	cd "$$builddir" && ( \
	  echo Making in "$$builddir"; \
	  rm -f remake; \
	  $(MAKE) "MAKE=$(MAKE)" all $$metatarget || ( \
	    test -f remake && $(MAKE) "MAKE=$(MAKE)" all $$metatarget \
	  ) \
	)

bin/pike: force
	sed -e "s|\"BASEDIR\"|\"`pwd`\"|" < bin/pike.in > bin/pike
	chmod a+x bin/pike

install:
	@$(MAKE) "MAKE=$(MAKE)" "prefix=$(prefix)" "BUILDDIR=$(BUILDDIR)" "METATARGET=install"

verify:
	@$(MAKE) "MAKE=$(MAKE)" "prefix=$(prefix)" "BUILDDIR=$(BUILDDIR)" "METATARGET=verify"

verify_installed:
	@$(MAKE) "MAKE=$(MAKE)" "prefix=$(prefix)" "BUILDDIR=$(BUILDDIR)" "METATARGET=verify_installed"

check: verify
	-@$(BIN_TRUE)

sure: verify
	-@$(BIN_TRUE)

verbose_verify:
	@$(MAKE) "MAKE=$(MAKE)" "prefix=$(prefix)" "BUILDDIR=$(BUILDDIR)" "METATARGET=verbose_verify"

gdb_verify:
	@$(MAKE) "MAKE=$(MAKE)" "prefix=$(prefix)" "BUILDDIR=$(BUILDDIR)" "METATARGET=gdb_verify"

run_hilfe:
	@$(MAKE) "MAKE=$(MAKE)" "prefix=$(prefix)" "BUILDDIR=$(BUILDDIR)" "METATARGET=run_hilfe"

feature_list:
	@$(MAKE) "MAKE=$(MAKE)" "prefix=$(prefix)" "BUILDDIR=$(BUILDDIR)" "METATARGET=feature_list"

clean:
	-cd "$(BUILDDIR)" && $(MAKE) "MAKE=$(MAKE)" clean

spotless:
	-cd "$(BUILDDIR)" && $(MAKE) "MAKE=$(MAKE)" spotless

distclean:
	-rm -rf build bin/pike

cvsclean: distclean
	for d in `find src -type d -print`; do if test -f "$$d/.cvsignore"; then (cd "$$d" && rm -f `cat ".cvsignore"`); else :; fi; done

depend:
	-cd "$(BUILDDIR)" && $(MAKE) "MAKE=$(MAKE)" depend
