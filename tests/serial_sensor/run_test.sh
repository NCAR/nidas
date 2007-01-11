#!/bin/sh

if true; then
# build the local sensor_sim program
cd src
scons
cd ..

[ -d tmp ] && rm -rf tmp
[ -d tmp ] || mkdir tmp

# start sensor simulations on pseudo-terminals
src/sensor_sim -f data/test.dat -r 10 -s 15 tmp/test0 &
src/sensor_sim -f data/test.dat -b $'\e' -r 10 -s 15 tmp/test1 &
src/sensor_sim -c -s 15 -r 60 tmp/test2 &
src/sensor_sim -c -s 15 -r 60 tmp/test3 &

# start data collection
# valgrind dsm -d config/test.xml
dsm -d config/test.xml

fi

# check data
ofiles=(tmp/test_*)
if [ ${#ofiles[*]} -ne 1 ]; then
    echo "Expected one output file, got ${#ofiles[*]}"
    exit 1
fi

statsf=tmp/data_stats.out
data_stats $ofiles > $statsf

nsensors=`egrep "^test:tmp/test" $statsf | wc | awk '{print $1}'`
if [ $nsensors -ne 4 ]; then
    echo "Expected 4 sensors in $statsf, got $nsensors"
    exit 1
fi

nsamps=(51 50 256 2982)
ok=1
for (( i = 0; i < 4; i++)); do
    sname=test$i
    nsamp=${nsamps[$i]}
    awk -v nsamp=$nsamp "
/^test:tmp\/$sname/{
    if (\$4 != nsamp) {
        print \"sensor $sname, nsamps=\" \$4 \", should be \" nsamp
        exit(1)
    }
}
" $statsf || ok=0
done

if [ $ok -ne 1 ]; then
    echo "test failed"
    exit 1
else
    echo "test OK"
    exit 0
fi

