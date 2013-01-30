#!/bin/sh
pg_dump --insert -h shiraz -U ads calibrations | grep " 'bath', " | psql -h thumper -U ads -d calibrations
pg_dump --insert -h shiraz -U ads calibrations | grep " 'bath', " | psql -h hyper.raf-guest.ucar.edu -U ads -d calibrations
pg_dump --insert -h shiraz -U ads calibrations | grep " 'bath', " | psql -h hercules.raf-guest.ucar.edu -U ads -d calibrations
