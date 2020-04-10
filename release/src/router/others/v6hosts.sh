#!/bin/sh

# Add EUI-64 addresses to dnsmasq hosts
# Based on http://tomatousb.org/forum/t-306529/auto-creation-of-ipv6-hostnames

scrname="v6hosts"
debug=$(nvram get autov6_debug)
if [ -z "$debug" -o "$debug" == "0" ]; then
    LOGA="/dev/null"
    LOGB="/dev/null"
else
	LOGA="/tmp/autov6.log"
    LOGB="/tmp/autov6.dat"
fi

V6HOSTS="/etc/hosts.autov6"

if [ "$1" != "add" -a "$1" != "old" ]; then
    exit 0
fi

DHCPNAME=""
DHCPHOSTS=$(grep "dhcp-hostsfile" "/etc/dnsmasq.conf" | awk -F "=" '{ print $NF }')
if [ -n "$DHCPHOSTS" ]; then
	DHCPNAME=$(grep "$2" "$DHCPHOSTS" | awk -F "," '{ print $NF }')
fi

if [ -n "$DHCPNAME" ]; then
	HNAME="$DHCPNAME"
elif [ -n "$DNSMASQ_OLD_HOSTNAME" ]; then
    HNAME="$DNSMASQ_OLD_HOSTNAME"
elif [ -n "$4" ]; then
    HNAME="$4"
elif [ -n "$DNSMASQ_SUPPLIED_HOSTNAME" ]; then
    HNAME="$DNSMASQ_SUPPLIED_HOSTNAME"
else
    echo "[$(date)] no name $2" >> "$LOGA"
    exit 0
fi
echo "[$(date)] $1 $2 '$4' '$DNSMASQ_SUPPLIED_HOSTNAME' '$DNSMASQ_OLD_HOSTNAME' ($HNAME)" >> "$LOGA"

DEL_NEEDED=0
WL_FLAGS=0
grep -qi  " ${HNAME}" "$V6HOSTS"
if [ $? == 0 ]; then
    sleep 2
    v6addr=$(grep -i " ${HNAME}" "$V6HOSTS" | awk '{ print $1 }')
    # Skip address verification if wireless client in powersave mode
    wl assoclist | grep -q -i $2
    if [ $? == 0 ]; then
        WL_FLAGS=$(wl sta_info $2 | grep flags | awk -F'[ :]' '{print $3}')
        if [[ $(($WL_FLAGS & 0x100)) -gt 0 ]]; then
            echo "[$(date)] known name for 2.4GHz wireless client $HNAME in powersave mode" >> "$LOGA"
            exit 0
        fi
    fi
    wl -i eth2 assoclist | grep -q i $2
    if [ $? == 0 ]; then
        WL_FLAGS=$(wl -i eth2 sta_info $2 | grep flags | awk -F'[ :]' '{print $3}')
        if [[ $(($WL_FLAGS & 0x100)) -gt 0 ]]; then
            echo "[$(date)] known name for 5GHz wireless client $HNAME in powersave mode" >> "$LOGA"
            exit 0
        fi
    fi
    # Do ping test
    ping -6 -q -c 2 -W 5 $v6addr
    if [ $? == 0 ]; then
        echo "[$(date)] known name, valid addr $HNAME" >> "$LOGA"
        #echo "found valid hosts entry $v6addr $HNAME" | logger -t "$scrname"
        exit 0
    else
        echo "[$(date)] known name, invalid addr error $HNAME" >> "$LOGA"
        DEL_NEEDED=1
    fi
else
    sleep 2
fi

# extract first neighbor as global address (eiu64 or temp)
V6IFACE=$(ip -6 neigh show to fe80::/10 | grep "$2" | awk 'FNR <= 1' | awk '{ print $1 }' | awk -F '::' '{ print $2 }')
if [ "$V6IFACE" == "" ]; then
    echo "[$(date)] no neighbor $HNAME" >> "$LOGA"
    exit 0
fi
V6IFLEN=$(echo $V6IFACE | awk -F ':' '{print NF}')

V6PFX=$(nvram get ipv6_prefix | awk -F ':' 'BEGIN {OFS=":"} { i=1; while ($i != "") {i++}; NF=i-1; print $0}')
V6PFXLEN=$(echo $V6PFX | awk -F ':' '{print NF}')

if [[ $(($V6PFXLEN + $V6IFLEN)) -eq 4 ]]; then
    V6ADDR="::${V6IFACE}";
    DEL_NEEDED=1;
    UPDATE_HOSTS=0;
elif [[ $(($V6PFXLEN + $V6IFLEN)) -lt 8 ]]; then
    V6ADDR="${V6PFX}::${V6IFACE}";
    UPDATE_HOSTS=1;
elif [[ $(($V6PFXLEN + $V6IFLEN)) -eq 8 ]]; then
    V6ADDR="${V6PFX}:${V6IFACE}";
    UPDATE_HOSTS=1;
else
    echo "[$(date)] wrong length error $V6PFX $V6IFACE  $HNAME" >> "$LOGA"
    exit 1
fi

ping -6 -q -c 2 -W 5 $V6ADDR > /tmp/ping

ip -6 neigh | grep -qi "$V6ADDR .* lladdr $2"
if [ $? == 0 ]; then
    if [ $DEL_NEEDED -eq 1 ]; then
        # workaround lack of case insensitivity options
        echo "[$(date)] removing hosts entry $v6addr $HNAME" >> "$LOGA"
        sed -i "s/.*\b${HNAME}\b.*//I; /^$/ d" "$V6HOSTS"
    fi
    if [ $UPDATE_HOSTS -eq 1 ]; then
        echo "[$(date)] $V6ADDR $HNAME" >> "$LOGB"
		grep -qi  "${V6ADDR} ${HNAME}" "$V6HOSTS"
		if [ $? == 0 ]; then
			echo "[$(date)] skipping duplicate hosts update $V6ADDR $HNAME" >> "$LOGA"
			echo "skipping duplicate hosts entry $V6ADDR $HNAME" | logger -t "$scrname"
			exit 0
		else
			echo "$V6ADDR $HNAME" >> "$V6HOSTS"
			echo "[$(date)] `if [ $DEL_NEEDED -eq 1 ]; then echo 'updating'; else echo 'adding'; fi` hosts entry $V6ADDR $HNAME" >> "$LOGA"
			echo "`if [ $DEL_NEEDED -eq 1 ]; then echo 'updating'; else echo 'adding'; fi` hosts entry $V6ADDR $HNAME" | logger -t "$scrname"
			if [ ! -e /var/lock/autov6.lock ]; then
				touch /var/lock/autov6.lock
				/usr/sbin/updatehosts.sh 60 </dev/null &>/dev/null &
			fi
		fi
    fi
else
    echo "[$(date)] no ping response from $V6ADDR $HNAME" >> "$LOGA"
fi

exit
