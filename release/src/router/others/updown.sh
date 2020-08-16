#!/bin/sh
filedir=/etc/openvpn/dns
filebase=$(echo $filedir/$dev | sed 's/\(tun\|tap\)1/client/;s/\(tun\|tap\)2/server/')
conffile=$filebase\.conf
resolvfile=$filebase\.resolv
dnsscript=$(echo /etc/openvpn/fw/$(echo $dev)-dns\.sh | sed 's/\(tun\|tap\)1/client/;s/\(tun\|tap\)2/server/')
fileexists=
instance=$(echo $dev | sed "s/tun1//;s/tun2//")
serverips=
searchdomains=
lan_if=$(nvram get lan_ifname)
lan_ip=$(nvram get lan_ipaddr)
vpn_dns_mode=$(nvram get vpn_dns_mode); if [ "$vpn_dns_mode" == "" ]; then vpn_dns_mode=0; fi
vpn_block_ipv6=$(nvram get vpn_block_ipv6); if [ "$vpn_block_ipv6" == "" ]; then vpn_block_ipv6=0; fi
ipv6_if=$(nvram get ipv6_ifname)
if [ "$(nvram get ipv6_service)" == "disabled" ]; then ipv6_enabled=0; else ipv6_enabled=1; fi
machine_arm=$(uname -r); if [ "$machine_arm" != "${machine_arm%"arm"*}" ]; then machine_arm=1; else machine_arm=0; fi
if [ $machine_arm == 1 -a $(nvram get allow_routelocal) == 1 ]; then allow_routelocal=1; else allow_routelocal=0; fi

proxy_active=0
ipv6_active=0
# DNSCRYPT
if [ $(nvram get dnscrypt_proxy) == 1 ]
then
	proxy_active=1
	ext_proxy="DNSCRYPT"
	ext_proxy_port=$(nvram get dnscrypt1_port)
	if [ $(nvram get dnscrypt1_ipv6) == 1 -a $(nvram get dnscrypt2_ipv6) == 1 ]
	then
		ipv6_active=1
	fi
fi
# STUBBY
if [ $(nvram get stubby_proxy) == 1 ]
then
	proxy_active=1
	ext_proxy="STUBBY"
	ext_proxy_port=$(nvram get stubby_port)
	if [ $(nvram get stubby_ipv6) -gt 0 -a $(nvram get stubby_noipv6) -eq 0 ]
	then
		ipv6_active=1
	fi
fi

# Set up VPN server
if [ $(nvram get dhcpd_dns_router) == 1 ]
then
	VPNserver=$(nvram get lan_ipaddr)
else
	VPNserver=
fi

# Set up ISP server
if [ $(nvram get wan0_dnsenable_x) == 1 ]
then
	ISPserver=$(nvram get wan0_dns | awk -F" " '{ print $1; }')
else
	ISPserver=$(nvram get wan0_dns1_x)
fi

# Check if DNS server is a private address
if [[ -n "$(echo "$ISPserver" | grep -E '^(10\.|100\.(6[4-9]|7[0-9]|8[0-9]|9[0-9]|1[0-2][0-9])\.|172\.(1[6789]|2[0-9]|3[01])\.|127\.0\.0\.|192\.168|198\.1[89])')" ]]
then
	if [ $allow_routelocal == 1 ]
	then
		echo 1 > /proc/sys/net/ipv4/conf/br0/route_localnet
	fi
fi

# Override if DNSCRYPT/STUBBY is active and exclusive
allow_ext_resolver=0
if [ $proxy_active == 1 -a $ipv6_active == 0 -a $(nvram get vpn_client$(echo $instance)_adns) == 3 -a $allow_routelocal == 1 ]
then
	echo 1 > /proc/sys/net/ipv4/conf/br0/route_localnet
	ISPserver=127.0.0.1:$ext_proxy_port
	allow_ext_resolver=1
fi

create_client_list(){
	server=$1
	if [ "$VPNserver" == "" ]
	then
		VPNserver=$server
	fi
	VPN_IP_LIST=$(nvram get vpn_client$(echo $instance)_clientlist)
	VPN_ACT_LIST=$(nvram get vpn_client$(echo $instance)_activelist)

	OLDIFS=$IFS
	IFS="<"
	INDEX=0

	for ENTRY in $VPN_IP_LIST
	do
		let INDEX=$INDEX+1
                RULE_ACTIVE=$(echo $VPN_ACT_LIST | tr " " ">" | cut -d ">" -f $INDEX)
                if [ "$ENTRY" == "" -o "$RULE_ACTIVE" == "0" ]
		then
			continue
		fi

		VPN_IP=$(echo $ENTRY | cut -d ">" -f 2)
		if [ "$VPN_IP" != "0.0.0.0" ]
		then
			TARGET_ROUTE=$(echo $ENTRY | cut -d ">" -f 4)
			if [ "$TARGET_ROUTE" == "VPN" ]
			then
				echo iptables -t nat -A DNSVPN$instance -s $VPN_IP -j DNAT --to-destination $VPNserver >> $dnsscript
				logger -t "openvpn-updown" "Setting $VPN_IP to use `if [ $(nvram get vpn_client$(echo $instance)_adns) == 4 ]; then echo $ext_proxy; else echo 'VPN'; fi` DNS servers `if [ $VPNserver != $server ]; then echo '(via dnsmasq)'; fi`"
			else
				if [ $allow_ext_resolver == 1 ]
				then
					echo iptables -t nat -I DNSVPN$instance -p tcp -s $VPN_IP -j DNAT --to-destination $ISPserver >> $dnsscript
					echo iptables -t nat -I DNSVPN$instance -p udp -s $VPN_IP -j DNAT --to-destination $ISPserver >> $dnsscript
				else
					echo iptables -t nat -I DNSVPN$instance -s $VPN_IP -j DNAT --to-destination $ISPserver >> $dnsscript
				fi
				logger -t "openvpn-updown" "Setting $VPN_IP to use default DNS server `if [ $allow_ext_resolver == 1 ]; then echo '('$ext_proxy')'; else echo '(Primary WAN)'; fi`"
			fi
		fi
	done

	if [ $allow_ext_resolver == 1 ]
	then
		echo iptables -t nat -A DNSVPN$instance -p tcp -j DNAT --to-destination $ISPserver >> $dnsscript
		echo iptables -t nat -A DNSVPN$instance -p udp -j DNAT --to-destination $ISPserver >> $dnsscript
	else
		echo iptables -t nat -A DNSVPN$instance -j DNAT --to-destination $ISPserver >> $dnsscript
	fi

	IFS=$OLDIFS
}


if [ ! -d $filedir ]; then mkdir $filedir; fi
if [ -f $conffile ]; then rm $conffile; fileexists=1; fi
if [ -f $resolvfile ]; then rm $resolvfile; fileexists=1; fi

if [ $script_type == 'up' ]
then
	if [ $instance != 0 -a $(nvram get vpn_client$(echo $instance)_rgw) == 2 -a $(nvram get vpn_client$(echo $instance)_adns) -ge 3 -a $vpn_dns_mode == 1 ]
	then
		setdns=0
		if [ -f $dnsscript ]; then rm $resolvfile; fi
		echo iptables -t nat -N DNSVPN$instance >> $dnsscript
		logger -t "openvpn-updown" "Using `if [ $allow_ext_resolver == 1 ]; then echo '('$ext_proxy')'; else echo '(Primary WAN)'; fi` DNS for non-VPN clients"
	else
		setdns=-1
		logger -t "openvpn-updown" "Using `if [ $(nvram get vpn_client$(echo $instance)_adns) == 4 ]; then echo $ext_proxy; elif [ $(nvram get vpn_client$(echo $instance)_adns) == 3 ]; then echo 'VPN'; else echo 'Default'; fi` DNS for non-VPN clients"
	fi

	# Extract IPs and search domains; write WINS
	for optionname in `set | grep "^foreign_option_" | sed "s/^\(.*\)=.*$/\1/g"`
	do
		option=`eval "echo \\$$optionname"`
		if echo $option | grep "dhcp-option WINS "; then echo $option | sed "s/ WINS /=44,/" >> $conffile; fi
		if echo $option | grep "dhcp-option DNS"; then serverips="$serverips $(echo $option | sed "s/dhcp-option DNS //")"; fi
		if echo $option | grep "dhcp-option DOMAIN"; then searchdomains="$searchdomains $(echo $option | sed "s/dhcp-option DOMAIN //")"; fi
	done

	# Write resolv/conf file
	for server in $serverips
	do
		echo "nameserver $server" >> $resolvfile
		if [ $setdns == 0 ]
		then
			create_client_list $server
			setdns=1
		fi
		for domain in $searchdomains
		do
			echo "server=/$domain/$server" >> $conffile
		done
	done

	if [ $setdns == 1 ]
	then
		echo iptables -t nat -A PREROUTING -i $lan_if -p udp -m udp --dport 53 -j DNSVPN$instance >> $dnsscript
		echo iptables -t nat -A PREROUTING -i $lan_if -p tcp -m tcp --dport 53 -j DNSVPN$instance >> $dnsscript
	fi

	if [ $ipv6_enabled == 1 -a $(nvram get vpn_client$(echo $instance)_rgw) -gt 0 ]
	then
		# user DROP instead of REJECT for 'Happy Eyeballs' RFC6555
		if [ $vpn_block_ipv6 = 1 ] # hard block all ipv6
		then
			$(ip6tables-save -t filter | grep -q -i "\-i $lan_if \-o $ipv6_if \-j REJECT")
			if [ $? -eq 1 ]
			then
				echo ip6tables -I FORWARD -i $lan_if -o $ipv6_if -j REJECT >> $dnsscript
			fi
		fi

		# default block some ipv6 dns queries
#		$(ip6tables-save -t filter | grep -q -i "dport 53 \-j REJECT")
#		if [ $? -eq 1 ]
#		then
#			if [ $(nvram get ipv6_dns_router) == 1 ]
#			then #dnsmasq as ipv6 dns server
#				echo ip6tables -I INPUT -i $lan_if -p tcp -m tcp --dport 53 -j REJECT >> $dnsscript
#				echo ip6tables -I INPUT -i $lan_if -p udp -m udp --dport 53 -j REJECT >> $dnsscript
#				logger -t "openvpn-updown" "Block IPv6 DNS queries to router (INPUT mode)"
#			else #router not ipv6 dns server
#				echo ip6tables -I FORWARD -i $lan_if -p tcp -m tcp --dport 53 -j REJECT >> $dnsscript
#				echo ip6tables -I FORWARD -i $lan_if -p udp -m udp --dport 53 -j REJECT >> $dnsscript
#				logger -t "openvpn-updown" "Block IPv6 DNS queries to internet (FORWARD mode)"
#			fi
#		fi
	fi
fi


if [ $script_type == 'down' -a $instance != 0 ]
then
	if [ -f $dnsscript ]
	then
		/usr/sbin/iptables -t nat -D PREROUTING -i $lan_if -p udp -m udp --dport 53 -j DNSVPN$instance
		/usr/sbin/iptables -t nat -D PREROUTING -i $lan_if -p tcp -m tcp --dport 53 -j DNSVPN$instance
		/usr/sbin/iptables -t nat -F DNSVPN$instance
		/usr/sbin/iptables -t nat -X DNSVPN$instance
		logger -t "openvpn-updown" "Removed ISP DNS rules for non-VPN clients"
	fi

	vpn_off=$(nvram get vpn_client${instance}_off)
	vpn_force=$(nvram get vpn_client${instance}_enforce)
	if [ $ipv6_enabled == 1 ]
	then
		if [ $vpn_force == 0 ] || [ $vpn_force == 2 -a $vpn_off == 1 ]
		then
#			/usr/sbin/ip6tables -D INPUT -i $lan_if -p tcp -m tcp --dport 53 -j REJECT
#			/usr/sbin/ip6tables -D INPUT -i $lan_if -p udp -m udp --dport 53 -j REJECT
#			/usr/sbin/ip6tables -D FORWARD -i $lan_if -p tcp -m tcp --dport 53 -j REJECT
#			/usr/sbin/ip6tables -D FORWARD -i $lan_if -p udp -m udp --dport 53 -j REJECT
			/usr/sbin/ip6tables -D FORWARD -i $lan_if -o $ipv6_if -j REJECT
			if [ $vpn_block_ipv6 != 1 ]
			then
#				logger -t "openvpn-updown" "Removed IPv6 DNS blocking"
#			else
				logger -t "openvpn-updown" "Removed IPv6 blocking"
			fi
		fi
	fi
fi

if [ -f $conffile -o -f $resolvfile -o -n "$fileexists" ]
then
	if [ $script_type == 'up' ] ; then
		if [ -f $dnsscript ]
		then
			sh $dnsscript
		fi
		service updateresolv
	elif [ $script_type == 'down' ]; then
		rm $dnsscript
		service updateresolv # also restarts or reloads dnsmasq as necessary
#		service restart_dnsmasq
	fi
fi

rmdir $filedir
rmdir /etc/openvpn

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
