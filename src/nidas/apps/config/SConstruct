# -*- python -*-
#
# This is a SConstruct file for building the config editor 'outside' the
# NIDAS source tree.  In other words, it does not need to load in the
# entire nidas source tree of SConscript files, it only loads the
# SConscript file for the config editor inside this directory.  The 'nidas'
# tool automatically selects the installed NIDAS libraries and header
# files.  However, since in that case this is the top level directory, this
# directory must contain a link to a site_scons directory.

# Because of this file, it does not work to build from this directory
# inside the nidas source tree using 'scons -u', since that will find this
# SConstruct file instead of the one up in nidas/src.

# Builds within the nidas source tree still work as before.  The
# nidas/apps/SConscript file loads the SConscript file in this directory,
# and in that case the 'nidas' tool compiles against the libraries and
# headers inside the nidas source tree.  This is similar to other nidas
# apps, except this app requires the jlocal and qt4 tools also.

# The idea is that the logical compile dependencies of the config editor
# are abstracted in one place, inside the SConscript file in this
# directory.  They do not need to be duplicated, and they work when
# building within the nidas source as well as when building against an
# installed nidas.

env = Environment(tools = ['default'])

SConscript("SConscript", exports={"env":env})
