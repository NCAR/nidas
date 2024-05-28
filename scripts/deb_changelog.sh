#!/bin/bash

# Use output of git log and git describe to create a debian changelog

# Debian "native" source packages can't have a hyphen in the version
# such as 1.2-14, because that indicates there is an upstream
# tarball *-1.2.orig.tar.gz.
# So when git describe returns v1.2-14-g..., that gets converted
# to a debian version 1.2+14.

awkcom=`mktemp /tmp/${script}_XXXXXX.awk`
trap "{ rm -f $awkcom; }" EXIT

# In the changelog, copy most recent commit subject lines up to a max of
# 25.  The previous approach only included commits since a tag of the form
# vN.N, but that fails when releasing the actual vN.N version, because then
# the changelog is empty and debuild does not like that.

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

# create Debian changelog from last 25 git log messages.  Put SHA hash by
# itself on first line. Above awk script then runs git describe on that
# hash in order to get a X.Y-Z version.  The final sed replaces the
# remaining (vN.N) with (N.N), since the awk script only fixes
# (vN.N-N-hash)
git log --max-count=25 --date-order --format="%H%nnidas%n  * (XXXXXXX) %s%n -- %aN <%ae>  %cD" --date=local | gawk -f $awkcom | sed -e 's/([vV]\([0-9][0-9]*\.[0-9][0-9]*[.0-9]*\))/(\1)/g'
