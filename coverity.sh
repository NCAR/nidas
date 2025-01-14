# Source this file to get coverity variables and functions.

# Most of the coverity configuration is now in the coverity.yaml file in each
# source tree.  This script is used by jenkins.sh to consolidate the coverity
# setup, but it can also be sourced for us in an interactive shell, especially
# to add coverity to the PATH.  For now it is duplicated in the ACS, ASPEN,
# and isf-software-security repositories.

COVURL="${COVURL:-https://snoopy.eol.ucar.edu/coverity/}"
COVAUTH="${COVAUTH:-${HOME}/.cov-auth-key.txt}"
STREAM="${STREAM:-ACS}"
COVBIN="${COVBIN:-/opt/local/cov-analysis/bin}"
COVERITY="${COVERITY:-coverity}"
export PATH=${COVBIN}:${PATH}
COVREPORTSBIN="${COVREPORTSBIN:-/opt/local/cov-reports/bin}"
COVREPORT="${COVREPORT:-cov-generate-disa-stig-report}"
export PATH=${COVREPORTSBIN}:${PATH}
ENABLE_COVERITY="${ENABLE_COVERITY:-1}"

COVRUN=
if [ $ENABLE_COVERITY -eq 0 ]; then
    COVRUN="echo Skipping:"
fi

# Capture source files
coverity_capture() {
    (set -x
     ${COVRUN} rm -rf idir
     ${COVRUN} ${COVERITY} capture
    )
}

coverity_analyze() {
    (set -x; ${COVRUN} ${COVERITY} analyze)
}

# Commit an analysis
coverity_commit() {
    # use --tags to get the latest ACS tag, since those tags are not
    # annotated, and hopefully that will still get an appropriate aspen tag.
    (set -x
        ${COVRUN} ${COVERITY} commit \
        -o "commit.connect.auth-key-file=${COVAUTH}" \
        -o "commit.connect.version=`git describe --tags --long`"
    )
}

coverity_report_name() # config-file
{
    config="$1"
    if [ -z "$config" ]; then
        config=/dev/null
    fi
    # could get fancy and use something like yq here, but this avoids
    # dependency.
    name=`grep -E '^\s*project-name:' $config | awk '{print $2;}'`
    version=`grep -E '^\s*project-version:' $config | awk '{print $2;}'`
    output="${name}-${version}-DISA-STIG.pdf"
    echo $output
}

coverity_report() # report.yaml
{
    config="$1"
    if [ -z "$config" ]; then
        echo "usage: coverity_report config-file"
        return 1
    fi
    user="--user ${USER}"
    auth="--auth-key-file ${COVAUTH}"
    output="--output `coverity_report_name $config`"
    (set -x; ${COVRUN} ${COVREPORT} $auth $user $output $config)
}
