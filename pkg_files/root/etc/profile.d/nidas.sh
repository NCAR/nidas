if ! echo ${PATH} | /bin/grep -q /opt/nidas/bin ; then
    # Uncommment this if you want nidas programs in PATH for all login users
    # PATH=${PATH}:/opt/nidas/bin
    :
fi
