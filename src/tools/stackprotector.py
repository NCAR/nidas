"""
Check for supported -fstack-protector options in compiler.
"""

from SCons.Script import Environment


_stack_option = None


def CheckStackProtectorOptions(env: Environment):
    def add_stack_option(env, opt):
        # make sure no LIBS set which might trigger other targets to be built,
        # since no libs are needed for this check.
        test_env = env.Clone(LIBS=[])
        test_env.AppendUnique(CCFLAGS=[opt])
        conf = test_env.NidasConfigure()
        env.PrintProgress('Checking gcc option ' + opt + '...')
        ok = conf.CheckCC()
        conf.Finish()
        if ok:
            global _stack_option
            _stack_option = opt
        return ok
    for opt in ['-fstack-protector-strong', '-fstack-protector']:
        if add_stack_option(env, opt):
            break
    global _stack_option
    if _stack_option is None:
        _stack_option = False
    # On ubuntu20 adding this option:
    #
    # add_stack_option(env, '-fstack-check')
    #
    # causes this warning:
    #
    # cc1plus: warning: '-fstack-check=' and '-fstack-clash_protection' are
    # mutually exclusive.  Disabling '-fstack-check='
    #
    # So leave it out.


def generate(env: Environment):
    if _stack_option is None:
        CheckStackProtectorOptions(env)
    if _stack_option:
        env.AppendUnique(CCFLAGS=[_stack_option])


def exists(env):
    return True
