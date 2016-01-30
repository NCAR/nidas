#
# Makefile which invokes scons in the src directory.
# This simplifies the debian packaging, primarily so that
# $DESTDIR is known.
#
SCONS = scons
BUILDS ?= "host"
REPO_TAG ?= v1.2
PREFIX=/opt/nidas
DESTDIR =

LDCONF = $(DESTDIR)/etc/ld.so.conf.d/nidas-$(DEB_HOST_GNU_TYPE).conf
PROFSH = $(DESTDIR)/etc/profile.d/nidas.sh
UDEVRULES = $(DESTDIR)/etc/udev/rules.d/99-nidas.rules
SYSTEMD = $(DESTDIR)$(PREFIX)/systemd/user/dsm_server.service

LIBDIR = $(DESTDIR)$(PREFIX)/lib/$(DEB_HOST_GNU_TYPE)

PKGCONFIG = $(DESTDIR)/usr/lib/$(DEB_HOST_GNU_TYPE)/pkgconfig/nidas.pc

ifeq ($(DEB_HOST_GNU_TYPE),x86_64-linux-gnu)
    SCONSLIBDIR = $(DESTDIR)$(PREFIX)/lib
else ifeq ($(DEB_HOST_GNU_TYPE),arm-linux-gnueabi)
    SCONSLIBDIR = $(DESTDIR)$(PREFIX)/armel/lib
    SCONSBINDIR = $(DESTDIR)$(PREFIX)/armel/bin
    SCONSINCDIR = $(DESTDIR)$(PREFIX)/armel/include
    BINDIR = $(DESTDIR)$(PREFIX)/bin
    INCDIR = $(DESTDIR)$(PREFIX)/include
endif

.PHONY : build install clean scons_install

$(info DESTDIR=$(DESTDIR))
$(info DEB_BUILD_GNU_TYPE=$(DEB_BUILD_GNU_TYPE))
$(info DEB_HOST_GNU_TYPE=$(DEB_HOST_GNU_TYPE))


build:
	cd src; $(SCONS) -j 4 BUILDS=$(BUILDS) REPO_TAG=$(REPO_TAG)

$(LDCONF):
	@mkdir -p $(@D)
	echo "/opt/nidas/lib/$(DEB_HOST_GNU_TYPE)" > $@

$(PROFSH):
	@mkdir -p $(@D)
	cp pkg_files/root/etc/profile.d/* $(@D)

$(UDEVRULES):
	@mkdir -p $(@D)
	cp pkg_files/root/etc/udev/rules.d/* $(@D)

$(SYSTEMD):
	@mkdir -p $(@D)
	cp pkg_files/systemd/user/* $(@D)

$(PKGCONFIG): pkg_files/root/usr/lib/pkgconfig/nidas.pc
	@mkdir -p $(@D)
	sed -e 's,@PREFIX@,$(PREFIX),g' -e 's/@DEB_HOST_GNU_TYPE@/$(DEB_HOST_GNU_TYPE)/g' -e 's/@REPO_TAG@/$(REPO_TAG)/g' $< > $@

scons_install:
	cd src; \
	$(SCONS) -j 4 BUILDS=$(BUILDS) REPO_TAG=$(REPO_TAG) PREFIX=$(DESTDIR)$(PREFIX) install

install: scons_install $(LDCONF) $(PROFSH) $(UDEVRULES) $(SYSTEMD) $(PKGCONFIG)
	mkdir -p $(LIBDIR);\
	mv $(SCONSLIBDIR)/*.so* $(LIBDIR);\
	if [ -n "$(SCONSBINDIR)" ]; then\
	    mkdir -p $(BINDIR);\
	    mv $(SCONSBINDIR)/* $(BINDIR);\
	    mv $(SCONSINCDIR)/* $(INCDIR);\
	fi

clean:
	cd src; $(SCONS) -c BUILDS=$(BUILDS)

