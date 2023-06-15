"""
Check for supported -fstack-protector options in compiler.
"""

from SCons.Script import Configure, Environment


def CheckStackProtectorOptions(env: Environment):
    def add_stack_option(env, opt):
        test_env = env.Clone()
        test_env.AppendUnique(CCFLAGS = [opt])
        conf = test_env.NidasConfigure()
        env.PrintProgress('Checking gcc option ' + opt + '...')
        ok = conf.CheckCC()
        conf.Finish()
        if ok:
            env.AppendUnique(CCFLAGS = [opt])
        return ok
    for opt in ['-fstack-protector-strong', '-fstack-protector']:
        if add_stack_option(env, opt):
            break
    # On ubuntu20 adding this option causes this warning:
    # cc1plus: warning: '-fstack-check=' and '-fstack-clash_protection' are mutually exclusive.  Disabling '-fstack-check='
    # So leave it out.
    #add_stack_option(env, '-fstack-check')


def generate(env):
    CheckStackProtectorOptions(env)


def exists(env):
    return True
