## Process this file with automake to produce Makefile.in

# let make re-run automake upon need
ACLOCAL_AMFLAGS = -I m4 $(ACLOCAL_MACOS_FLAGS)

SUBDIRS = m4 @ORTP_DIR@ @MS2_DIR@ \
		po coreapi console share scripts tools


ACLOCAL_FLAGS=-I$(top_srcdir)/m4

INSTALLDIR=$(abs_top_builddir)/linphone-install
INSTALLDIR_WITH_PREFIX=$(INSTALLDIR)/$(prefix)

EXTRA_DIST =		BUGS  \
			README.arm \
			autogen.sh \
			linphone.spec \
			linphone.spec.in \
			$(LINPHONEDEPS_FILELIST)

CLEANFILES=Portfile Portfile-devel

# `make rpm'

all-local: linphone.spec

linphone.spec: linphone.spec.in

zip:
	rm -rf $(INSTALLDIR)
	$(MKDIR_P) $(INSTALLDIR)
	make install DESTDIR=$(INSTALLDIR)
# remove unwanted linphone stuff
	cp -f $(top_srcdir)/README $(INSTALLDIR_WITH_PREFIX)/.
	cp -f $(top_srcdir)/COPYING $(INSTALLDIR_WITH_PREFIX)/.
	cd $(INSTALLDIR_WITH_PREFIX)

sdk:
	rm -rf $(INSTALLDIR)
	$(MKDIR_P) $(INSTALLDIR)
	make install DESTDIR=$(INSTALLDIR)
# remove unwanted stuff (gtk interface)
	cp -f $(top_srcdir)/README $(INSTALLDIR_WITH_PREFIX)/.
	cp -f $(top_srcdir)/COPYING $(INSTALLDIR_WITH_PREFIX)/.
	cd $(INSTALLDIR_WITH_PREFIX)


if HAVE_MD5SUM
GEN_MD5=`$(MD5SUM) linphone-$(VERSION).tar.gz | awk {'print $$4'}`
else
GEN_MD5=`$(MD5SUM) linphone-$(VERSION).tar.gz | awk {'print $$1'}`
endif

Portfile: $(top_srcdir)/scripts/Portfile.tmpl dist
	sed -e 's/\@VERSION\@/$(LINPHONE_VERSION)/g' \
	  -e 's/\@LINPHONE_MD5\@/$(GEN_MD5)/' < $< > $@

Portfile-devel:	$(top_srcdir)/scripts/Portfile-devel.tmpl dist
	sed -e 's/\@VERSION\@/$(LINPHONE_VERSION)/g' \
	  -e 's/\@LINPHONE_MD5\@/$(GEN_MD5)/' < $< > $@


###
### CLEAN

clean-local:
	rm -rf $(BUNDLEDIR)
discovery:
	touch specs.cpp
	$(CC) --include $(top_builddir)/config.h \
          $(TUNNEL_CFLAGS) $(CFLAGS) $(MEDIASTREAMER2_CFLAGS) $(ORTP_CFLAGS) -E -P -v -dD specs.cpp
