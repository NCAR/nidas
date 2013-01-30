#!/bin/sh

master="shiraz"
ping -c 1 $master
if [[ $? -ne 0 ]] ; then
  echo "Master calibration database server '"$master"' is offline."
  exit
fi

for server in thumper hyper.raf-guest.ucar.edu hercules.raf-guest.ucar.edu; do
  echo "Pushing bath cals to:" $server
  ping -c 1 $server
  if [[ $? -eq 0 ]] ; then
    pg_dump --insert -h $master -U ads calibrations | grep " 'bath', " | psql -h $server -U ads -d calibrations
    echo "Pushed bath cals to:" $server
  else
    echo "Failed to push bath cals to:" $server "(no connection)."
  fi
done
