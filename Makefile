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
#     scons: $DESTDIR/$PREFIX/lib64
#     rpm (nidas-libs): $PREFIX/lib64
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/x86_64-linux-gnu
#   bin:
#     scons: $DESTDIR/$PREFIX/bin
#     rpm (nidas): $PREFIX/bin
#     deb (nidas): $PREFIX/bin
#   headers:
#     scons: $DESTDIR/$PREFIX/include
#     rpm (nidas-devel): $PREFIX/include
#     deb (nidas-dev): $PREFIX/include
#   modules:
#     scons: $DESTDIR/$PREFIX/modules
#     rpm (nidas-modules):
#       should be /lib/modules/$(uname-r)/nidas
#     deb (nidas-modules-amd64): /lib/modules/$(uname-r)/nidas
#	e.g.: /lib/modules/3.16.0-4-amd64/nidas
# armel: titan, viper
#   libs:
#     scons: $DESTDIR/$PREFIX/armel/lib
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/arm-linux-gnueabi
#   bin:
#     scons: $DESTDIR/$PREFIX/armel/bin
#     deb (nidas): $PREFIX/bin
#   headers:
#     scons: $DESTDIR/$PREFIX/include
#     deb (nidas-dev): $PREFIX/include
#   modules:
#     scons: $DESTDIR/$PREFIX/armel/modules
#     deb (nidas-modules-titan, nidas-modules-viper):
#	/lib/modules/$(uname -r)/nidas
#	e.g.:/lib/modules/3.16.0-titan2
# armhf: rpi2
#   libs:
#     scons: $DESTDIR/$PREFIX/armhf/lib
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/arm-linux-gnueabihf
#   bin:
#     scons: $DESTDIR/$PREFIX/armhf/bin
#     deb (nidas): $PREFIX/bin
#   headers:
#     scons: $DESTDIR/$PREFIX/include
#     deb (nidas-dev): $PREFIX/include
#   modules:
#     scons: $DESTDIR/$PREFIX/armhf/modules
#     deb (nidas-modules-rpi2): 
#	/lib/modules/?
# arm (old, non-EABI): viper, titan
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
# armbe (old, non-EABI): vulcan
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

LDCONF = $(DESTDIR)/etc/ld.so.conf.d/nidas-$(DEB_HOST_GNU_TYPE).conf

LIBDIR = $(DESTDIR)$(PREFIX)/lib/$(DEB_HOST_GNU_TYPE)
MODDIR = $(DESTDIR)/lib/modules
BINDIR = $(DESTDIR)$(PREFIX)/bin
INCDIR = $(DESTDIR)$(PREFIX)/include

PKGCONFIG = $(DESTDIR)/usr/lib/$(DEB_HOST_GNU_TYPE)/pkgconfig/nidas.pc


ifeq ($(DEB_HOST_GNU_TYPE),x86_64-linux-gnu)
    SCONSLIBDIR = $(DESTDIR)$(PREFIX)/lib64
    TITAN_KERN :=
    VIPER_KERN :=
    X86_64_KERN := $(shell uname -r)
    SCONSMODDIR = $(DESTDIR)$(PREFIX)/modules
else ifeq ($(DEB_HOST_GNU_TYPE),arm-linux-gnueabi)
    SCONSLIBDIR = $(DESTDIR)$(PREFIX)/armel/lib
    SCONSBINDIR = $(DESTDIR)$(PREFIX)/armel/bin
    SCONSINCDIR = $(DESTDIR)$(PREFIX)/armel/include
    TITAN_KERN := $(shell find /usr/src -maxdepth 1 -name "linux-headers-*titan*" -type d | sed s/.*linux-headers-//)
    VIPER_KERN := $(shell find /usr/src -maxdepth 1 -name "linux-headers-*viper*" -type d | sed s/.*linux-headers-//)
    SCONSMODDIR = $(DESTDIR)$(PREFIX)/armel/modules
else ifeq ($(DEB_HOST_GNU_TYPE),arm-linux-gnueabihf)
    SCONSLIBDIR = $(DESTDIR)$(PREFIX)/armhf/lib
    SCONSBINDIR = $(DESTDIR)$(PREFIX)/armhf/bin
    SCONSINCDIR = $(DESTDIR)$(PREFIX)/armhf/include
    SCONSMODDIR = $(DESTDIR)$(PREFIX)/armhf/modules
endif

.PHONY : build install clean scons_install $(LDCONF) $(PKGCONFIG)

$(info DESTDIR=$(DESTDIR))
$(info DEB_BUILD_GNU_TYPE=$(DEB_BUILD_GNU_TYPE))
$(info DEB_HOST_GNU_TYPE=$(DEB_HOST_GNU_TYPE))

build:
	cd src; $(SCONS) --config=force -j 4 BUILDS=$(BUILDS) REPO_TAG=$(REPO_TAG)

$(LDCONF):
	@mkdir -p $(@D)
	echo "/opt/nidas/lib/$(DEB_HOST_GNU_TYPE)" > $@

$(PKGCONFIG): pkg_files/root/usr/lib/pkgconfig/nidas.pc
	@mkdir -p $(@D)
	sed -e 's,@PREFIX@,$(PREFIX),g' -e 's/@DEB_HOST_GNU_TYPE@/$(DEB_HOST_GNU_TYPE)/g' -e 's/@REPO_TAG@/$(REPO_TAG)/g' $< > $@

scons_install:
	cd src; \
	$(SCONS) -j 4 BUILDS=$(BUILDS) REPO_TAG=$(REPO_TAG) PREFIX=$(DESTDIR)$(PREFIX) install

install: scons_install $(LDCONF) $(PKGCONFIG)
	mkdir -p $(LIBDIR);\
	mv $(SCONSLIBDIR)/*.so* $(LIBDIR);\
	if [ -n "$(SCONSBINDIR)" ]; then\
	    mkdir -p $(BINDIR);\
	    mv $(SCONSBINDIR)/* $(BINDIR);\
	fi
	if [ -n "$(SCONSINCDIR)" ]; then\
	    mkdir -p $(INCDIR);\
	    mv $(SCONSINCDIR)/* $(INCDIR);\
	fi
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
	# Specify all BUILDS in the clean, since we don't want
	# other architecture binaries in the package.  Also have to clean
	# the armel binaries too, otherwise they'll end up in the source tar.
	# For now armel is not in the list of default BUILDS
	# so we have to list them all.
	cd src; $(SCONS) -c BUILDS="armel armhf host arm armbe"

