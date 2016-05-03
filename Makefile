#
# Makefile for nidas Debian packages
# The primary task is to invoke scons to do the build and
# install to the $DESTDIR. Install targets in the Makefile
# move things around in $DESTDIR when paths are dependent on
# Debian variable that is known to this Makefile, but isn't
# (yet) passed to scons
# 
# Here's a table of install directories of RPMs and Debian
# packages for various architectures. "scons install" puts
# things in a nidas directory tree, and then, if necessary
# nidas.spec and this Makefile move them to a place approprite
# for the package.
#
# $PREFIX is /typically defined to be /opt/nidas.
#
# x86_64, amd64:
#   libs:
#     rpm (nidas-libs): $PREFIX/lib64
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/x86_64-linux-gnu
#   modules:
#     scons: $DESTDIR/$PREFIX/modules
#     rpm (nidas-modules):
#       should be /lib/modules/$(uname-r)/nidas
#     deb (nidas-modules-amd64): /lib/modules/$(uname-r)/nidas
#	e.g.: /lib/modules/3.16.0-4-amd64/nidas
# armel: titan, viper
#   libs:
#     scons: $DESTDIR/$PREFIX/lib/arm-linux-gnueabi
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/arm-linux-gnueabi
#   modules:
#     scons: $DESTDIR/$PREFIX/armel/modules
#     deb (nidas-modules-titan, nidas-modules-viper):
#	/lib/modules/$(uname -r)/nidas
#	e.g.:/lib/modules/3.16.0-titan2
# armhf: rpi2
#   libs:
#     scons: $DESTDIR/$PREFIX/lib/arm-linux-gnueabihf
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/arm-linux-gnueabihf
#   modules:
#     scons: $DESTDIR/$PREFIX/armhf/modules
#     deb (nidas-modules-rpi2): 
#	/lib/modules/?
# arm (old, non-EABI): viper, titan (not built from this Makefile)
#   libs:
#     scons: $DESTDIR/$PREFIX/arm/lib
#     deb: /usr/local/lib
#   bin:
#     scons: $DESTDIR/$PREFIX/arm/bin
#     deb: /usr/local/bin
#   headers:
#     scons: $DESTDIR/$PREFIX/arm/include
#     deb: not in package
#   modules:
#     scons: $DESTDIR/$PREFIX/arm/modules/titan,
#		$DESTDIR/$PREFIX/arm/modules/viper
#     deb: /lib/modules/$kernel/local
#		$kernel is from modinfo *.ko | fgrep vermagic
#		e.g.:   2.6.35.9-ael1-1-viper
# arm (old, non-EABI): viper, titan (not built from this Makefile)
#   libs:
#     scons: $DESTDIR/$PREFIX/armbe/lib
#     deb: /usr/local/lib
#   bin:
#     scons: $DESTDIR/$PREFIX/armbe/bin
#     deb: /usr/local/bin
#   headers:
#     scons: $DESTDIR/$PREFIX/armbe/include
#     deb: not in package
#   modules:
#     scons: $DESTDIR/$PREFIX/armbe/modules/vulcan
#     deb: /lib/modules/$kernel/local
#		$kernel is from modinfo *.ko | fgrep vermagic
#		e.g.:   2.6.21.7-ael2-1-vulcan

SCONS = scons
BUILDS ?= "host"
REPO_TAG ?= v1.2
PREFIX=/opt/nidas

LDCONF := $(DESTDIR)/etc/ld.so.conf.d/nidas-$(DEB_HOST_GNU_TYPE).conf

ARCHLIBDIR := lib/$(DEB_HOST_GNU_TYPE)

MODDIR := $(DESTDIR)/lib/modules

# Copy nidas.pc from $(PREFIX) to /usr/lib
PKGCONFIG := $(DESTDIR)/usr/lib/$(DEB_HOST_GNU_TYPE)/pkgconfig/nidas.pc

SCONSMODDIR := $(DESTDIR)$(PREFIX)/modules

ifeq ($(DEB_HOST_GNU_TYPE),x86_64-linux-gnu)
    TITAN_KERN :=
    VIPER_KERN :=
    X86_64_KERN := $(shell uname -r)
    ARCHLIBDIR := lib64
else ifeq ($(DEB_HOST_GNU_TYPE),arm-linux-gnueabi)
    TITAN_KERN := $(shell find /usr/src -maxdepth 1 -name "linux-headers-*titan*" -type d | sed s/.*linux-headers-//)
    VIPER_KERN := $(shell find /usr/src -maxdepth 1 -name "linux-headers-*viper*" -type d | sed s/.*linux-headers-//)
else ifeq ($(DEB_HOST_GNU_TYPE),arm-linux-gnueabihf)
endif

SCONSPKGCONFIG := $(DESTDIR)$(PREFIX)/$(ARCHLIBDIR)/pkgconfig/nidas.pc

.PHONY : build clean scons_install $(LDCONF)

$(info DESTDIR=$(DESTDIR))
$(info DEB_BUILD_GNU_TYPE=$(DEB_BUILD_GNU_TYPE))
$(info DEB_HOST_GNU_TYPE=$(DEB_HOST_GNU_TYPE))

build:
	cd src; $(SCONS) --config=force -j 4 BUILDS=$(BUILDS) \
		REPO_TAG=$(REPO_TAG) PREFIX=$(PREFIX)

$(LDCONF):
	@mkdir -p $(@D); \
	echo "/opt/nidas/lib/$(DEB_HOST_GNU_TYPE)" > $@

scons_install:
	cd src; $(SCONS) -j 4 BUILDS=$(BUILDS) REPO_TAG=$(REPO_TAG) \
		PREFIX=$(DESTDIR)$(PREFIX) install

$(SCONSPKGCONFIG): scons_install

$(PKGCONFIG): $(SCONSPKGCONFIG)
	@mkdir -p $(@D); \
	mv $< $@

install: scons_install $(LDCONF) $(PKGCONFIG)
	if [ -n "$(TITAN_KERN)" ]; then\
	    mkdir -p $(MODDIR)/$(TITAN_KERN)/nidas;\
	    mv $(SCONSMODDIR)/titan/* $(MODDIR)/$(TITAN_KERN)/nidas;\
	fi
	if [ -n "$(VIPER_KERN)" ]; then\
	    mkdir -p $(MODDIR)/$(VIPER_KERN)/nidas;\
	    mv $(SCONSMODDIR)/viper/* $(MODDIR)/$(VIPER_KERN)/nidas;\
	fi
	if [ -n "$(X86_64_KERN)" ]; then\
	    mkdir -p $(MODDIR)/$(X86_64_KERN)/nidas;\
	    mv $(SCONSMODDIR)/* $(MODDIR)/$(X86_64_KERN)/nidas;\
	fi

clean:
	cd src; $(SCONS) -c BUILDS="host armel armhf arm armbe"

