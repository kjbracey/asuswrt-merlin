#!/bin/sh

scr_name="$(basename $0)[$$]"

# ensure every openvpn client's routing policy table contains the server's ip network on the tunnel
# version: 1.1.0, 26-mar-2021, by eibgrad
if [ $script_type = 'up' ]
then
	for i in 1 2 3 4 5; do
    	[ "$(ip route show table ovpnc${i})" ] || continue

    	ip route | grep $dev | \
    	    while read route; do
    	        ip route add $route table ovpnc${i} 2>/dev/null
                logger -t "openvpn-updown" "Adding VPN server route `echo ${route} | cut -d' ' -f1-3` to VPN client${i}"
    	    done
	done
fi

if [ -f /jffs/scripts/openvpn-event ]
then
    if [ "$(nvram get jffs2_scripts)" = "0" ]
		then
            logger -t "custom_script" "Found openvpn-event, but custom script execution is disabled!"
		else
            logger -t "custom_script" "Running /jffs/scripts/openvpn-event (args: $*)"
            sh /jffs/scripts/openvpn-event $*
    fi
fi

exit 0


