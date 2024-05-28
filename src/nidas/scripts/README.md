
# NIDAS Utility Scripts

This directory is for utility scripts which are installed alongside the NIDAS
applications from [NIDAS Apps](../apps) into the NIDAS bin directory.

## setup_nidas.sh

This script can add and remove NIDAS installations in the paths in the current
shell. Run with `-h` or `--help` to see usage.

It can be helpful to switch easily between alternate NIDAS installations, such
as between the system package installation (typically under `/opt/nidas`) and a
development version.  This is especially helpful on embedded targets.
Development versions can be installed and updated with
[dsm_rsync.sh](../../../scripts/dsm_rsync.sh), and then
[setup_nidas.sh](setup_nidas.sh) can be used to switch between production and
development in the current shell.

## dsm.init

This is the SysV init script originally installed as part of the `nidas-daq`
package.  The `nidas-daq` package has been deprecated, but this script is
still installed into the bin directory so it is available on systems which
need it.  For example, it can be copied or linked into the `/etc/init.d` tree.
