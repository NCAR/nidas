"""
Override Install methods to insert a build install prefix in front of the
usual prefix, such as when installing into a temporary build directory to
assemble a package.
"""
# This uses the same approach as the eol_scons prefixoptions tool, so they
# probably shouldn't be used together.  And these specifically do not add an
# automatic alias, so the caller can assign alternate aliases like install and
# install.root.

from pathlib import Path
from SCons.Script import Environment, Variables, PathVariable
import system_info as si


_variables: Variables
_variables = None


# This tool also figures out the install path for pkg-config files, and it
# creates a variable for that path called PKGCONFIGDIR.  The default for that
# path depends on whether this is a debian cross-build, as indicated by the
# multiarch setting, or a native build.  If a native build, and a
# /usr/lib64/pkgconfig path exists, then this assumes that should be the
# default PKGCONFIGDIR, ie, the lib64 path would not exist on a 32-bit native
# system.  For Debian multiarch builds, the multiarch pkgconfig path is both
# the default for PKGCONFIGDIR and also the first directory in the default
# PKG_CONFIG_PATH.  Even though PKG_CONFIG_PATH is not related to
# installation, the variable is created here since the default depends on
# PKGCONFIGDIR.

# Just for reference, it seems reasonable to get the directory for installing
# pkg-config files from pkg-config itself:
#
# $ pkg-config --variable pc_path pkg-config
# /usr/lib64/pkgconfig:/usr/share/pkgconfig
#
# I'm not sure which versions of pkg-config support that, but it could be
# useful if at some point it is unreasonable to derive the default here.

# This tool is loaded after the BUILD target is set, so it should be possible
# to derive different defaults based on the target.  However, at present that
# is not needed.  All redhat variants are native builds, and for debian vortex
# builds, the multiarch setting can queried from the system using
# si.get_debian_multiarch().


def _setup_variables(env: Environment):
    global _variables
    if _variables is None:

        multiarch = si.get_debian_multiarch()
        pkgconfigdir = '/usr/lib/pkgconfig'
        # Use the standard default unless it needs to be overridden for a
        # cross-compile.
        pkgconfigpath = None
        if multiarch:
            pkgconfigdir = f'/usr/lib/{multiarch}/pkgconfig'
            pkgconfigpath = f'{pkgconfigdir}'
            pkgconfigpath += ':/usr/lib/pkgconfig:/usr/share/pkgconfig'
        elif Path("/usr/lib64/pkgconfig").exists():
            pkgconfigdir = "/usr/lib64/pkgconfig"

        _variables = env.GlobalVariables()
        # These are sort-of related, so just include them in this one tool.
        _variables.AddVariables(
            PathVariable('INSTALL_ROOT',
                         'path to be prepended to all install paths',
                         '', PathVariable.PathAccept))
        _variables.AddVariables(
            PathVariable('SYSCONFIGDIR', '/etc installation path',
                         '/etc', PathVariable.PathAccept))
        _variables.AddVariables(
            PathVariable('PKGCONFIGDIR', 'system dir to install nidas.pc',
                         pkgconfigdir, PathVariable.PathAccept))
        _variables.Add('PKG_CONFIG_PATH', "Path to pkg-config files, "
                       "otherwise use system default.", pkgconfigpath)

    _variables.Update(env)


def Install(self: Environment, dest: str, *args, **kw):
    # the first argument is always the destination, but this assumes it is a
    # string.
    dest = "${INSTALL_ROOT}" + dest
    return self._install_root_Install(dest, *args, **kw)


def InstallAs(self: Environment, dest: str, *args, **kw):
    dest = "${INSTALL_ROOT}" + dest
    return self._install_root_InstallAs(dest, *args, **kw)


def generate(env: Environment):
    _setup_variables(env)
    if getattr(env, '_install_root_Install', None):
        return
    env._install_root_Install = env.Install
    env._install_root_InstallAs = env.InstallAs
    env.AddMethod(Install)
    env.AddMethod(InstallAs)


def exists(env: Environment):
    return True
