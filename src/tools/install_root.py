"""
Override Install methods to insert a build install prefix in front of the
usual prefix, such as when installing into a temporary build directory to
assemble a package.
"""
# This uses the same approach as the eol_scons prefixoptions tool, so they
# probably shouldn't be used together.  And these specifically do not add an
# automatic alias, so the caller can assign alternate aliases like install and
# install.root.

import traceback
from SCons.Script import Environment, Variables, PathVariable


_variables: Variables
_variables = None


def _setup_variables(env: Environment):
    global _variables
    if _variables is None:
        _variables = env.GlobalVariables()
        # These are sort-of related, so just include them in this one tool.
        _variables.AddVariables(PathVariable('INSTALL_ROOT',
                                'path to be prepended to all install paths',
                                '', PathVariable.PathAccept))
        _variables.AddVariables(PathVariable('SYSCONFIGDIR','/etc installation path',
                                '/etc', PathVariable.PathAccept))
        _variables.AddVariables(PathVariable('PKGCONFIGDIR',
                                'system dir to install nc_server.pc',
                                '/usr/$ARCHLIBDIR/pkgconfig',
                                PathVariable.PathAccept))
    _variables.Update(env)


def Install(self: Environment, dest: str, *args, **kw):
    """Add 'install' alias to targets created with standard Install() method."""
    # the first argument is always the destination, but this assumes it is a
    # string.
    dest = "${INSTALL_ROOT}" + dest
    return self._install_root_Install(dest, *args, **kw)


def InstallAs(self: Environment, dest: str, *args, **kw):
    """Add 'install' alias to targets created with standard Install() method."""
    dest = "${INSTALL_ROOT}" + dest
    return self._install_root_Install(dest, *args, **kw)


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
