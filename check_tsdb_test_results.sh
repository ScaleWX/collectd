#!/bin/sh
echo 'checking if mdt_jobstats values are stored in opentsdb'
ssh monitor01 "influx -database barreleyedb -execute 'select * from \"mdt_jobstats_samples\" where time > now() - 2m'|grep samedir_rename 1>/dev/null"
if [ $? != 0 ] ; then
  echo 'mdt_jobstats values are not in opentsdb'
  exit 1
else
  echo 'mdt_jobstats values are in opentsdb'
fi
echo 'checking if ost_jobstats values are stored in opentsdb'
ssh monitor01 "influx -database barreleyedb -execute 'select * from \"ost_jobstats_samples\" where time > now() - 2m'|grep write_samples 1>/dev/null"
if [ $? != 0 ] ; then
  echo 'ost_jobstats values are not in opentsdb'
  exit 1
else
  echo 'ost_jobstats values are in opentsdb'
fi
