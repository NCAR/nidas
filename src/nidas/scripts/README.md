
# NIDAS Utility Scripts

This directory is for utility scripts which are installed alongside the NIDAS
applications in [NIDAS Apps](../apps).

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
