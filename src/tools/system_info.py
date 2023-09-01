"""
Module for system information like OS, arch, and Debian multiarch.

SCons does setup some host platform info in the construction environment, but
they are not quite specific enough for this purpose.  HOST_OS is 'posix',
HOST_ARCH is usually the same as platform.machine(), and PLATFORM is also
posix.  So this tool uses the /etc/os-release file to get some more specific
OS name and release info, in addition to the host architecture.  Similar info
is available through  platform.freedesktop_os_release(), but since Python
3.10.
"""

import platform
from pathlib import Path
import re

import os
import subprocess as sp


_os = None


def _parse_settings(contents: str):
    settings = {}
    rx = re.compile(r'^(?P<key>[^=]+)="?(?P<value>[^"]+)"?$')
    for line in contents.splitlines():
        rp = rx.match(line.strip())
        if rp:
            settings[rp.group('key')] = rp.group('value')
    return settings


def _os_from_settings(settings):
    os_id = settings.get('ID', '')
    os_release = settings.get('VERSION_ID', '')
    return (os_id + os_release).lower()


def get_os():
    """
    Return a string identifying host OS using parameters from the
    /etc/os-release file.  The form is <ID><VERSION_ID>, like fedora37 or
    raspbian10.
    """
    global _os
    if _os is None:
        ospath = Path("/etc/os-release")
        settings = {}
        if ospath.exists():
            settings.update(_parse_settings(ospath.read_text()))
        _os = _os_from_settings(settings)
    return _os or None


def get_arch():
    arch = get_debian_multiarch()
    if arch:
        arch = arch.split('-')[0]
    else:
        arch = platform.machine()
    return arch


def dpkg_arch(name):
    try:
        return sp.check_output(["dpkg-architecture", "-q", name],
                               universal_newlines=True).strip()
    except sp.CalledProcessError:
        return None


def get_debian_multiarch(_env=None):
    "If on Debian and a multiarch setting can be found, return it."
    osid = get_os()
    debian = (osid.startswith('debian') or osid.startswith('raspbian') or
              osid.startswith('ubuntu'))
    multiarch = None
    if debian:
        # On Debian systems, need to figure out the right suffix to the nidas
        # conf file for ld.so.conf.d.  DEB_HOST_MULTIARCH takes precedence
        # over DEB_HOST_GNU_TYPE, and an environment setting takes precedence
        # over querying dpkg-architecture.
        multiarch = os.environ.get('DEB_HOST_MULTIARCH')
        multiarch = multiarch or os.environ.get('DEB_HOST_GNU_TYPE')
        multiarch = multiarch or dpkg_arch("DEB_HOST_MULTIARCH")
        multiarch = multiarch or dpkg_arch("DEB_HOST_GNU_TYPE")
    return multiarch


# These are examples of the /etc/os-release file on several hosts.  It looks
# like ID and VERSION_ID are the most useful and universal parameters to
# identify the OS.
centos7 = """
NAME="CentOS Linux"
VERSION="7 (Core)"
ID="centos"
ID_LIKE="rhel fedora"
VERSION_ID="7"
PRETTY_NAME="CentOS Linux 7 (Core)"
ANSI_COLOR="0;31"
CPE_NAME="cpe:/o:centos:centos:7"
HOME_URL="https://www.centos.org/"
BUG_REPORT_URL="https://bugs.centos.org/"

CENTOS_MANTISBT_PROJECT="CentOS-7"
CENTOS_MANTISBT_PROJECT_VERSION="7"
REDHAT_SUPPORT_PRODUCT="centos"
REDHAT_SUPPORT_PRODUCT_VERSION="7"
"""

raspbian10 = """
PRETTY_NAME="Raspbian GNU/Linux 10 (buster)"
NAME="Raspbian GNU/Linux"
VERSION_ID="10"
VERSION="10 (buster)"
VERSION_CODENAME=buster
ID=raspbian
ID_LIKE=debian
HOME_URL="http://www.raspbian.org/"
SUPPORT_URL="http://www.raspbian.org/RaspbianForums"
BUG_REPORT_URL="http://www.raspbian.org/RaspbianBugs"
"""

fedora37 = """
NAME="Fedora Linux"
VERSION="37 (Workstation Edition)"
ID=fedora
VERSION_ID=37
VERSION_CODENAME=""
PLATFORM_ID="platform:f37"
PRETTY_NAME="Fedora Linux 37 (Workstation Edition)"
ANSI_COLOR="0;38;2;60;110;180"
LOGO=fedora-logo-icon
CPE_NAME="cpe:/o:fedoraproject:fedora:37"
DEFAULT_HOSTNAME="fedora"
HOME_URL="https://fedoraproject.org/"
DOCUMENTATION_URL="https://docs.fedoraproject.org/en-US/fedora/f37/system-administrators-guide/"
SUPPORT_URL="https://ask.fedoraproject.org/"
BUG_REPORT_URL="https://bugzilla.redhat.com/"
REDHAT_BUGZILLA_PRODUCT="Fedora"
REDHAT_BUGZILLA_PRODUCT_VERSION=37
REDHAT_SUPPORT_PRODUCT="Fedora"
REDHAT_SUPPORT_PRODUCT_VERSION=37
SUPPORT_END=2023-11-14
VARIANT="Workstation Edition"
VARIANT_ID=workstation
"""

debian10 = """
PRETTY_NAME="Debian GNU/Linux 10 (buster)"
NAME="Debian GNU/Linux"
VERSION_ID="10"
VERSION="10 (buster)"
VERSION_CODENAME=buster
ID=debian
HOME_URL="https://www.debian.org/"
SUPPORT_URL="https://www.debian.org/support"
BUG_REPORT_URL="https://bugs.debian.org/"
"""


def test_os_release():
    assert _os_from_settings(_parse_settings(fedora37)) == 'fedora37'
    assert _os_from_settings(_parse_settings(raspbian10)) == 'raspbian10'
    assert _os_from_settings(_parse_settings(debian10)) == 'debian10'
    assert _os_from_settings(_parse_settings(centos7)) == 'centos7'
