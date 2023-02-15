"""
Configure a native nidas build.
"""
from SCons.Script import Environment


def generate(env: Environment):
    # this assumes that native builds are only done on x86_64 arch
    if not env.get('ARCHLIBDIR'):
        env['ARCHLIBDIR'] = 'lib64'

    env.AppendUnique(CCFLAGS = ['-g', '-fpic',
        '-Wformat', '-Werror=format-security', '-D_FORTIFY_SOURCE=2'])
    env.AppendUnique(CXXFLAGS = ['-Wextra','-Weffc++'])
    env.AppendUnique(CXXFLAGS=['-std=c++11'])

    # hardening option "-pie" in LINKFLAGS results in this error:
    # /usr/bin/ld: /opt/local/lib/libraf++.a(PMSspex.o): relocation R_X86_64_32
    # against `.rodata.str1.1' can not be used when making a shared object; recompile with -fPIC
    env.AppendUnique(LINKFLAGS = ['-Wl,-z,relro', '-Wl,-z,now'])


def exists(env):
    return True
