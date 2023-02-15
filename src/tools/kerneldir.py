"""
SCons tool to add configuration variables to set the KERNELDIR according to
various architectures and target machines.  There are configuration variables
to set the defaults for each, and then a single KERNELDIR variable which
specifies the actual path to use for the builders.
"""

from SCons.Script import Variables, Environment, Export


_variables: Variables
_variables = None


def generate(env: Environment):
    global _variables
    if _variables:
        _variables.Update(env)
    _variables = env.GlobalVariables()
    _variables.Add('KERNELDIR', '\n'
        '  path to linux kernel headers for building modules.\n'
        '  Use $KERNELDIR_FOUND to use the path found by the kmake tool.',
        '$KERNELDIR_FOUND')
    _variables.Add('KERNELDIR_viper_arm',
        'Default path to linux kernel headers for Viper:',
        '/opt/arcom/src/linux-source-2.6.35.9-ael1-viper')
    _variables.Add('KERNELDIR_titan_arm',
        'Default path to linux kernel headers for Titan:',
        '/opt/arcom/src/linux-source-2.6.35.9-ael1-titan')
    _variables.Add('KERNELDIR_vulcan_armbe',
        'Default path to linux kernel headers for Vulcan:',
        '/opt/arcom/src/linux-source-2.6.21.7-ael2-vulcan')
    _variables.Add('KERNELDIR_viper_armel',
        'Default path to kernel headers for Viper/armel. See Debian package linux-headers-3.16.0-viper2:armel',
        '/usr/src/linux-headers-3.16.0-viper2')
    _variables.Add('KERNELDIR_titan_armel',
        'Default path to kernel headers for Titan/armel. See Debian package linux-headers-3.16.0-titan2:armel',
        '/usr/src/linux-headers-3.16.0-titan2')
    # This isn't quite ready for cross building modules for armhf, RPi2.
    # linux-headers contains an executable scripts/genksyms/genksyms that is used
    # in building modules. The package we have on our Debian chroots has a genksyms
    # that is an armhf binary, meant for building on a RPi. Until that is sorted out,
    # don't set KERNELDIR_rpi2_armhf.
    _variables.Add('KERNELDIR_rpi2_armhf',
        'Default path to kernel headers for Raspberry Pi2. See raspberrypi-kernel-headers package.',
        '/usr/src/linux-headers-4.4.11-v7n')
    _variables.Update(env)


def exists(env):
    return True
