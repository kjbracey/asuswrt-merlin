/*

	Tomato Firmware
	Copyright (C) 2006-2007 Jonathan Zarate
	$Id: qos.c 241383 2011-02-18 03:30:06Z stakita $

	ASUS features:
		- traditional qos
		- bandwidth limiter

	Copyright (C) ASUSTek. Computer Inc.

*/
#include "rc.h"
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <shared.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static const char *qosfn = "/tmp/qos";
static const char *mangle_fn = "/tmp/mangle_rules";
#ifdef RTCONFIG_IPV6
static const char *mangle_fn_ipv6 = "/tmp/mangle_rules_ipv6";
#endif

int etable_flag = 0;
int manual_return = 0;

#define MAX_RETRY 10
#define BUF_SIZE 256

// FindMask : 
// 1. sourceStr 	: replace "*'" to "0"
// 2. matchStr 		: here is "*"
// 3. replaceStr 	: here is "0" 
// 4. Mask 		: find rule's submask 
static int FindMask(char *sourceStr, char *matchStr, char *replaceStr, char *Mask){
	char newStr[40];
	int strLen;
	int count = 0;

	char *FindPos = strstr(sourceStr, matchStr);
	if((!FindPos) || (!matchStr)) return -1;

	while(FindPos != NULL){
		count++;
		//fprintf(stderr,"[FindMask] FindPos=%s, strLen=%d, count=%d, sourceStr=%s\n", FindPos, strLen, count, sourceStr); // tmp test
		memset(newStr, 0, sizeof(newStr));
		strLen = FindPos - sourceStr;
		strncpy(newStr, sourceStr, strLen);
		strcat(newStr, replaceStr);
		strcat(newStr, FindPos + strlen(matchStr));
		strcpy(sourceStr, newStr);

		FindPos = strstr(sourceStr, matchStr);
	}

	switch(count){
		case 1:
			strcpy(Mask, "255.255.255.0");
			break;
		case 2:
			strcpy(Mask, "255.255.0.0");
			break;
		case 3:
			strcpy(Mask, "255.0.0.0");
			break;
		case 4:
			strcpy(Mask, "0.0.0.0");
			break;
		default:
			strcpy(Mask, "255.255.255.255");
			break;
	}

	//fprintf(stderr,"[FindMask] count=%d, Mask=%s\n", count, Mask); // tmp test
	return 0;
}

static unsigned calc(unsigned bw, unsigned pct)
{
	unsigned n = ((unsigned long)bw * pct) / 100;
	return (n < 2) ? 2 : n;
}


#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
void del_EbtablesRules(void)
{
	/* Flush all rules in nat table of ebtable*/
	eval("ebtables", "-t", "nat", "-F");
	etable_flag = 0;
}

void add_EbtablesRules(void)
{
	if(etable_flag == 1) return;
	char *nv, *p, *g;
	nv = g = strdup(nvram_safe_get("wl_ifnames"));
	if(nv){
		while ((p = strsep(&g, " ")) != NULL){
			//fprintf(stderr, "%s: g=%s, p=%s\n", __FUNCTION__, g, p); //tmp test
			eval("ebtables", "-t", "nat", "-A", "PREROUTING", "-i", p, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
			eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", p, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
		}
		free(nv);
	}

	// for MultiSSID
	int UnitNum = 2; 	// wl0.x, wl1.x
	int GuestNum = 3;	// wlx.0, wlx.1, wlx.2
	char mssid_if[32];
	char mssid_enable[32];
	int i, j;
	for( i = 0; i < UnitNum; i++){
		for( j = 1; j <= GuestNum; j++ ){
			snprintf(mssid_if, sizeof(mssid_if), "wl%d.%d", i, j);
			snprintf(mssid_enable, sizeof(mssid_enable), "%s_bss_enabled", mssid_if);
			//fprintf(stderr, "%s: mssid_enable=%s\n", __FUNCTION__, mssid_enable); //tmp test
			if(!strcmp(nvram_safe_get(mssid_enable), "1")){
				eval("ebtables", "-t", "nat", "-A", "PREROUTING", "-i", mssid_if, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
				eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", mssid_if, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
			}
		}
	}

	etable_flag = 1;
}
#endif

void del_iQosRules(void)
{
	FILE *fn;
	char *argv[7];
	char buffer[BUF_SIZE];
	char rule[BUF_SIZE];

#ifdef CLS_ACT
	eval("ip", "link", "set", "imq0", "down");
#endif

#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	del_EbtablesRules(); // flush ebtables nat table
#endif

	/* Remove or Flush all rules in mangle table */
        argv[0] = "sed";
        argv[1] = "-i";
        argv[2] = "s/-A/-D/g;s/-I/-D/g;/-D/!d";
        argv[3] = NULL;
        argv[4] = NULL;

	sprintf(&buffer[0], "%s", mangle_fn);
	if(check_if_file_exist(&buffer[0])) {
		argv[3] = &buffer[0];
		if (!_eval(argv, NULL, 0, NULL)) {
			if ((fn = fopen(mangle_fn, "r")) != NULL) {
				while (fgets(rule, sizeof(rule), fn)) /* read a line */
				{
					sprintf(&buffer[0], "iptables -t mangle %s", rule);
					system(&buffer[0]);
				}
				fclose(fn);
				unlink(mangle_fn);
			}
		}
		else {
			logmessage("qos-rules","error removing ipv4 rules, flushing table");
			eval("iptables", "-t", "mangle", "-F");
		}
	}
#ifdef RTCONFIG_IPV6
	sprintf(&buffer[0], "%s", mangle_fn_ipv6);
	if(check_if_file_exist(&buffer[0])) {
		argv[3] = &buffer[0];
		if (!_eval(argv, NULL, 0, NULL)) {
			if ((fn = fopen(mangle_fn_ipv6, "r")) != NULL) {
				while (fgets(rule, sizeof(rule), fn)) /* read a line */
				{
					sprintf(&buffer[0], "ip6tables -t mangle %s", rule);
					system(&buffer[0]);
				}
				fclose(fn);
				unlink(mangle_fn_ipv6);
			}
		}
		else {
			logmessage("qos-rules","error removing ipv6 rules, flushing table");
	                eval("ip6tables", "-t", "mangle", "-F");
		}
	}
#endif
}

int add_qos_rules(char *pcWANIF)
{
	FILE *fn;
#ifdef RTCONFIG_IPV6
	FILE *fn_ipv6 = NULL;
#endif
	char *buf;
	char *g;
	char *p;
	char *desc, *addr, *port, *prio, *transferred, *proto;
	int class_num;
	int down_class_num=6; 	// for download class_num = 0x6 / 0x106
	int i, inuse;
	char q_inuse[32]; 	// for inuse 
	char dport[192], saddr_1[256], saddr_2[256], maddr[256], proto_1[8], proto_2[8],conn[256], end[256], end2[256];
	int method;
	int gum;
	int sticky_enable;
	const char *chain;
	int v4v6_ok;
	int v6rule_ok;

	del_iQosRules(); // clear all Qos rules in mangle table

	if( pcWANIF == NULL || nvram_get_int("qos_enable") != 1 || nvram_get_int("qos_type") != 0) return -1;
	if ((fn = fopen(mangle_fn, "w")) == NULL) return -2;
#ifdef RTCONFIG_IPV6
	if (ipv6_enabled() && (fn_ipv6 = fopen(mangle_fn_ipv6, "w")) == NULL){
		fclose(fn);
		return -3;
	}
#endif

	inuse = sticky_enable = 0;

        if(get_model()==MODEL_RTAC56U || get_model()==MODEL_RTAC56S || get_model()==MODEL_RTAC68U || get_model()==MODEL_DSLAC68U || get_model()==MODEL_RTAC87U)
                manual_return = 1;

	if (nvram_match("qos_sticky", "0"))
		sticky_enable = 1;

//	del_iQosRules(); // flush all rules in mangle table

#ifdef CLS_ACT
	eval("ip", "link", "set", "imq0", "up");
#endif
	fprintf(stderr, "[qos] iptables START\n");

	fprintf(fn, 
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		":QOSO - [0:0]\n"
		"%s\n" //NAT Loopback
		"-A QOSO -m state --state RELATED,ESTABLISHED -j CONNMARK --restore-mark --mask 0x1ff\n"
		"-A QOSO -m mark ! --mark 0/0xff00 -j RETURN\n"
		, (nvram_get_int("fw_nat_loopback") ? "" : "-A QOSO -m mark --mark 0x8000/0x8000 -j RETURN")
		);
#ifdef RTCONFIG_IPV6
	if (ipv6_enabled())
	fprintf(fn_ipv6,
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		":QOSO - [0:0]\n"
		"-A QOSO -m state --state RELATED,ESTABLISHED -j CONNMARK --restore-mark --mask 0x1ff\n"
		"-A QOSO -m mark ! --mark 0/0xff00 -j RETURN\n"
		);
#endif

	g = buf = strdup(nvram_safe_get("qos_rulelist"));
	while (g) {
		
		/* ASUSWRT 
  		qos_rulelist : 
			desc>addr>port>proto>transferred>prio
			
			addr  : (source) IP or MAC or IP-range
			port  : dest port
			proto : tcp, udp, tcp/udp, any , (icmp, igmp)
			transferred : min:max
		 	prio  : 0-4, 0 is the highest		
  		*/
	
		if ((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;
		class_num = atoi(prio);
		if ((class_num < 0) || (class_num > 4)) continue;

		i = 1 << class_num;
		++class_num;
		
		//if (method == 1) class_num |= 0x200;
		if ((inuse & i) == 0) {
			inuse |= i;
			fprintf(stderr, "[qos] iptable creates, inuse=%d\n", inuse);
		}

		v4v6_ok = IPT_V4;
		v6rule_ok = 0;
#ifdef RTCONFIG_IPV6
		if (ipv6_enabled()) {
			v4v6_ok |= IPT_V6;
			v6rule_ok = 1;
		}
#endif

		/* Beginning of the Rule */
		/* 
 			if transferred != NULL, class_num must bt 0x1~0x6, not 0x101~0x106
			0x1~0x6		: keep tracing this connection.
			0x101~0x106 	: connection will be considered as marked connection, won't detect again.
		*/
//#if 0
		if(strcmp(transferred, "") != 0 ) 
			method = 1;
		else
			method = nvram_get_int("qos_method");	// strict rule ordering
		gum = (method == 0) ? 0x100 : 0;
//#else
//		method = 1;
//		gum = 0;
//#endif
	        class_num |= gum;
		down_class_num |= gum; // for download

		chain = "QOSO";                     // chain name
                sprintf(end , " -j CONNMARK --set-return 0x%x/0x1ff\n", class_num);	// CONNMARK string
		sprintf(end2, " -j RETURN\n");

		/*************************************************/
		/*                        addr                   */
		/*           src mac or src ip or IP range       */
		/*************************************************/
		char tmp[20], addr_t[40];
		char *tmp_addr, *q_ip, *q_mac;

		memset(saddr_1, 0, sizeof(saddr_1));
		memset(saddr_2, 0, sizeof(saddr_2));
		memset(maddr, 0, sizeof(maddr));

		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%s", addr);
		tmp_addr = tmp;
		q_ip  = strsep(&tmp_addr, ":");
		q_mac = tmp_addr;

		memset(addr_t, 0, sizeof(addr_t));
		sprintf(addr_t, "%s", addr);

		// step1: check contain '-' or not, if yes, IP-range, ex. 192.168.1.10-192.168.1.100
		// step2: check addr is NULL
		// step3: check IP or MAC
		// step4: check IP contain '*' or not, if yes, IP-range
		// step5: check DUT's LAN IP shouldn't inside IP-range
		
		// step1: check contain '-' or not, if yes, IP-range
		if(strchr(addr_t, '-') == NULL){
			// step2: check addr is NULL
			if(!strcmp(addr_t, "")){
				sprintf(saddr_1, "%s", addr_t);	// NULL
			}
			else{ // step2
				// step3: check IP or MAC
				if (q_mac == NULL){
					// step4: check IP contain '*' or not, if yes, IP-range
					if(strchr(q_ip, '*') != NULL){
						char *rule;
						char Mask[40];
						struct in_addr range_A, range_B, range_C;

						memset(Mask, 0, sizeof(Mask));
						rule =  strdup(addr_t);
						FindMask(rule, "*", "0", Mask); 				// find submask and replace "*" to "0"
						memset(addr_t, 0, sizeof(addr_t));
						sprintf(addr_t, "%s", rule);					// copy rule to addr_t for v4v6_ok

						unsigned int ip = inet_addr(rule); 				// covert rule's IP into binary form
						unsigned int nm = inet_addr(Mask);				// covert submask into binary form
						unsigned int gw = inet_addr(nvram_safe_get("lan_ipaddr")); 	// covert DUT's LAN IP into binary form
						unsigned int gw_t = htonl(gw);

						range_A.s_addr = ntohl(gw_t - 1);
						range_B.s_addr = ntohl(gw_t + 1);
						range_C.s_addr = ip | ~nm;

			//fprintf(stderr, "[addr] addr_t=%s, rule/Mask=%s/%s, ip/nm/gw=%x/%x/%x\n", addr_t, rule, Mask, ip, nm, gw); // tmp test
						
						// step5: check DUT's LAN IP shouldn't inside IP-range
						// DUT's LAN IP inside IP-range
						if( (ip & nm) == (gw & nm)){
			//fprintf(stderr, "[addr] %x/%x/%x/%x/%s matched\n", ip_t, nm_t, gw_t, range_B.s_addr, inet_ntoa(range_B)); // tmp test
							char range_B_addr[40];
							sprintf(range_B_addr, "%s", inet_ntoa(range_B));

							sprintf(saddr_1, "-m iprange --src-range %s-%s", rule, inet_ntoa(range_A)); 		// IP-range
							sprintf(saddr_2, "-m iprange --src-range %s-%s", range_B_addr, inet_ntoa(range_C)); 	// IP-range
							v6rule_ok = 0;
						}
						else{
							sprintf(saddr_1, "-m iprange --src-range %s-%s", rule, inet_ntoa(range_C)); 		// IP-range
							v6rule_ok = 0;
						}
						free(rule);
					}
					else{ // step4
						sprintf(saddr_1, "-s %s", addr_t);	// IP
					}

					v4v6_ok &= ipt_addr_compact(addr_t, v4v6_ok, (v4v6_ok==IPT_V4));
					if (!v4v6_ok) continue;
				}
				else{ // step3
					sprintf(maddr, "-m mac --mac-source %s", addr_t);	// MAC
				}
			}
		}
		else{ // step1
			sprintf(saddr_1, "-m iprange --src-range %s", addr_t);	// IP-range
			v6rule_ok = 0;
		}
		//fprintf(stderr, "[qos] tmp=%s, ip=%s, mac=%s, addr=%s, addr_t=%s, saddr_1=%s, saddr_2=%s\n", tmp, q_ip, q_mac, addr, addr_t, saddr_1, saddr_2); // tmp test
		
		/*************************************************/
		/*                      port                     */
		/*            single port or multi-ports         */
		/*************************************************/
		char *tmp_port, *q_port, *q_leave;

		sprintf(tmp, "%s", port);
		tmp_port = tmp;
		q_port = strsep(&tmp_port, ",");
		q_leave = tmp_port;

		if(strcmp(port, "") == 0 ){
			sprintf(dport, "%s", "");
		}
		else{ // always use multiport to specify port as source or destination
//			if(q_leave != NULL)
				sprintf(dport, "-m multiport --port %s", port); // multi port
//			else
//				sprintf(dport, "--dport %s", port); // single port
		}
		//fprintf(stderr, "[qos] tmp=%s, q_port=%s, q_leave=%s, port=%s\n", tmp, q_port, q_leave, port ); // tmp test

		/*************************************************/
		/*                   transferred                 */
		/*   --connbytes min:max                         */
 		/*   --connbytes-dir (original/reply/both)       */
 		/*   --connbytes-mode (packets/bytes/avgpkt)     */
		/*************************************************/
		char *tmp_trans, *q_min, *q_max;
		long min, max ;	
	
		sprintf(tmp, "%s", transferred);
		tmp_trans = tmp;
		q_min = strsep(&tmp_trans, "~");
		q_max = tmp_trans;

		if (strcmp(transferred,"") == 0){
			sprintf(conn, "%s", ""); 
		}
		else{
			sprintf(tmp, "%s", q_min);
			min = atol(tmp);

			if(strcmp(q_max,"") == 0) // q_max == NULL
				sprintf(conn, "-m connbytes --connbytes %ld:%s --connbytes-dir both --connbytes-mode bytes", min*1000, q_max);
			else{// q_max != NULL
				sprintf(tmp, "%s", q_max);
				max = atol(tmp);
				sprintf(conn, "-m connbytes --connbytes %ld:%ld --connbytes-dir both --connbytes-mode bytes", min*1000, max*1000-1);
			}
		}
		//fprintf(stderr, "[qos] tmp=%s, transferred=%s, min=%ld, max=%ld, q_max=%s, conn=%s\n", tmp, transferred, min*1000, max*1000-1, q_max, conn); // tmp test

		/*************************************************/
		/*                      proto                    */
		/*        tcp, udp, tcp/udp, any, (icmp, igmp)   */
		/*************************************************/
		memset(proto_1, 0, sizeof(proto_1));
		memset(proto_2, 0, sizeof(proto_2));
		if(!strcmp(proto, "tcp"))
		{
			sprintf(proto_1, "-p tcp");
			sprintf(proto_2, "NO");
		}
		else if(!strcmp(proto, "udp"))
		{
			sprintf(proto_1, "-p udp");
			sprintf(proto_2, "NO");
		}
		else if(!strcmp(proto, "any"))
		{
			sprintf(proto_1, "%s", "");
			sprintf(proto_2, "NO");
		}
		else if(!strcmp(proto, "tcp/udp"))
		{
			sprintf(proto_1, "-p tcp");
			sprintf(proto_2, "-p udp");
		}
		else{
			sprintf(proto_1, "NO");
			sprintf(proto_2, "NO");
		}
		//fprintf(stderr, "[qos] proto_1=%s, proto_2=%s, proto=%s\n", proto_1, proto_2, proto); // tmp test
		
		/*******************************************************************/
		/*                                                                 */
		/*  build final rule for check proto_1, proto_2, saddr_1, saddr_2  */
		/*                                                                 */
		/*******************************************************************/
		// step1. check proto != "NO"
		// step2. if proto = any, no proto / dport
		// step3. check saddr for ip-range; saddr_1 could be empty, dport only

		if (v4v6_ok & IPT_V4){
			// step1. check proto != "NO"
			if(strcmp(proto_1, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_1, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
					if(strcmp(maddr, "")){
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, maddr, conn, end2);
                                        }
					else{
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end2);
					}
                                        if(strcmp(saddr_2, "")){
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_2, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_2, conn, end2);
                                        }
				}
				else{
					if(strcmp(maddr, "")){
                                                fprintf(fn, "-A %s %s %s %s", chain, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s", chain, maddr, conn, end2);
                                        }
					else{
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end2);
					}
                                        if(strcmp(saddr_2, "")){
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_2, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_2, conn, end2);
                                        }
				}
			}

			// step1. check proto != "NO"
			if(strcmp(proto_2, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_2, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
					if(strcmp(maddr, "")){
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, maddr, conn, end2);
                                        }
					else{
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end2);
					}
                                        if(strcmp(saddr_2, "")){
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_2, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_2, conn, end2);
                                        }
				}
				else{
					if(strcmp(maddr, "")){
                                                fprintf(fn, "-A %s %s %s %s", chain, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s", chain, maddr, conn, end2);
                                        }
					else{
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end2);
					}
                                        if(strcmp(saddr_2, "")){
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_2, conn, end);
                                                if(manual_return)
                                                fprintf(fn, "-A %s %s %s %s", chain, saddr_2, conn, end2);
                                        }
				}
			}
		}
		
#ifdef RTCONFIG_IPV6
		if (ipv6_enabled() && (v4v6_ok & IPT_V6)){
			// step1. check proto != "NO"
			if(strcmp(proto_1, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_1, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
					// don't build ipv6 rules with ipv4 addresses
					if(strcmp(maddr, "")){
                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, maddr, conn, end2);
                                        }
					else{
						if (v6rule_ok) {
							fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end);
							if(manual_return)
							fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end2);
						}
					}
                                        if(strcmp(saddr_2, "")){
						if (v6rule_ok) {
							fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_2, conn, end);
							if(manual_return)
							fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_2, conn, end2);
						}
                                        }
					
				}
				else{
					if(strcmp(maddr, "")){
                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, maddr, conn, end2);
                                        }
					else{
						if (v6rule_ok) {
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end);
	                                                if(manual_return)
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end2);
						}
					}
                                        if(strcmp(saddr_2, "")){
						if (v6rule_ok) {
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_2, conn, end);
	                                                if(manual_return)
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_2, conn, end2);
						}
                                        }
				}
			}

			// step1. check proto != "NO"
			if(strcmp(proto_2, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_2, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
					// don't build ipv6 rules with ipv4 addresses
					if(strcmp(maddr, "")){
                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, maddr, conn, end2);
                                        }
					else{
						if (v6rule_ok) {
	                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end);
	                                                if(manual_return)
	                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end2);
						}
					}
                                        if(strcmp(saddr_2, "")){
						if (v6rule_ok) {
	                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_2, conn, end);
	                                                if(manual_return)
	                                                fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_2, conn, end2);
						}
                                        }
				}
				else{
					if(strcmp(maddr, "")){
                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, maddr, conn, end);
                                                if(manual_return)
                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, maddr, conn, end2);
                                        }
					else{
						if (v6rule_ok) {
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end);
	                                                if(manual_return)
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end2);
						}
					}
                                        if(strcmp(saddr_2, "")){
						if (v6rule_ok) {
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_2, conn, end);
	                                                if(manual_return)
	                                                fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_2, conn, end2);
						}
                                        }
				}
			}
		}
#endif
	}
	free(buf);

	method = nvram_get_int("qos_method");	// strict rule ordering
	gum = (method == 0) ? 0x100 : 0;

	/* lan_addr for iptables use (LAN download) */
	char *a, *b, *c, *d;
	char lan_addr[20];
	char ipv6_lan_addr[44];
	int ipv4_err, ipv6_err;

	ipv4_err = 1;
	ipv6_err = 2;

	/* ipv4 lan_addr for iptables use (LAN download) */
	for ( i = 1; i <= MAX_RETRY; i++ ) {
		g = buf = strdup(nvram_safe_get("lan_ipaddr"));
		if((vstrsep(g, ".", &a, &b, &c, &d)) != 4){
			fprintf(stderr,"[qos] lan_ipaddr doesn't exist!!\n");
			//logmessage("qos-rules","ipv4_lan_ipaddr doesn't exist, retrying");
			sleep(1);
		}
		else{
			sprintf(lan_addr, "%s.%s.%s.0/24", a, b, c);
			fprintf(stderr,"[qos] lan_addr=%s\n", lan_addr);
			logmessage("qos-rules","using ipv4_lan_ipaddr %s", lan_addr);
			ipv4_err = 0;
			i = MAX_RETRY + 1;
		}
	}
	if (ipv4_err > 0)
		logmessage("qos-rules","error setting ipv4_lan_ipaddr!");
	free(buf);

#ifdef RTCONFIG_IPV6
	if (ipv6_enabled()) {
		/* ipv6_lan_addr for ip6tables use (LAN download) */
		for ( i = 1; i <= MAX_RETRY; i++ ) {
			g = buf = strdup(nvram_safe_get("ipv6_prefix"));
			if (!strlen(g)){
				fprintf(stderr,"[qos] ipv6_lan_ipaddr doesn't exist!!\n");
				//logmessage("qos-rules","ipv6_lan_ipaddr doesn't exist, retrying");
				sleep(1);
			}
			else{
				sprintf(ipv6_lan_addr, "%s/%d", g, nvram_get_int("ipv6_prefix_length") ? : 64);
                                strlcpy(wan6face, get_wan6face(), IFNAMSIZ+1);  //refresh ipv6 ifname
				nvram_set("ipv6_ifname", wan6face);
				fprintf(stderr,"[qos] ipv6_lan_addr=%s\n", ipv6_lan_addr);
				logmessage("qos-rules","using ipv6_lan_ipaddr %s on interface %s", ipv6_lan_addr, wan6face);
				ipv6_err = 0;
				i = MAX_RETRY + 1;
			}
		}
		if (ipv6_err > 0)
			logmessage("qos-rules","error setting ipv6_lan_ipaddr!");
		free(buf);
	}
	else{
		ipv6_err = 0;
	}
#endif

	nvram_set_int("qos_addr_err", (ipv4_err + ipv6_err));

	// access router : mark 9
	fprintf(fn,
		"-A POSTROUTING -s %s -d %s -j CONNMARK --set-return 0x9/0x1ff\n"
		"-A PREROUTING -s %s -d %s -j CONNMARK --set-return 0x9/0x1ff\n"
//		, nvram_safe_get("lan_ipaddr"), lan_addr
//		, lan_addr, nvram_safe_get("lan_ipaddr")
		, lan_addr, lan_addr
		, lan_addr, lan_addr
		);

	if(manual_return){
	fprintf(fn,
		"-A POSTROUTING -s %s -d %s -j RETURN\n"
		"-A PREROUTING -s %s -d %s -j RETURN\n"
//		, nvram_safe_get("lan_ipaddr"), lan_addr
//		, lan_addr, nvram_safe_get("lan_ipaddr")
		, lan_addr, lan_addr
		, lan_addr, lan_addr
		);
	}

#ifdef RTCONFIG_IPV6
		if (ipv6_enabled() && *wan6face) {
			fprintf(fn_ipv6,
				"-A POSTROUTING -s %s -d %s -j CONNMARK --set-return 0x9/0x1ff\n"
				"-A PREROUTING -s %s -d %s -j CONNMARK --set-return 0x9/0x1ff\n"
//				, nvram_safe_get("ipv6_rtr_addr"), ipv6_lan_addr
//				, ipv6_lan_addr, nvram_safe_get("ipv6_rtr_addr")
				, ipv6_lan_addr, ipv6_lan_addr
				, ipv6_lan_addr, ipv6_lan_addr
				);

			if(manual_return){
			fprintf(fn_ipv6,
				"-A POSTROUTING -s %s -d %s -j RETURN\n"
				"-A PREROUTING -s %s -d %s -j RETURN\n"
//				, nvram_safe_get("ipv6_rtr_addr"), ipv6_lan_addr
//				, ipv6_lan_addr, nvram_safe_get("ipv6_rtr_addr")
				, ipv6_lan_addr, ipv6_lan_addr
				, ipv6_lan_addr, ipv6_lan_addr
				);
			}
		}
#endif

	//fprintf(stderr, "[qos] down_class_num=%x\n", down_class_num);

	/* The default class */
        i = nvram_get_int("qos_default");
        if ((i < 0) || (i > 4)) i = 3;  // "low"
        class_num = i + 1;

#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	if(strncmp(pcWANIF, "ppp", 3)==0){
		// ppp related interface doesn't need physdev
		// do nothing
	}
	else{
		/* for WLAN to LAN bridge packet */
		// ebtables : identify bridge packet
		add_EbtablesRules();

		// for multicast
		fprintf(fn, "-A QOSO -d 224.0.0.0/4 -j CONNMARK --set-return 0x%x/0x1ff\n",
			1|gum);
                if(manual_return)
                        fprintf(fn , "-A QOSO -d 224.0.0.0/4 -j RETURN\n");
/*
		// for download (LAN or wireless)
		fprintf(fn, "-A QOSO -d %s -j CONNMARK --set-return 0x%x/0xf\n", lan_addr, down_class_num);
                if(manual_return)
                        fprintf(fn , "-A QOSO -d %s -j RETURN\n", lan_addr);
*/
/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
		// for WLAN to LAN bridge issue
		fprintf(fn, "-A POSTROUTING -d %s -m physdev --physdev-is-in -j CONNMARK --set-return 0x6/0xf\n", lan_addr);
*/
		// for download, interface br0
		fprintf(fn, "-A POSTROUTING -o br0 -j QOSO\n");
	}
#endif
		fprintf(fn,
			"-A QOSO -j CONNMARK --set-return 0x%x/0x1ff\n"
	                "-A FORWARD -o %s -j QOSO\n"
        	        "-A OUTPUT -o %s -j QOSO\n",
                	        class_num|gum, pcWANIF, pcWANIF);
                if(manual_return)
                        fprintf(fn , "-A QOSO -j RETURN\n");

#ifdef RTCONFIG_IPV6
	if (ipv6_enabled() && *wan6face) {
#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
		if(strncmp(wan6face, "ppp", 3)==0){
			// ppp related interface doesn't need physdev
			// do nothing
		}
		else{
			/* for WLAN to LAN bridge packet */
			// ebtables : identify bridge packet
			add_EbtablesRules();

			// for multicast
			fprintf(fn_ipv6, "-A QOSO -d ff00::/8 -j CONNMARK --set-return 0x%x/0x1ff\n",
				1|gum);
                        if(manual_return)
                                fprintf(fn_ipv6, "-A QOSO -d ff00::/8 -j RETURN\n");
/*
			// for download (LAN or wireless)
			fprintf(fn_ipv6, "-A QOSO -d %s -j CONNMARK --set-return 0x%x/0x1ff\n", ipv6_lan_addr, down_class_num);
                        if(manual_return)
                                fprintf(fn_ipv6, "-A QOSO -d %s -j RETURN\n", ipv6_lan_addr);
*/
/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
			// for WLAN to LAN bridge issue
			fprintf(fn_ipv6, "-A POSTROUTING -d %s -m physdev --physdev-is-in -j CONNMARK --set-return 0x6/0x1ff\n", lan_addr);
*/
			// for download, interface br0
			fprintf(fn_ipv6, "-A POSTROUTING -o br0 -j QOSO\n");
		}
#endif
        	fprintf(fn_ipv6,
			"-A QOSO -j CONNMARK --set-return 0x%x/0x1ff\n"
                	"-A FORWARD -o %s -j QOSO\n"
			"-A OUTPUT -o %s -p icmpv6 -j RETURN\n"
                	"-A OUTPUT -o %s -j QOSO\n",
				class_num|gum, wan6face, wan6face, wan6face);
		if(manual_return)
			fprintf(fn_ipv6, "-A QOSO -j RETURN\n");
	}
#endif

        inuse |= (1 << i) | 1;  // default and highest are always built
        sprintf(q_inuse, "%d", inuse);
        nvram_set("qos_inuse", q_inuse);
	fprintf(stderr, "[qos] qos_inuse=%d\n", inuse);

	/* Ingress rules */
        g = buf = strdup(nvram_safe_get("qos_irates"));
        for (i = 0; i < 10; ++i) {
                if ((!g) || ((p = strsep(&g, ",")) == NULL)) continue;
                if ((inuse & (1 << i)) == 0) continue;
                if (atoi(p) > 0) {
                        fprintf(fn, "-A PREROUTING -m state --state RELATED,ESTABLISHED -j CONNMARK --restore-mark --mask 0x1ff\n");
#ifdef CLS_ACT
			fprintf(fn, "-A PREROUTING -i %s -j IMQ --todev 0\n", pcWANIF);
#endif
#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && *wan6face) {
				fprintf(fn_ipv6, "-A PREROUTING -m state --state RELATED,ESTABLISHED -j CONNMARK --restore-mark --mask 0x1ff\n");
#ifdef CLS_ACT
				fprintf(fn_ipv6, "-A PREROUTING -i %s -j IMQ --todev 0\n", wan6face);
#endif
			}
#endif
                        break;
                }
        }
        free(buf);

	fprintf(fn, "COMMIT\n");
	fclose(fn);
	chmod(mangle_fn, 0700);

	if (ipv4_err == 0) {
		// add retries to iptables rules apply
		for ( i = 1; i <= MAX_RETRY; i++ ) {
			if (eval("iptables-restore", "-n", (char*)mangle_fn)) {
				_dprintf("iptables-restore failed - attempt: %d ...\n", i);
				if (i == MAX_RETRY)
					logmessage("qos-rules", "apply rules (%s) failed!", mangle_fn);
				sleep(1);
			} else {
				logmessage("qos-rules", "apply rules (%s) success!", mangle_fn);
				i = MAX_RETRY + 1;
			}
		}
	} else {
		logmessage("qos-rules", "ipv4_lan_addr not available - skipping rules (%s)", mangle_fn);
	}

#ifdef RTCONFIG_IPV6
	if (ipv6_enabled())
	{
		fprintf(fn_ipv6, "COMMIT\n");
		fclose(fn_ipv6);
		chmod(mangle_fn_ipv6, 0700);

		if (ipv6_err == 0) {
			// add retries to ip6tables rules apply
			for ( i = 1; i <= MAX_RETRY; i++ ) {
				if (eval("ip6tables-restore", "-n", (char*)mangle_fn_ipv6)) {
					_dprintf("ip6tables-restore failed - attempt: %d ...\n", i);
					if (i == MAX_RETRY)
						logmessage("qos-rules", "apply rules (%s) failed!", mangle_fn_ipv6);
					sleep(1);
				} else {
					logmessage("qos-rules", "apply rules (%s) success!", mangle_fn_ipv6);
					i = MAX_RETRY + 1;
				}
			}
		} else {
			logmessage("qos-rules", "ipv6_lan_addr not available - skipping rules (%s)", mangle_fn_ipv6);
		}
	}
#endif

	run_custom_script("qos-start", "rules");
	fprintf(stderr, "[qos] iptables DONE!\n");

	return 0;
}


/*******************************************************************/
// The definations of all partations
// eth0 : WAN
// 1:1  : upload   
// 1:2  : download   (1000000Kbits)
// 1:10 : highest
// 1:20 : high
// 1:30 : middle
// 1:40 : low        (default)
// 1:50 : lowest
// 1:60 : ALL Download (WAN to LAN and LAN to LAN) (1000000kbits)
/*******************************************************************/

/* Tc */
int start_tqos(void)
{
	int i;
	char *buf, *g, *p;
	char *buf2, *g2, *p2;
	char *wan_ifname;
	unsigned int rate, irate_min;
	unsigned int ceil;
	unsigned int ibw, obw, bw;
	unsigned int ibw_max, obw_max;
	unsigned int mtu;
	unsigned int ceiling_factor;
	FILE *f;
	int x;
	int inuse;
	char s[256];
	int first;
	char burst_root[32];
	char burst_leaf[32];
	char r2q[32];
	char sfq_limit[32];
	char qsched[64];
#ifdef CONFIG_BCMWL5
	char *protocol="ip";
	char *protocol6="ipv6";
#endif
//	int down_class_num=6;   // for download class_num = 0x6 / 0x106
	int overhead = 0;
	char overheadstr[sizeof("overhead 64 linklayer atm")];
	unsigned int l, u, v;  // vars for use in intermediate bw calcs

	// judge interface by get_wan_ifname
	// add Qos iptable rules in mangle table,
	// move it to firewall - mangle_setting
	// add_iQosRules(get_wan_ifname(0)); // iptables start

	if (!nvram_match("qos_enable", "1")) return -1;

	wan_ifname = get_wan_ifname(wan_primary_ifunit());
	if (!wan_ifname || (strlen(wan_ifname) <= 0)) {
		logmessage("qos-init", "wan_ifname not defined!");
		nvram_set_int("qos_addr_err", 4);
		return -2;
	}
	else
		logmessage("qos-init", "using wan_ifname %s", wan_ifname);

	ibw = strtoul(nvram_safe_get("qos_ibw"), NULL, 10);
	obw = strtoul(nvram_safe_get("qos_obw"), NULL, 10);
	if(ibw==0||obw==0) return -1;
	ibw_max = obw_max = 10000000; //10Gb
	mtu = strtoul(nvram_safe_get("wan_mtu"), NULL, 10) + 14; //add 14 bytes for hardware header

	stop_iQos();  // remove existing tc classes
	if ((f = fopen(qosfn, "w")) == NULL) return -2;

	fprintf(stderr, "[qos] tc START!\n");

	/* r2q */
	if ((i = nvram_get_int("qos_r2q")) == 0) {
		i = 10;
		if ((ibw * 1000) / (8 * i) < mtu)
		{
			i = (ibw * 1000) / (8 * mtu);
			if (i < 1) i = 1;
		}
		else if ((ibw * 1000) / (8 * i) > 60000)
		{
			i = (ibw * 1000) / (8 * 60000) + 1;
		}
	}
	if (i > 0) sprintf(r2q, "r2q %d", i);
		else r2q[0] = 0;

	/* sfq limit */
        i = nvram_get_int("qos_sfql");
        if (i > 1)
		sprintf(sfq_limit, "limit %d", i);
	/*
	else if (i == 1) {
		x = (obw/1000) + 10;
		x = (x > 127) ? 127 : x;
		sprintf(sfq_limit, "limit %d", x);
	}
	*/
	else
		sfq_limit[0] = 0;

	/* ceiling factor */
	i = nvram_get_int("qos_cfactor");
	if (i == 0)
		i = 10; //default 10% ceiling adjustment for download bandwidth limiting
	ceiling_factor = i + 100;

	/* Egress OBW  -- set the HTB shaper (Classful Qdisc)  
	* the BW is set here for each class 
	*/
	bw = obw;
	/* qos_burst root */
	v = obw / 50; // recommended 2% of rate
	if (v < 50) v = 50;
		sprintf(burst_root, "burst %dk cburst %dk", v, v);

#ifdef RTCONFIG_BCMARM
	switch(nvram_get_int("qos_sched")){
		case 1:
			sprintf(qsched, "codel");
			break;
		case 2:
			sprintf(qsched, "fq_codel quantum %u limit 1000 noecn", mtu);
			break;
		default:
			/* qsched = "sfq perturb 10"; */
			sprintf(qsched, "sfq perturb 10 %s", sfq_limit);
			break;
	}
#else
	sprintf(qsched, "sfq perturb 10 %s", sfq_limit);
#endif

#ifdef RTCONFIG_BCMARM
	overhead = nvram_get_int("qos_overhead")%100;

	if (overhead > 0)
		snprintf(overheadstr, sizeof(overheadstr),"overhead %d linklayer atm", overhead);
	else
		strcpy(overheadstr, "");
#else
	strcpy(overheadstr, "");
#endif

	/* WAN */
	fprintf(f,
		"#!/bin/sh\n"
		"#LAN/WAN\n"
		"I=%s\n"
		"SFQ=\"%s\"\n"
		"TQA=\"tc qdisc add dev $I\"\n"
		"TCA=\"tc class add dev $I\"\n"
		"TFA=\"tc filter add dev $I\"\n"
#ifdef CLS_ACT
		"DLIF=imq0\n"
#else
		"DLIF=br0\n"
#endif
		"TQADL=\"tc qdisc add dev $DLIF\"\n"
		"TCADL=\"tc class add dev $DLIF\"\n"
		"TFADL=\"tc filter add dev $DLIF\"\n"

		"case \"$1\" in\n"
		"start)\n"
		"#LAN/WAN\n"
		"\ttc qdisc del dev $I root 2>/dev/null\n"
		"\ttc qdisc del dev $I ingress 2>/dev/null\n"
		"\t$TQA root handle 1: htb %s\n"
		"\ttc qdisc del dev $DLIF root 2>/dev/null\n"
		"\ttc qdisc del dev $DLIF ingress 2>/dev/null\n"
		"\t$TQADL root handle 2: htb %s\n"

		"# upload 1:1 (unrestricted)\n"
		"\t$TCA parent 1: classid 1:1 htb rate %ukbit ceil %ukbit %s %s\n"
		"# upload 1:2 (ouput bandwidth)\n"
		"\t$TCA parent 1: classid 1:2 htb rate %ukbit ceil %ukbit %s %s\n" ,
			(nvram_get("qos_iface") ? : get_wan_ifname(wan_primary_ifunit())), // judge WAN interface
			qsched,
			r2q, r2q,
			obw_max, obw_max, burst_root, overheadstr,
			obw, calc(obw, ceiling_factor), burst_root, overheadstr);

	inuse = nvram_get_int("qos_inuse");

	/* LAN protocol: 802.1q */
#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	protocol = "ip"; // base protocol ipv4
	protocol6 = "ipv6"; // need separate filter for ipv6 for upload
	irate_min = 100;
	g = buf = strdup(nvram_safe_get("qos_irates"));
	for (i = 0; i < 5; ++i) {
		if ((!g) || ((p = strsep(&g, ",")) == NULL)) break;
		if ((inuse & (1 << i)) == 0) continue;
		if ((rate = atoi(p)) < 1) continue;     // 0 = off

		if ((rate > 0) && (rate < irate_min)) irate_min = rate; // get minimum in use download rate
	}
/*

	fprintf(f,
		"# download 2:1\n"
		"\t$TCADL parent 2: classid 2:1 htb rate %ukbit ceil %ukbit %s %s\n",
		"# 2:60 ALL Download for BCM\n"
		"\t$TCADL parent 2:1 classid 2:60 htb rate %ukbit ceil %ukbit burst 100000 cburst 100000 prio 6 quantum %u\n"
		"\t$TQADL parent 2:60 handle 60: pfifo\n"
		"\t$TFADL parent 2: prio 60 protocol all handle %d fw flowid 2:60\n",
		ibw_max, ibw_max, burst_root, overheadstr,
		nvram_get_int("qos_ibwopt") ? calc(ibw, irate_min) : ibw, nvram_get_int("qos_ibwopt") ? calc(ibw, ceiling_factor) : ibw_max, mtu,
		down_class_num);
*/
#endif

	// access local lan : mark 9
	// default : 10Gbps
	fprintf(f,
		"# access lan\n"
		"\t$TCA parent 1:1 classid 1:9 htb rate %ukbit ceil %ukbit prio 0\n"
		"\t$TQA parent 1:9 handle 9: $SFQ\n"
		"\t$TFA parent 1: prio 0 protocol %s handle 9 fw flowid 1:9\n",
		obw_max, obw_max, protocol);
#ifdef RTCONFIG_IPV6
	if (ipv6_enabled() && *wan6face) {
	fprintf(f,
		"\t$TFA parent 1: prio 1 protocol %s handle 9 fw flowid 1:9\n",
		protocol6);
	}
#endif

	g = buf = strdup(nvram_safe_get("qos_orates"));
	for (i = 0; i < 5; ++i) { // 0~4 , 0:highest, 4:lowest

		if ((!g) || ((p = strsep(&g, ",")) == NULL)) break;

		if ((inuse & (1 << i)) == 0){
			fprintf(stderr, "[qos] egress %d doesn't create, inuse=%d\n", i, inuse );
			continue;
		}
		else
			fprintf(stderr, "[qos] egress %d creates\n", i);

		if ((sscanf(p, "%u-%u", &rate, &ceil) != 2) || (rate < 1)) continue; 

		if (ceil > 0) sprintf(s, "ceil %ukbit ", calc(bw, ceil));
			else s[0] = 0;

		// rate in kb/s
		u = calc(bw, rate);

		// burst rate
		v = u / 50; // recommended 2% of rate
		if (v < 50) v = 50;
		sprintf(burst_leaf, "burst %dk cburst %dk", v, v);

		x = (i + 1) * 10;

		fprintf(f,
			"# egress %d: %u-%u%%\n"
			"\t$TCA parent 1:2 classid 1:%d htb rate %ukbit %s %s prio %d quantum %u %s\n"
			"\t$TQA parent 1:%d handle %d: $SFQ\n"
			"\t$TFA parent 1: prio %d protocol %s handle %d fw flowid 1:%d\n"
			"\t$TFA parent 1: prio %d protocol %s handle %d fw flowid 1:%d\n",
				i, rate, ceil,
				x, calc(bw, rate), s, burst_leaf, (i >= 6) ? 7 : (i + 1), mtu, overheadstr,
				x, x,
				x, protocol, (i + 1), x,
				x, protocol, (i + 1)|0x100, x);
#ifdef RTCONFIG_IPV6
		if (ipv6_enabled() && *wan6face) {
			fprintf(f,
			"\t$TFA parent 1: prio %d protocol %s handle %d fw flowid 1:%d\n"
			"\t$TFA parent 1: prio %d protocol %s handle %d fw flowid 1:%d\n",
			x + 1, protocol6, (i + 1), x,
			x + 1, protocol6, (i + 1)|0x100, x);
		}
#endif
	}
	free(buf);

	/*
		10000 = ACK
		00100 = RST
		00010 = SYN
		00001 = FIN
	*/

	if (nvram_match("qos_ack", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 14 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x10 0xff at 33 "			// ACK only
			"flowid 1:10\n");
	}
	if (nvram_match("qos_syn", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 15 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x02 0x02 at 33 "			// SYN,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_fin", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 17 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x01 0x01 at 33 "			// FIN,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_rst", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 19 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x04 0x04 at 33 "			// RST,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_icmp", "on")) {
		fputs("\n\t$TFA parent 1: prio 13 protocol ip u32 match ip protocol 1 0xff flowid 1:10\n", f);
	}

	// ingress
	first = 1;
	bw = ibw;
	/* qos_burst root */
	v = ibw / 50; // recommended 2% of rate
	if (v < 50) v = 50;
		sprintf(burst_root, "burst %dk cburst %dk", v, v);

	if (bw > 0) {
		g = buf = strdup(nvram_safe_get("qos_irates"));
		g2 = buf2 = strdup(nvram_safe_get("qos_irates_min"));
		for (i = 0; i < 5; ++i) { // 0~4 , 0:highest, 4:lowest 
			if ((!g) || ((p = strsep(&g, ",")) == NULL) ||
				(!g2) || ((p2 = strsep(&g2, ",")) == NULL)) break;
			if ((inuse & (1 << i)) == 0) continue;
			if ((ceil = atoi(p)) < 1) continue;	// 0 = off

			if (first) {
				first = 0;
				fprintf(f,
					"\n"
//#if !defined(CLS_ACT)
					"# download 2:1 (unrestricted)\n"
					"\t$TCADL parent 2: classid 2:1 htb rate %ukbit ceil %ukbit %s %s\n"
					"# download 2:2 (input bandwidth)\n"
					"\t$TCADL parent 2: classid 2:2 htb rate %ukbit ceil %ukbit %s %s\n"
					"# access lan\n"
					"\t$TCADL parent 2:1 classid 2:9 htb rate %ukbit ceil %ukbit prio 1\n"
					"\t$TQADL parent 2:9 handle 9: $SFQ\n"
					"\t$TFADL parent 2: prio 1 protocol all handle 9 fw flowid 2:9\n",
						ibw_max, ibw_max, burst_root, overheadstr,
						ibw, ibw_max, burst_root, overheadstr,
						ibw_max, ibw_max

//					"\ttc qdisc del dev $DLIF ingress 2>/dev/null\n"
//					"\t$TQADL handle ffff: ingress\n"
//#endif
					);
			}	

			// rate in kb/s
			u = calc(bw, ceil);

			// lowest rate to try and maintain
			if (atoi(p2) == 0)
				rate = 10;  // 10% of input bandwidth
			else
				rate = atoi(p2);
			l = calc(bw, rate);

			// burst rate
			v = u / 50; // recommended 2% of rate
			if (v < 50) v = 50;
			sprintf(burst_leaf, "burst %dk cburst %dk", v, v);

//#ifdef CLS_ACT
			x = (i + 1) * 10;
			fprintf(f,
				"# ingress %d: %u-%u%%\n"
				"\t$TCADL parent 2:2 classid 2:%d htb rate %ukbit ceil %ukbit %s prio %d quantum %u %s\n"
				"\t$TQADL parent 2:%d handle %d: $SFQ\n"
				"\t$TFADL parent 2: prio %d protocol all handle %d fw flowid 2:%d\n"
				"\t$TFADL parent 2: prio %d protocol all handle %d fw flowid 2:%d\n",
					i, rate, ceil,
					x, l, u, burst_leaf, (i >= 6) ? 7 : (i + 1), mtu, overheadstr,
					x, x,
					x, (i + 1), x,
					x, (i + 1)|0x100, x);
/* #else
			x = i + 1;
			fprintf(f,
				"# ingress %d: %u%%\n"
				"\t$TFADL parent ffff: prio %d protocol all handle %d"
					" fw police rate %ukbit burst %ukbit mtu %u drop flowid ffff:%d\n",
					i, rate,
					x, x,
					u, v, mtu, x);

			if (i == nvram_get_int("qos_default")) { // police filter for downloads
			x = down_class_num;
			fprintf(f,
                                "# ingress %d: %u%% (download default)\n"
                                "\t$TFADL parent ffff: prio %d protocol all handle %d"
                                        " fw police rate %ukbit burst %ukbit mtu %u drop flowid ffff:%d\n",
                                        (x - 1), rate, x, x, u, v, mtu, x);
			}
#endif
*/
		}
		free(buf);
	}

	fputs(
		"\t;;\n"
		"stop)\n"
		"\ttc qdisc del dev $I root 2>/dev/null\n"
		"\ttc qdisc del dev $DLIF root 2>/dev/null\n"
		"\ttc qdisc del dev $I ingress 2>/dev/null\n"
		"\ttc qdisc del dev $DLIF ingress 2>/dev/null\n"
		"\t;;\n"
		"*)\n"
		"\t#---------- Upload ----------\n"
		"\ttc -s -d class ls dev $I\n"
		"\ttc -s -d qdisc ls dev $I\n"
		"\techo\n"

		"\t#--------- Download ---------\n"
		"\ttc -s -d class ls dev $DLIF\n"
		"\ttc -s -d qdisc ls dev $DLIF\n"
		"\techo\n"
/*
#if !defined(CLS_ACT)
		"\ttc -s filter show dev $DLIF parent ffff:\n"
		"\techo\n"
#endif
*/
		"esac\n",
		f);

	fclose(f);
	chmod(qosfn, 0700);
	eval((char *)qosfn, "start");
	logmessage("qos-init", "tqos start complete");

	run_custom_script("qos-start", "init");
	fprintf(stderr,"[qos] tc done!\n");

	return 0;
}


void stop_iQos(void)
{
	eval((char *)qosfn, "stop");
}

#define TYPE_IP 0
#define TYPE_MAC 1
#define TYPE_IPRANGE 2
#define TYPE_GUEST 3

static void address_checker(int *addr_type, char *addr_old, char *addr_new, int len)
{
	char *second, *last_dot;
	int len_to_minus, len_to_dot;

	// guestnetwork interface
	if (strstr(addr_old, "wl")){
		*addr_type = TYPE_GUEST;
		strncpy(addr_new, addr_old, len);
		return;
	}

	second = strchr(addr_old, '-');
	if (second != NULL)
	{
		*addr_type = TYPE_IPRANGE;
		if (strchr(second+1, '.') != NULL){
			// long notation
			strncpy(addr_new, addr_old, len);
		}
		else{
			// short notation
			last_dot = strrchr(addr_old, '.');
			len_to_minus = second - addr_old;
			len_to_dot = last_dot - addr_old;
			strncpy(addr_new, addr_old, len_to_minus+1);
			strncpy(addr_new + len_to_minus + 1, addr_new, len_to_dot+1);
			strcpy(addr_new + len_to_minus + len_to_dot + 2, second+1);
		}
	}
	else
	{
		if (strlen(addr_old) == 17)
			*addr_type = TYPE_MAC;
		else
			*addr_type = TYPE_IP;
		strncpy(addr_new, addr_old, len);
	}
}

static int add_bandwidth_limiter_rules(char *pcWANIF)
{
	FILE *fn = NULL;
	char *buf, *g, *p;
	char *enable, *addr, *dlc, *upc, *prio;
	char lan_addr[32];
	char addr_new[32];
	int i, addr_type;
	char *action = NULL;
	char mssid_mark[4];

#ifdef CONFIG_BCMWL5
	del_EbtablesRules(); // flush ebtables nat table
#endif
	del_iQosRules(); // flush all rules in mangle table

	if ((fn = fopen(mangle_fn, "w")) == NULL) return -2;

	switch (get_model()){
		case MODEL_DSLN55U:
		case MODEL_RTN13U:
		case MODEL_RTN56U:
			action = "CONNMARK --set-return";
			manual_return = 0;
			break;
		default:
			action = "MARK --set-mark";
			manual_return = 1;
			break;
	}

	/* ASUSWRT
	qos_bw_rulelist :
		enable>addr>DL-Ceil>UL-Ceil>prio
		enable : enable or disable this rule
		addr : (source) IP or MAC or IP-range or wireless interface(wl0.1, wl0.2, etc.)
		DL-Ceil : the max download bandwidth
		UL-Ceil : the max upload bandwidth
		prio : priority for client
	*/

	memset(lan_addr, 0, sizeof(lan_addr));
	sprintf(lan_addr, "%s/%s", nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));

	fprintf(fn,
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		);

	// access lan : mark 9
	fprintf(fn,
		"-A POSTROUTING -s %s -d %s -j %s 9\n"
		"-A PREROUTING -s %s -d %s -j %s 9\n"
//		, nvram_safe_get("lan_ipaddr"), lan_addr, action
//		, lan_addr, nvram_safe_get("lan_ipaddr"), action
		, lan_addr, lan_addr, action
		, lan_addr, lan_addr, action
		);

	if(manual_return){
	fprintf(fn,
		"-A POSTROUTING -s %s -d %s -j RETURN\n"
		"-A PREROUTING -s %s -d %s -j RETURN\n"
//		, nvram_safe_get("lan_ipaddr"), lan_addr
//		, lan_addr, nvram_safe_get("lan_ipaddr")
		, lan_addr, lan_addr
		, lan_addr, lan_addr
		);
	}

	g = buf = strdup(nvram_safe_get("qos_bw_rulelist"));
	while (g) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if ((vstrsep(p, ">", &enable, &addr, &dlc, &upc, &prio)) != 5) continue;
		if (!strcmp(enable, "0")) continue;
		memset(addr_new, 0, sizeof(addr_new));
		address_checker(&addr_type, addr, addr_new, sizeof(addr_new));
		_dprintf("[BWLIT] %s: addr_type=%d, addr=%s, add_new=%s, lan_addr=%s\n", __FUNCTION__, addr_type, addr, addr_new, lan_addr);

		if (addr_type == TYPE_IP){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -d %s -j %s %d\n"
				"-A PREROUTING -s %s ! -d %s -j %s %d\n"
				, lan_addr, addr_new, action, atoi(prio)+10
				, addr_new, lan_addr, action, atoi(prio)+10
				);
			if(manual_return){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -d %s -j RETURN\n"
				"-A PREROUTING -s %s ! -d %s -j RETURN\n"
				, lan_addr, addr_new, addr_new, lan_addr
				);
			}
		}
		else if (addr_type == TYPE_MAC){
			fprintf(fn,
				"-A PREROUTING -m mac --mac-source %s ! -d %s  -j %s %d\n"
				, addr_new, lan_addr, action, atoi(prio)+10
				);
			if(manual_return){
			fprintf(fn,
				"-A PREROUTING -m mac --mac-source %s ! -d %s  -j RETURN\n"
				, addr_new, lan_addr
				);
			}
		}
		else if (addr_type == TYPE_IPRANGE){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -m iprange --dst-range %s -j %s %d\n"
				"-A PREROUTING -m iprange --src-range %s ! -d %s -j %s %d\n"
				, lan_addr, addr_new, action, atoi(prio)+10
				, addr_new, lan_addr, action, atoi(prio)+10
				);
			if(manual_return){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -m iprange --dst-range %s -j RETURN\n"
				"-A PREROUTING -m iprange --src-range %s ! -d %s -j RETURN\n"
				, lan_addr, addr_new, addr_new, lan_addr
				);
			}
		}
		else if (addr_type == TYPE_GUEST){
			snprintf(mssid_mark, sizeof(mssid_mark), "%d", atoi(prio)+10);
			eval("ebtables", "-t", "nat", "-A", "PREROUTING", "-i", addr_new, "-j", "mark", "--set-mark", mssid_mark, "--mark-target", "ACCEPT");
			eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", addr_new, "-j", "mark", "--set-mark", mssid_mark, "--mark-target", "ACCEPT");
		}
	}
	free(buf);

	fprintf(fn, "COMMIT\n");
	fclose(fn);
	chmod(mangle_fn, 0700);
	for ( i = 1; i <= MAX_RETRY; i++ ) {
		if (eval("iptables-restore", "-n", (char*)mangle_fn)) {
			_dprintf("iptables-restore failed - attempt: %d ...\n", i);
			if (i == MAX_RETRY)
				logmessage("qos-rules", "apply rules (%s) failed!", mangle_fn);
			sleep(1);
		} else {
			logmessage("qos-rules", "apply rules (%s) success!", mangle_fn);
			i = MAX_RETRY + 1;
		}
	}
	_dprintf("[BWLIT] %s: create rules\n", __FUNCTION__);

	return 0;
}

static int guest; // qdisc root only 3: ~ 12: (9 guestnetwork)

static int start_bandwidth_limiter(void)
{
	FILE *f = NULL;
	char *buf, *g, *p;
	char bw_iface[128] = " ";
	char word[8], *next;
	char *enable, *addr, *dlc, *upc, *prio;
	char *wan_ifname;
	int class = 0;
	int s[6]; // strip mac address
	int addr_type;
	char addr_new[30];
	char qsched[32];
	unsigned int mtu;

	if (!nvram_match("qos_enable", "1")) return -1;

	wan_ifname = get_wan_ifname(wan_primary_ifunit());
	if (!wan_ifname || (strlen(wan_ifname) <= 0)) {
		logmessage("qos-init", "wan_ifname not defined!");
		return -2;
	}
	else
		logmessage("qos-init", "using wan_ifname %s", wan_ifname);

	mtu = strtoul(nvram_safe_get("wan_mtu"), NULL, 10) + 14; //add 14 bytes for hardware header

	// init guest 3: ~ 12: (9 guestnetwork), start number = 3
	guest = 3;

#ifdef RTCONFIG_BCMARM
	switch(nvram_get_int("qos_sched")){
		case 1:
			sprintf(qsched, "codel");
			break;
		case 2:
			sprintf(qsched, "fq_codel quantum %u", mtu);
			break;
		default:
			sprintf(qsched, "sfq perturb 10");
			break;
	}
#else
	sprintf(qsched, "sfq perturb 10");
#endif

	if ((f = fopen(qosfn, "w")) == NULL) return -2;
	fprintf(f,
		"#!/bin/sh\n"
		"WAN=%s\n"
		"\n"
		"case \"$1\" in\n"
		"start)\n"
		, (nvram_get("qos_iface") ? : get_wan_ifname(wan_primary_ifunit()))
	);

	/* ASUSWRT
	qos_bw_rulelist :
		enable>addr>DL-Ceil>UL-Ceil>prio
		enable : enable or disable this rule
		addr : (source) IP or MAC or IP-range or wireless interface(wl0.1, wl0.2, etc.)
		DL-Ceil : the max download bandwidth
		UL-Ceil : the max upload bandwidth
		prio : priority for client
	*/

	g = buf = strdup(nvram_safe_get("qos_bw_rulelist"));

	// if no qos_bw_rulelist, shouldn't set tc rule
	if (strcmp(g, "")) {
		fprintf(f,
		"tc qdisc del dev $WAN root 2>/dev/null\n"
		"tc qdisc del dev $WAN ingress 2>/dev/null\n"
		"tc qdisc del dev br0 root 2>/dev/null\n"
		"tc qdisc del dev br0 ingress 2>/dev/null\n"
		"\n"
		"TQAU=\"tc qdisc add dev $WAN\"\n"
		"TCAU=\"tc class add dev $WAN\"\n"
		"TFAU=\"tc filter add dev $WAN\"\n"
		"SFQ=\"%s\"\n"
		"TQA=\"tc qdisc add dev br0\"\n"
		"TCA=\"tc class add dev br0\"\n"
		"TFA=\"tc filter add dev br0\"\n"
		"\n"
		"$TQA root handle 1: htb\n"
		"$TCA parent 1: classid 1:1 htb rate 10000000kbit\n"
		"\n"
		"$TQAU root handle 2: htb\n"
		"$TCAU parent 2: classid 2:1 htb rate 10000000kbit\n" ,
		qsched
		);

		// access router : mark 9
		// default : 10Gbps
		fprintf(f,
		"\n"
		"$TCA parent 1:1 classid 1:9 htb rate 10000000kbit ceil 10000000kbit prio 1\n"
		"$TQA parent 1:9 handle 9: $SFQ\n"
		"$TFA parent 1: prio 1 protocol ip handle 9 fw flowid 1:9\n"
		"\n"
		"$TCAU parent 2:1 classid 2:9 htb rate 10000000kbit ceil 10000000kbit prio 1\n"
		"$TQAU parent 2:9 handle 9: $SFQ\n"
		"$TFAU parent 2: prio 1 protocol ip handle 9 fw flowid 2:9\n"
		);
	}

	while (g) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if ((vstrsep(p, ">", &enable, &addr, &dlc, &upc, &prio)) != 5) continue;
		if (!strcmp(enable, "0")) continue;

		address_checker(&addr_type, addr, addr_new, sizeof(addr_new));
		class = atoi(prio) + 10;
		if (addr_type == TYPE_MAC)
		{
			sscanf(addr_new, "%02X:%02X:%02X:%02X:%02X:%02X",&s[0],&s[1],&s[2],&s[3],&s[4],&s[5]);
			fprintf(f,
				"\n"
				"$TCA parent 1:1 classid 1:%d htb rate %skbit ceil %skbit prio %s\n"
				"$TQA parent 1:%d handle %d: $SFQ\n"
				"$TFA parent 1: protocol ip prio %s u32 match u16 0x0800 0xFFFF at -2 match u32 0x%02X%02X%02X%02X 0xFFFFFFFF at -12 match u16 0x%02X%02X 0xFFFF at -14 flowid 1:%d"
				"\n"
				"$TCAU parent 2:1 classid 2:%d htb rate %skbit ceil %skbit prio %s\n"
				"$TQAU parent 2:%d handle %d: $SFQ\n"
				"$TFAU parent 2: prio %s protocol ip handle %d fw flowid 2:%d\n"
				, class, dlc, dlc, prio
				, class, class
				, prio, s[2], s[3], s[4], s[5], s[0], s[1], class
				, class, upc, upc, prio
				, class, class
				, prio, class, class
			);
		}
		else if (addr_type == TYPE_IP || addr_type == TYPE_IPRANGE)
		{
			fprintf(f,
				"\n"
				"$TCA parent 1:1 classid 1:%d htb rate %skbit ceil %skbit prio %s\n"
				"$TQA parent 1:%d handle %d: $SFQ\n"
				"$TFA parent 1: prio %s protocol ip handle %d fw flowid 1:%d\n"
				"\n"
				"$TCAU parent 2:1 classid 2:%d htb rate %skbit ceil %skbit prio %s\n"
				"$TQAU parent 2:%d handle %d: $SFQ\n"
				"$TFAU parent 2: prio %s protocol ip handle %d fw flowid 2:%d\n"
				, class, dlc, dlc, prio
				, class, class
				, prio, class, class
				, class, upc, upc, prio
				, class, class
				, prio, class, class
			);
		}
		else if (addr_type == TYPE_GUEST)
		{
			// setup guest network's bandwidth limiter
			char mssid_mark[4];
			char *wl_if = NULL;
			int i, j;

			if(sscanf(addr_new, "wl%d.%d", &i, &j) != 2){
				_dprintf("[BWLIT] %s: fail to strip i, j from wlx.x\n", __FUNCTION__);
			}

			snprintf(mssid_mark, sizeof(mssid_mark), "%d", class);

			if(get_model()==MODEL_RTAC87U && (i == 1)){
				if(j == 1) wl_if = "vlan4000";
				if(j == 2) wl_if = "vlan4001";
				if(j == 3) wl_if = "vlan4002";
			}
			else{
				wl_if = addr_new;
			}
			strcat(bw_iface, strcat(wl_if, " "));

			fprintf(f,
				"\n"
				"tc qdisc del dev %s root 2>/dev/null\n"
				"GUEST%d%d=%s\n"
				"TQA%d%d=\"tc qdisc add dev $GUEST%d%d\"\n"
				"TCA%d%d=\"tc class add dev $GUEST%d%d\"\n"
				"TFA%d%d=\"tc filter add dev $GUEST%d%d\"\n" // 5
				"\n"
				"$TQA%d%d root handle %d: htb\n"
				"$TCA%d%d parent %d: classid %d:1 htb rate %skbit\n" // 7
				"\n"
				"$TCA%d%d parent %d:1 classid %d:%d htb rate 1kbit ceil %skbit prio %d\n"
				"$TQA%d%d parent %d:%d handle %d: $SFQ\n"
				"$TFA%d%d parent %d: prio %d protocol ip handle %d fw flowid %d:%d\n" // 10
				"\n"
				"$TCAU parent 2:1 classid 2:%d htb rate 1kbit ceil %skbit prio %d\n"
				"$TQAU parent 2:%d handle %d: $SFQ\n"
				"$TFAU parent 2: prio %d protocol ip handle %d fw flowid 2:%d\n" // 13
				, wl_if
				, i, j, wl_if
				, i, j, i, j
				, i, j, i, j
				, i, j, i, j // 5
				, i, j, guest
				, i, j, guest, guest, dlc //7
				, i, j, guest, guest, class, dlc, class
				, i, j, guest, class, class
				, i, j, guest, class, class, guest, class // 10
				, class, upc, class
				, class, class
				, class, class, class //13
			);
			_dprintf("[BWLIT] %s: create %s bandwidth limiter, qdisc=%d, class=%d\n", __FUNCTION__, wl_if, guest, class);
			guest++; // add guest 3: ~ 12: (9 guestnetwork)
		}
	}
	free(buf);

	fprintf(f,
		";;\n"
		"stop)\n"
		"tc qdisc del dev $WAN root 2>/dev/null\n"
		"tc qdisc del dev $WAN ingress 2>/dev/null\n"
		"tc qdisc del dev br0 root 2>/dev/null\n"
		"tc qdisc del dev br0 ingress 2>/dev/null\n"
	);
	if(strlen(bw_iface) > 0){
		foreach(word, bw_iface, next) {
			fprintf(f, "tc qdisc del dev %s root 2>/dev/null\n"
				   "tc qdisc del dev %s ingress 2>/dev/null\n"
				   , word, word
			);
		}
	}
	fprintf(f,
		";;\n"
		"*)\n"
		"tc -s -d class ls dev $WAN\n"
		"tc -s -d class ls dev br0\n"
	);
	if(strlen(bw_iface) > 0){
                foreach(word, bw_iface, next) {
                        fprintf(f, "tc -s -d class ls dev %s\n"
				   , word
			);
                }
        }
	fprintf(f,
		"esac"
	);

	fclose(f);
	chmod(qosfn, 0700);
	eval((char *)qosfn, "start");
	logmessage("qos-init", "bwl start complete");
	_dprintf("[BWLIT] %s: create bandwidth limiter\n", __FUNCTION__);

	return 0;
}

int add_iQosRules(char *pcWANIF)
{
	int status = 0;
	if (pcWANIF == NULL || nvram_get_int("qos_enable") != 1 || nvram_get_int("qos_type") == 1) return -1;

	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0)
		status = add_qos_rules(pcWANIF);
	else if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 2)
		status = add_bandwidth_limiter_rules(pcWANIF);

	if (status < 0) _dprintf("[%s] status = %d\n", __FUNCTION__, status);

	return status;
}

int start_iQos(void)
{
	int status = 0;
	if (nvram_get_int("qos_enable") != 1 || nvram_get_int("qos_type") == 1) return -1;

	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0)
		status = start_tqos();
	else if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 2)
		status = start_bandwidth_limiter();

	if (status < 0) _dprintf("[%s] status = %d\n", __FUNCTION__, status);

	return status;
}

void reset_qos_stats(void)
{
	char cmd[16];

	//stop and restart tc setup
	sprintf(cmd, "%s %s", qosfn, "start");
	system(cmd);
	//run custom script
	run_custom_script("qos-start", "init");
}
