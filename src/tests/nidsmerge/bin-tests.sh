#!/bin/sh

#This runs some basic tests on the nidsmerge binary to check for valid operation

#clean out any old outputs
rm -fr dsm12.out.dat dsm13.out.dat dsm-both.dat

#Given some simple sample files (data is irrelevent for these tests) merge into a single file
nidsmerge -i dsm12.dat -i dsm13.dat -o dsm-both.dat 1>/dev/null 2>/dev/null || exit "Cannot create output merged file"

#output must be different than either input
diff dsm12.dat dsm-both.dat 1>/dev/null 2>/dev/null && exit "created dsm-both is the same as DSM12"
diff dsm13.dat dsm-both.dat 1>/dev/null 2>/dev/null && exit "created dsm-both is the same as DSM13"

nidsmerge -i dsm-both.dat -d 12 -o dsm12.out.dat 1>/dev/null 2>/dev/null || exit "Cannot create dsm12 output merged file"
nidsmerge -i dsm-both.dat -d 13 -o dsm13.out.dat 1>/dev/null 2>/dev/null || exit "Cannot create dsm13 output merged file"

#do a bare diff between dsm##.dat and dsm##.out.dat to make sure they are the same

diff dsm12.dat dsm12.out.dat || exit "dsm12 outputs differ, yet shouldnt"
diff dsm13.dat dsm13.out.dat || exit "dsm13 outputs differ, yet shouldnt"

rm -fr dsm12.out.dat dsm13.out.dat dsm-both.dat

echo "[Success] The '-d' flag seems to be operational"
