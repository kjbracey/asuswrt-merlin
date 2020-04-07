#!/bin/sh

# Add EUI-64 addresses to dnsmasq hosts
# Based on http://tomatousb.org/forum/t-306529/auto-creation-of-ipv6-hostnames

scrname="updatehosts"

trap "" SIGHUP
let i=timeout=$1
oldpid=$(cat "/var/run/dnsmasq.pid")
while [ $i -gt -1 ]; do
	newpid=$(cat "/var/run/dnsmasq.pid")
	if [ "$newpid" == "$oldpid" -a "$newpid" != "" ]; then
		i=$(($i-1))
		sleep 1
	else
        sleep $(($timeout-$i))
		rm -f /var/lock/autov6.lock
		exit 0
	fi
done

echo "updating dnsmasq hosts files" | logger -t "$scrname"
kill -SIGHUP $newpid
rm -f /var/lock/autov6.lock
exit 0
