# NIDAS Systemd Service Units

## Setting executable capabilities

NIDAS processes can use file-based capabilities to add privileges rather than
running as root.  If dsm_server has the CAP_SYS_NICE capability, it can
schedule threads to run with real-time priority, which may help provide
complete data recovery on a heavily loaded system. If dsm_server does not have
CAP_SYS_NICE, it will still run, but without real-time priority.

The documentation (man 7 capabilities) also mentions that CAP_NET_ADMIN
is needed enable multicasting, which dsm_server may be configured to use,
if an input socket is set to type "mcaccept".

If dsm_server is installed from the nidas RPM, these capabiltiies
are enabled on the executable file. To check the capabilities:

    getcap /opt/nidas/bin/dsm_server
    dsm_server = cap_sys_nice,cap_net_admin+p

If the executable file has been over-written, or the ownership changed,
you will have to add the capabilities:

    sudo setcap cap_sys_nice,cap_net_admin+p /opt/nidas/bin/dsm_server

## Systemd services for non-root users

systemd is an alternative to SysV init scripts.  It is supported on RedHat
systems starting with RHEL7 (CentOS 7) and Fedora 15.

Instead of using a script on `/etc/init.d` to start dsm_server, it can be
started by systemd at boot time to run from a non-root user's account.

However, as of `systemd-208-11.el7_0.6.x86_64` on CentOS7 we can't seem to get
processes to start successfully with the `--user` option.

## User setup

The user must be a login user, and it's shell in `/etc/passwd` must be a
legitimate shell, not something like `/bin/false` or `/sbin/nologin`.

From the user's account, if it has sudo privileges:

    sudo loginctl enable-linger $USER

or as root:

    loginctl enable-linger {username}

After this is done, all the following commands are done from the user's
account, not the root account or with sudo.

## Install dsm_server.service systemd unit for user

When logged in as the user (not root), copy `dsm_server.service` file to
`$HOME/.local/share/systemd/user` where systemd can find it.

    mkdir -p $HOME/.local/share/systemd/user 
    cp /opt/nidas/systemd/user/dsm_server.service $HOME/.local/share/systemd/user 

Or use this command to link the unit file into the user unit search path,
likely the same location as above:

    systemctl --user link /opt/nidas/systemd/user/dsm_server.service

## Setup user environment and add options to dsm_server

Any required environment variables must be imported to the
process started by systemd. This can be done in any of several
ways. See "man system.exec" for more info.

- Add the variable assignments to the "Environment=" option in the
  `dsm_server.service` unit file on `$HOME/.local/share/systemd/user`.

- Place the variable definitions in a separate file, and specify the file
  with the "EnvironmentFile=" option in the service unit file:
  `EnvironmentFile=/myproject/systemd.env`

- Create a directory called
  $HOME/.local/share/systemd/user/dsm_server.service.d, and
  a file in that directory with a name extension ".conf"
  In that file, set the environment, using the unit file syntax,
  for example:
  [Service]
  Environment=XENV=test1 YENV=test2
  EnvironmentFile=/myproject/systemd.env

- Using a ExecStartPre option in dsm_server.service, setup the environment by
  executing a login shell for the user, and from that shell, run `systemctl
  --user import-environment ...`.  This is the method used in the example
  `dsm_server.service` file.

  If the user's shell is sh/bash, where the required environment variables are
  set in `$HOME/.profile` or `$HOME/.bash_profile` and `$HOME/.bashrc`, then
  uncomment the line starting with "ExecStartPre=/usr/bin/bash", and comment
  the line starting with "ExecStartPre=/usr/bin/tcsh".

  Do the reverse, if the user's shell is csh/tcsh, where the required
  environment variables are set in `$HOME/.login` and `$HOME/.cshrc`.

  Then in the ExecStartPre command, update the list of environment
  variables that are needed by the dsm_server process. This list
  should include all environment variables used within the NIDAS XML.

  Make sure you have no syntax or run-time errors in the shell startup files,
  `.bash_profile`, `.login`, etc.

The system administrator can also set options for all systemd users in the
files:

- `/etc/systemd/user.conf`
- `/etc/systemd/system/user@.service.d/*.conf`

Also see `systemctl --user edit dsm_server.service`, which automatically
creates an override file, if necessary, and opens it in an editor.

## Change dsm_server runstring options

If necessary, the `dsm_server` command-line arguments can be changed by
editing the ExecStart line.  Add to it any runstring options that need to be
passed to `dsm_server`.

## Commands to control the service

### Enable the service

Enable `dsm_server.service` with systemd, from the user's account:

    systemctl --user daemon-reload
    systemctl --user enable dsm_server

### Start the service

    systemctl --user start dsm_server

### Disable and stop dsm_server

    systemctl --user disable dsm_server
    systemctl --user stop dsm_server

### Check the status

    systemctl --user status -l dsm_server

To check status of the user's "lingering" processes:

    systemctl status user@$UID
