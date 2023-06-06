"""
SCons tool to add configuration variables to set the KERNELDIR according to
various architectures and target machines.  There are configuration variables
to set the defaults for each, and then a single KERNELDIR variable which
specifies the actual path to use for the builders.
"""

from SCons.Script import Variables, Environment, Export

import os
import re

_variables: Variables
_variables = None


# This somewhat replaces functionality originally in the Debian Makefile to
# set the kernal name, aka KERNELRELEASE, so kernel modules can be installed
# directly to the right directory by linux/SConscript.
#
# However, rather than test the dpkg architecture settings or search for a
# kernel source or header directory, this tool can derive the release name
# from the kernel directory that has already been set by a command-line option
# or by the kmake tool.  This way the kernel name should exactly coincide with
# the kernel build tree.

def set_kernel_release(env: Environment, kname=None):
    kdir = env.get('KERNELDIR')
    if kdir:
        # Make sure to evaluate it, in case kdir is set to $KERNELFOUND
        kdir = env.subst(kdir)
    if kdir and kname is None:
        # Take just the last directory in the path, and remove the common
        # prefixes.
        kname = os.path.basename(kdir)
        # Remove any leading linux-headers
        kname = re.sub(r"linux-headers-", "", kname)
        kname = re.sub(r"linux-source-", "", kname)
    if kname is not None:
        env['KERNELRELEASE'] = kname
        env.PrintProgress("KERNELRELEASE set: %s" % (kname))


def setup_variables(env: Environment):
    global _variables
    if _variables:
        _variables.Update(env)
        return
    _variables = env.GlobalVariables()
    _variables.Add('KERNELDIR', """
 Path to linux kernel headers for native builds of kernel modules.  Use
 $KERNELDIR_FOUND to use the path found by the kmake tool.
""", '$KERNELDIR_FOUND')
    _variables.Add('KERNELDIR_viper_arm', """
 Path to linux kernel headers for Viper:
""", '/opt/arcom/src/linux-source-2.6.35.9-ael1-viper')
    _variables.Add('KERNELDIR_titan_arm', """
 Path to linux kernel headers for Titan:
""", '/opt/arcom/src/linux-source-2.6.35.9-ael1-titan')
    _variables.Add('KERNELDIR_vulcan_armbe', """
 Path to linux kernel headers for Vulcan:
""", '/opt/arcom/src/linux-source-2.6.21.7-ael2-vulcan')
    _variables.Add('KERNELDIR_viper_armel', """
 Path to kernel headers for Viper/armel. See Debian package
 linux-headers-3.16.0-viper2:armel.
""", '/usr/src/linux-headers-3.16.0-viper2')
    _variables.Add('KERNELDIR_titan_armel', """
 Path to kernel headers for Titan/armel. See Debian package
 linux-headers-3.16.0-titan2:armel.
""", '/usr/src/linux-headers-3.16.0-titan2')
    # This isn't quite ready for cross building modules for armhf, RPi2.
    # linux-headers contains an executable scripts/genksyms/genksyms that is used
    # in building modules. The package we have on our Debian chroots has a genksyms
    # that is an armhf binary, meant for building on a RPi. Until that is sorted out,
    # don't set KERNELDIR_rpi2_armhf.
    _variables.Add('KERNELDIR_rpi2_armhf', """
 Path to kernel headers for Raspberry Pi2. See raspberrypi-kernel-headers
 package.
""", '/usr/src/linux-headers-4.4.11-v7n')
    _variables.Update(env)


def lookup_kerneldir(env: Environment):
    """
    Look for 'KERNELDIR_' + mach + '_' + arch.  If not found, look for
    KERNELDIR.
    """
    arch = env['ARCH']
    mach = env['MACH']
    kkey = 'KERNELDIR_' + mach + '_' + arch
    kdir = env.get(kkey)
    if not kdir:
        kdir = env.get('KERNELDIR')
    env.PrintProgress("KERNELDIR set: %s" % (kdir))
    return kdir


def generate(env: Environment):
    setup_variables(env)
    env['KERNELDIR'] = lookup_kerneldir(env)
    env.AddMethod(set_kernel_release, "SetKernelRelease")


def exists(env):
    return True
