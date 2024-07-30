"""
Provide a Configure context where all settings are added to a single Config.h
file under the variant dir, along with all temporary the files and logs.

All the configure checks that were in one place, in nidas/SConscript, have
been moved to the SConscript files where the dependencies are needed.  For
example, all the lower-level system header checks are in util.  Environments
which need a configure context can apply the nidasconfig tool to get the
NidasConfigureContext method.
"""
from SCons.Script import Environment

configh = None


def NidasConfigHeader(env):
    """
    Return the Config.h header node written by NidasConfigure() contexts.
    """
    global configh
    if not configh:
        configh = env.File("$VARIANT_DIR/include/nidas/Config.h")
    return configh


def NidasConfigure(env: Environment, **kw):
    """
    Return a Configure context whose settings will be appended to a single
    global config header file.  This does not create a temporary environment,
    since the settings and build flags for all the config checks are meant to
    be added to the given environment.  SConscripts only need to use this if
    the configure settings need to be added to Config.h.  However, all
    SConscript files have to be careful to put the configure files under
    VARIANT_DIR, so configuration for different variants is cached separately.
    """
    configh = NidasConfigHeader(env)
    conf = env._save_Configure(env, conf_dir="$VARIANT_DIR/.sconf_temp",
                               log_file='$VARIANT_DIR/config.log',
                               config_h=configh, **kw)
    return conf


def Configure(env: Environment, **kw):
    """
    Override the regular Configure, where the setting does not need to be
    written into the config header.
    """
    conf = env._save_Configure(env, conf_dir="$VARIANT_DIR/.sconf_temp",
                               log_file='$VARIANT_DIR/config.log',
                               **kw)
    return conf


def generate(env: Environment):
    env.AddMethod(NidasConfigHeader)
    env.AddMethod(NidasConfigure)
    env._save_Configure = Environment.Configure
    env.AddMethod(Configure, "Configure")


def exists(env: Environment):
    return True
