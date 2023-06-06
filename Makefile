#
# Makefile for nidas Debian packages
# The primary task is to invoke scons to do the build and
# install to the $DESTDIR.
# 
# Here's a table of install directories of RPMs and Debian packages for
# various architectures. "scons install" puts things in a nidas directory
# tree, and "scons install.root" install files into system directories, under
# DESTDIR.
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
#	e.g.:/lib/modules/3.16.0-titan2/nidas
# armhf: rpi2
#   libs:
#     scons: $DESTDIR/$PREFIX/lib/arm-linux-gnueabihf
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/arm-linux-gnueabihf
#   modules:
#     scons: $DESTDIR/$PREFIX/armhf/modules
#     deb (nidas-modules-rpi2): 
#	/lib/modules/$(uname -r)/nidas
#	e.g.: /lib/modules/4.4.9-v7+/nidas (RPi2, May 2016)
# i386, vortex
#   libs:
#     scons: $DESTDIR/$PREFIX/lib
#     deb (nidas-libs,nidas-dev): $PREFIX/lib/i386-linux-gnu
#   modules:
#     scons: $DESTDIR/$PREFIX/modules
#     deb (nidas-modules-vortex): 
#	/lib/modules/$(uname -r)/nidas
#	e.g.: /lib/modules/4.15.18-vortex86dx3/nidas (vortex, Oct 2020)
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
# armbe (old, non-EABI): vulcan (not built from this Makefile)
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
BUILD ?= "host"
REPO_TAG ?= v1.2
PREFIX=/opt/nidas

ARCHLIBDIR := lib/$(DEB_HOST_MULTIARCH)

# Build modules according to the default for the current target.
LINUX_MODULES := yes

# Where to find pkg-configs of other software
PKG_CONFIG_PATH := /usr/lib/$(DEB_HOST_MULTIARCH)/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig

.PHONY : build clean scons_install

$(info DESTDIR=$(DESTDIR))
$(info DEB_BUILD_GNU_TYPE=$(DEB_BUILD_GNU_TYPE))
$(info DEB_HOST_GNU_TYPE=$(DEB_HOST_GNU_TYPE))
$(info DEB_HOST_MULTIARCH=$(DEB_HOST_MULTIARCH))

# hack: scons --config option is passed using GCJFLAGS
$(info DEB_GCJFLAGS_MAINT_SET= $(DEB_GCJFLAGS_MAINT_SET))
$(info GCJFLAGS= $(GCJFLAGS))

build:
	$(SCONS) -C src $(GCJFLAGS) -j 4 BUILD=$(BUILD) \
		REPO_TAG=$(REPO_TAG) \
		PREFIX=$(PREFIX) \
		ARCHLIBDIR=$(ARCHLIBDIR) \
		LINUX_MODULES=$(LINUX_MODULES) \
		PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)

install:
	$(SCONS) -C src -j 4 BUILD=$(BUILD) \
		REPO_TAG=$(REPO_TAG) \
		INSTALL_ROOT=$(DESTDIR) \
		PREFIX=$(PREFIX) \
		ARCHLIBDIR=$(ARCHLIBDIR) \
		LINUX_MODULES=$(LINUX_MODULES) \
		PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) install install.root

clean:
	$(SCONS) -C src -c BUILDS="$(BUILDS)"
