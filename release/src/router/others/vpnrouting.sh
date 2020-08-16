#!/bin/sh

PARAM=$*
if [ "$PARAM" == "" ]
then
	# Add paramaters equivalent to those passed for up command
	PARAM="$dev $tun_mtu $link_mtu $ifconfig_local $ifconfig_remote"
fi

create_client_list(){
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
		TARGET_ROUTE=$(echo $ENTRY | cut -d ">" -f 4)
		if [ "$TARGET_ROUTE" = "WAN" ]
		then
			TARGET_LOOKUP="main"
			WAN_PRIO=$((WAN_PRIO+1))
			RULE_PRIO=$WAN_PRIO
			TARGET_NAME="WAN"
		else
			TARGET_LOOKUP=$VPN_TBL
			VPN_PRIO=$((VPN_PRIO+1))
			RULE_PRIO=$VPN_PRIO
			TARGET_NAME="VPN"
		fi
		VPN_IP=$(echo $ENTRY | cut -d ">" -f 2)
		if [ "$VPN_IP" != "0.0.0.0" ]
		then
			SRCC="from"
			SRCA="$VPN_IP"
		else
			SRCC=""
			SRCA=""
		fi
		DST_IP=$(echo $ENTRY | cut -d ">" -f 3)
		if [ "$DST_IP" != "0.0.0.0" ]
		then
			DSTC="to"
			DSTA="$DST_IP"
		else
			DSTC=""
			DSTA=""
		fi
		if [ "$SRCC" != "" -o "$DSTC" != "" ]
		then
			ip rule add $SRCC $SRCA $DSTC $DSTA table $TARGET_LOOKUP priority $RULE_PRIO
			logger -t "openvpn-routing" "Added $VPN_IP to $DST_IP through $TARGET_NAME to routing policy"
		fi
	done
	IFS=$OLDIFS
}

purge_client_list(){
	IP_LIST=$(ip rule show | cut -d ":" -f 1)
	for PRIO in $IP_LIST
	do
		if [ $PRIO -ge $START_PRIO -a $PRIO -le $END_PRIO ]
		then
			ip rule del prio $PRIO
			logger -t "openvpn-routing" "Removing rule $PRIO from routing policy"
		fi
	done
}

run_custom_script(){
	if [ -f /jffs/scripts/openvpn-event ]
	then
		logger -t "custom_script" "Running /jffs/scripts/openvpn-event (args: $PARAM)"
		sh /jffs/scripts/openvpn-event $PARAM
	fi
}

init_table(){
	logger -t "openvpn-routing" "Creating VPN routing table"
	ip route flush table $VPN_TBL

# Fill it with copy of existing main table
	ip route show table main | grep -v "tun1" | while read ROUTE # Prevent vpn routes already in table main from being copied
	do
		ip route add table $VPN_TBL $ROUTE
	done

# Delete pseudo default VPN routes that were pushed by server on table main
        NET_LIST=$(ip route show|awk '($1 == "0.0.0.0/1" || $1 == "128.0.0.0/1") && $2=="via" && $3==ENVIRON["route_vpn_gateway"] && $4=="dev" && $5==ENVIRON["dev"] {print $1}')
        for NET in $NET_LIST
        do
                ip route del $NET dev $dev table $VPN_TBL
                logger -t "openvpn-routing" "Removing route for $NET to $dev from VPN table"
        done
}

# Begin
if [ "$dev" == "tun11" ]
then
	VPN_IP_LIST=$(nvram get vpn_client1_clientlist)
	VPN_ACT_LIST=$(nvram get vpn_client1_activelist)
	VPN_REDIR=$(nvram get vpn_client1_rgw)
	VPN_FORCE=$(nvram get vpn_client1_enforce)
	VPN_ENABLED=$(nvram get vpn_client1_enabled)
	VPN_OFF=$(nvram get vpn_client1_off)
	VPN_INSTANCE=1

elif [ "$dev" == "tun12" ]
then
	VPN_IP_LIST=$(nvram get vpn_client2_clientlist)
	VPN_ACT_LIST=$(nvram get vpn_client2_activelist)
	VPN_REDIR=$(nvram get vpn_client2_rgw)
	VPN_FORCE=$(nvram get vpn_client2_enforce)
	VPN_ENABLED=$(nvram get vpn_client2_enabled)
	VPN_OFF=$(nvram get vpn_client2_off)
	VPN_INSTANCE=2
else
	run_custom_script
	exit 0
fi

VPN_TBL="ovpnc"$VPN_INSTANCE
START_PRIO=$((10000+(200*($VPN_INSTANCE-1))))
END_PRIO=$(($START_PRIO+199))
WAN_PRIO=$START_PRIO
VPN_PRIO=$(($START_PRIO+100))

export VPN_GW VPN_IP VPN_TBL VPN_FORCE VPN_ENABLED


# webui reports that vpn_force changed while vpn client was down
if [ $script_type = 'rmupdate' ]
then
	logger -t "openvpn-routing" "Refreshing policy rules for client $VPN_INSTANCE"
	purge_client_list

	if [ "$VPN_REDIR" == "2" ]
	then
		if [ "$VPN_FORCE" == "1" ] || [ "$VPN_FORCE" == "2" -a "$VPN_OFF" == "0" ]
		then
			init_table
			logger -t "openvpn-routing" "Tunnel down - VPN client access blocked"
			ip route del default table $VPN_TBL
			ip route add prohibit default table $VPN_TBL
			$(nvram set vpn_client${VPN_INSTANCE}_block=1)
			create_client_list
		else
			logger -t "openvpn-routing" "Allow WAN access to all VPN clients"
			ip route flush table $VPN_TBL
			$(nvram set vpn_client${VPN_INSTANCE}_block=0)
		fi
	fi
	ip route flush cache
	exit 0
fi

if [ $script_type == 'route-up' -a "$VPN_REDIR" != "2" ]
then
	logger -t "openvpn-routing" "Skipping, client $VPN_INSTANCE not in routing policy mode"
	run_custom_script
	exit 0
fi

logger -t "openvpn-routing" "Configuring policy rules for client $VPN_INSTANCE"

if [ $script_type == 'route-pre-down' ]
then
	purge_client_list

	if [ "$VPN_REDIR" == "2" ]
	then
		if [ "$VPN_FORCE" == "1" ] || [ "$VPN_FORCE" == "2" -a "$VPN_OFF" == "0" ]
		then
			logger -t "openvpn-routing" "Tunnel down - VPN client access blocked"
			ip route change prohibit default table $VPN_TBL
			$(nvram set vpn_client${VPN_INSTANCE}_block=1)
			create_client_list
		else
			ip route flush table $VPN_TBL
			$(nvram set vpn_client${VPN_INSTANCE}_block=0)
			logger -t "openvpn-routing" "Flushing client routing table"
		fi
	fi
fi	# End route down



if [ $script_type == 'route-up' ]
then
	init_table

# Delete existing VPN routes that were pushed by server on table main
	NET_LIST=$(ip route show|awk '$2=="via" && $3==ENVIRON["route_vpn_gateway"] && $4=="dev" && $5==ENVIRON["dev"] {print $1}')
	for NET in $NET_LIST
	do
		ip route del $NET dev $dev
		logger -t "openvpn-routing" "Removing route for $NET to $dev from table main"
	done

# Update policy rules
        purge_client_list
        create_client_list

# Setup table default route
	if [ "$VPN_IP_LIST" != "" ]
	then
		if [ "$VPN_FORCE" == "1" -o "$VPN_FORCE" == "2" ]
		then
			logger -t "openvpn-routing" "Tunnel re-established, restoring WAN access to clients"
		fi
		if [ "$route_net_gateway" != "" ]
		then
			ip route del default table $VPN_TBL
			ip route add default via $route_vpn_gateway table $VPN_TBL
			logger -t "openvpn-routing" "Setting default VPN route via $route_vpn_gateway"
		else
			logger -t "openvpn-routing" "WARNING: no VPN gateway provided, routing might not work properly!"
		fi

		if [ "$trusted_ip" != "" ]
		then
			logger -t "openvpn-routing" "VPN WAN address is $trusted_ip"
		fi
	fi

	if [ "$route_net_gateway" != "" ]
	then
		ip route del default
		ip route add default via $route_net_gateway
		$(nvram set vpn_client${VPN_INSTANCE}_block=0)
	fi
fi	# End route-up

ip route flush cache
logger -t "openvpn-routing" "Completed routing policy configuration for client $VPN_INSTANCE"
run_custom_script

exit 0
