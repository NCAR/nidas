#!/bin/bash

# Use output of git log and git describe to create a debian changelog

# Debian "native" source packages can't have a hyphen in the version
# such as 1.2-14, because that indicates there is an upstream
# tarball *-1.2.orig.tar.gz.
# So when git describe returns v1.2-14-g..., that gets converted
# to a debian version 1.2+14.

awkcom=`mktemp /tmp/${script}_XXXXXX.awk`
trap "{ rm -f $awkcom; }" EXIT

# In the changelog, copy most recent commit subject lines
# since this tag (max of 100).
# sincetag=v1.0

# to get the second-most recent tag of the form: vN.  using the most recent
# means the changelog will be empty when building exactly that version, and
# debuild fails.
sincetag=$(git tag -l --sort=version:refname "[vV][0-9]*" | tail -n 2 | head -1)

if ! gitdesc=$(git describe --match "v[0-9]*"); then
    echo "git describe failed, looking for a tag of the form v[0-9]*"
    exit 1
fi

# example output of git describe: v2.0-14-g9abcdef

# awk script to convert output of git log to changelog format
# run git describe on each hash to create a version
cat << \EOD > $awkcom
/^[0-9a-f]{7}/ {
    cmd = "git describe --match '[vV][0-9]*' " $0 " 2>/dev/null"
    res = (cmd | getline gitdesc)
    close(cmd)
    if (res == 0) {
        gitdesc = ""
    }
    else {
        # print "gitdesc=" gitdesc
        hash = gensub(".*-g([0-9a-f]+)","\\1",1,gitdesc)
        # convert v2.0-14-g9abcdef to 2.0+14
        version = gensub("^v([^-]+)-([0-9]+)-.*$","\\1+\\2",1,gitdesc)
        # print "version=" version ",hash=" hash
    }
}
/^nidas/ { print $0 " (" version ") stable; urgency=low" }
# truncate "*" line to 74 characters, 14 + max of 60 from commit subject
/^  \*/ { out = gensub("XXXXXXX",hash,1); print substr(out,1,74);}
/^ --/ { print $0 }
/^$/ { print $0 }
EOD

# create Debian changelog from git log messages since the tag $sincetag.
# Put SHA hash by itself on first line. Above awk script then
# runs git describe on that hash in order to get a X.Y-Z version.
git log --max-count=100 --date-order --format="%H%nnidas%n  * (XXXXXXX) %s%n -- %aN <%ae>  %cD" --date=local ${sincetag}.. | awk -f $awkcom

