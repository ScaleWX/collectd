#!/bin/sh
echo 'checking if collectd.service is active'
/bin/systemctl is-active collectd 1>/dev/null
if [ $? = 0 ] ; then
  echo 'stopping collectd.service'
  sudo /bin/systemctl stop collectd
else
  echo 'collectd.service is not active'
fi

echo 'checking if collectd-ssh is installed'
rpm -qa|grep "collectd-ssh" 1>/dev/null
if [ $? = 0 ] ; then
  echo 'uninstalling collectd-ssh'
  sudo rpm -e collectd-ssh
else
  echo 'collectd-ssh is not installed'
fi

echo 'checking if collectd-filedata is installed'
rpm -qa|grep "collectd-filedata" 1>/dev/null
if [ $? = 0 ] ; then
  echo 'uninstalling collectd-filedata'
  sudo rpm -e collectd-filedata
else
  echo 'collectd-filedata is not installed'
fi

echo 'checking if collectd-rrdtool is installed'
rpm -qa|grep "collectd-rrdtool" 1>/dev/null
if [ $? = 0 ] ; then
  echo 'uninstalling collectd-rrdtool'
  sudo rpm -e collectd-rrdtool
else
  echo 'collectd-rrdtool is not installed'
fi

echo 'checking if xml_definition is installed'
rpm -qa|grep "xml_definition" 1>/dev/null
if [ $? = 0 ] ; then
  echo 'uninstalling xml_definition'
  sudo rpm -e xml_definition
else
  echo 'xml_definition is not installed'
fi

echo 'checking if collectd is installed'
rpm -qa|grep "collectd-[0-9]" 1>/dev/null
if [ $? = 0 ] ; then
  echo 'uninstalling collectd'
  sudo rpm -e collectd
else
  echo 'collectd is not installed'
fi
