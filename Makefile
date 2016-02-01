#
# Makefile which invokes scons in the src directory.
# This simplifies the debian packaging, primarily so that
# $DESTDIR is known.
#
SCONS = scons
BUILDS ?= "host"
REPO_TAG ?= v1.2
PREFIX=/opt/nidas

LDCONF = $(DESTDIR)/etc/ld.so.conf.d/nidas-$(DEB_HOST_GNU_TYPE).conf
PROFSH = $(DESTDIR)/etc/profile.d/nidas.sh
UDEVRULES = $(DESTDIR)/etc/udev/rules.d/99-nidas.rules
SYSTEMD = $(DESTDIR)$(PREFIX)/systemd/user/dsm_server.service

LIBDIR = $(DESTDIR)$(PREFIX)/lib/$(DEB_HOST_GNU_TYPE)
MODDIR = $(DESTDIR)/lib/modules
VARLIBDIR = $(DESTDIR)/var/lib/nidas

PKGCONFIG = $(DESTDIR)/usr/lib/$(DEB_HOST_GNU_TYPE)/pkgconfig/nidas.pc

ifeq ($(DEB_HOST_GNU_TYPE),x86_64-linux-gnu)
    SCONSLIBDIR = $(DESTDIR)$(PREFIX)/lib
    TITAN_KERN :=
    VIPER_KERN :=
else ifeq ($(DEB_HOST_GNU_TYPE),arm-linux-gnueabi)
    SCONSLIBDIR = $(DESTDIR)$(PREFIX)/armel/lib
    SCONSBINDIR = $(DESTDIR)$(PREFIX)/armel/bin
    SCONSINCDIR = $(DESTDIR)$(PREFIX)/armel/include
    BINDIR = $(DESTDIR)$(PREFIX)/bin
    INCDIR = $(DESTDIR)$(PREFIX)/include
    TITAN_KERN := $(shell find /usr/src -maxdepth 1 -name "linux-headers-*titan*" -type d | sed s/.*linux-headers-//)
    VIPER_KERN := $(shell find /usr/src -maxdepth 1 -name "linux-headers-*viper*" -type d | sed s/.*linux-headers-//)
    SCONSMODDIR = $(DESTDIR)$(PREFIX)/armel/modules
endif

.PHONY : build install clean scons_install $(LDCONF) $(PROFSH) $(UDEVRULES) $(SYSTEMD) \
	$(PKGCONFIG) $(VARLIBDIR)

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

$(VARLIBDIR):
	@mkdir -p $@
	cp pkg_files/var/lib/nidas/DacUser $@
	cp pkg_files/var/lib/nidas/BuildUserGroup $@

scons_install:
	cd src; \
	$(SCONS) -j 4 BUILDS=$(BUILDS) REPO_TAG=$(REPO_TAG) PREFIX=$(DESTDIR)$(PREFIX) install

install: scons_install $(LDCONF) $(PROFSH) $(UDEVRULES) $(SYSTEMD) $(PKGCONFIG) $(VARLIBDIR)
	mkdir -p $(LIBDIR);\
	mv $(SCONSLIBDIR)/*.so* $(LIBDIR);\
	if [ -n "$(SCONSBINDIR)" ]; then\
	    mkdir -p $(BINDIR);\
	    mv $(SCONSBINDIR)/* $(BINDIR);\
	    mv $(SCONSINCDIR)/* $(INCDIR);\
	fi
	if [ -n "$(TITAN_KERN)" ]; then\
	    mkdir -p $(MODDIR)/$(TITAN_KERN)/nidas;\
	    mv $(SCONSMODDIR)/titan/* $(MODDIR)/$(TITAN_KERN)/nidas;\
	fi
	if [ -n "$(VIPER_KERN)" ]; then\
	    mkdir -p $(MODDIR)/$(VIPER_KERN)/nidas;\
	    mv $(SCONSMODDIR)/titan/* $(MODDIR)/$(VIPER_KERN)/nidas;\
	fi

	# 
	# look in /usr/src/linux-headers-3.16.0-titan2
	# for "titan", grab string after linux-headers-
	# move /opt/nidas/arm/modules/titan to
	# /lib/modules/3.16.0-titan2/nidas
	# Do depmod -a in postinst

clean:
	# Specify all BUILDS in the clean, since we don't want
	# other architecture binaries in the package.  Also have to clean
	# the armel binaries too, otherwise they'll end up in the source tar.
	# For now armel is not in the list of default BUILDS
	# so we have to list them all.
	cd src; $(SCONS) -c BUILDS="armel host arm armbe"

