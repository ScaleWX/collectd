#!/bin/sh
echo 'checking if barreleyedb database exists on monitor server'
ssh monitor01 "influx -execute 'show databases'"|grep barreleyedb 1>/dev/null
if [ $? = 0 ] ; then
  echo 'dropping database'
  ssh monitor01 "influx -execute 'drop database barreleyedb';sudo /bin/systemctl restart influxd"
fi
