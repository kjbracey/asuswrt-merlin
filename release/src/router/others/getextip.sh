#!/bin/sh
# use STUN to find the external IP.

DEV=$1
max_instance=2
servers="stun.l.google.com:19302 stun.stunprotocol.org default"
prefixes="wan0_ wan1_"
ifname=""

if [ "$DEV" == "-h" ]
then
	echo "Get External IP Address via STUN"
	echo "Usage: getextip.sh [interface]"
	echo "Usage: gettunnelip.sh [vpn_instance]"
	exit 0
fi

if [ "$DEV" == "" ] || [ ${#DEV} -gt 1 ]
then
    if [ "${DEV:0:3}" == "tun" ]
    then
        ifname="${DEV}"
        break
    else
        for prefix in $prefixes; do
		    primary=$(nvram get ${prefix}primary)
		    if [ "$primary" = "1" ]
            then
                # get_wan_ifname()
	            proto=$(nvram get ${prefix}proto)
	            if [ "$proto" = "pppoe" -o "$proto" = "pptp" -o "$proto" = "l2tp" ] ; then
		            ifname=$(nvram get ${prefix}pppoe_ifname)
	            else
		            ifname=$(nvram get ${prefix}ifname)
	            fi
                break
            fi
        done
    fi
else
    ifname="tun1${DEV}"
fi

INSTANCE="${ifname:$((${#ifname}-1)):1}"
if [ "${ifname:0:3}" == "tun" -a $INSTANCE -gt $max_instance ]
then
   	echo "Invalid client instance!"
   	exit 1
fi

for server in $servers; do
	[ "$server" = "default" ] && server=
	result=$(/usr/sbin/ministun -t 1000 -c 1 -i $ifname $server 2>/dev/null)
	[ $? -eq 0 ] && break
	result=""
done

[ "${ifname:0:3}" == "tun" ] && nvram set "vpn_client${INSTANCE}_rip"=$result || nvram set "ext_ipaddr"=$result
echo "$result"
