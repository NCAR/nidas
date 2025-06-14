# sh functions for test scripts

check_executable() # name
{
    name="$1"
    where=`which $name`
    if [ $? -ne 0 -o ! -x "$where" ]; then
        cat <<EOF
Executable not found: $name

If this test is being run by scons, the runtime environment should be
setup to find and run this executable.  If being run from the terminal,
first setup the environment to run against like so:

source /opt/local/nidas/bin/setup_nidas.sh
EOF
        exit 1
    fi
    echo "$name executable: $where"
    echo VARIANT_DIR="${VARIANT_DIR}"
    echo PATH="$PATH"
    echo LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
    echo "nidas libaries:"
    ldd $where | grep -F libnidas
}


valgrind_errors() {
    grep -E -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

sed_times()
{
    # truncate the last two digits anything that looks like a timestamp with
    # fractional seconds.
    sed -E -e 's/([0-9][0-9]:[0-9][0-9]:[0-9][0-9]\.[0-9]+)[0-9][0-9] /\1 /'
}

compare_outputs() # reffile outfile
{
    reffile="$1"
    outfile="$2"
    diff --side-by-side --width=200 --suppress-common-lines "${reffile}" "${outfile}"
    if [ $? -ne 0 ]; then
        echo "Compared $reffile to $outfile ..."
        echo "*** Output differs: $*"
        if [ -z "$SCONS_KEEP_GOING" ]; then
            exit 1
        fi
    fi
    echo "Comparing $reffile to $outfile: success."
}

compare() # reffile
{
    reffile="$1"
    if [ -f "baseline/$reffile" ]; then
        reffile="baseline/$reffile"
    fi
    outfile=outputs/`basename "$reffile"`
    shift
    test -d outputs || mkdir outputs
    rm -f "$outfile"
    (set -x; "$@" > "$outfile" 2> "${outfile}.stderr")
    if [ $? -ne 0 ]; then
        echo "*** Non-zero exit status: $*"
        cat "${outfile}.stderr"
        exit 1
    fi
    compare_outputs "$reffile" "$outfile"
}

failed() # message
{
    echo "FAILED: $@"
    exit 1
}
