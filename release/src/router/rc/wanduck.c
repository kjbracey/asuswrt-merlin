/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <rc.h>
#include <rtconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#ifdef RTCONFIG_USB_MODEM
#include <usb_info.h>
#endif

#include "wanduck.h"

#define NO_DETECT_INTERNET
#define NO_IOS_DETECT_INTERNET
#ifdef RTCONFIG_LAN4WAN_LED
int LanWanLedCtrl(void);
#endif

static void safe_leave(int signo){
	if(wanduck_debug & 1)
		_dprintf("\n## wanduck.safeexit ##\n");
	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	FD_ZERO(&allset);
	close(http_sock);
	close(dns_sock);

	int i, ret;
	for(i = 0; i < maxfd; ++i){
		ret = close(i);
		if(wanduck_debug & 1)
			_dprintf("## close %d: result=%d.\n", i, ret);
	}

#ifndef RTCONFIG_BCMARM
	sleep(1);
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER){
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "filter", "-F");
		f_write_string("/proc/net/dnsmqctrl", "", 0, 0);
	}
#endif

	if(rule_setup == 1){
		int len;
		char *fn = NAT_RULES, ln[PATH_MAX];
		struct stat s;

		if(wanduck_debug & 1)
			_dprintf("\n# Disable direct rule(exit wanduck)\n");

		rule_setup = 0;
		conn_changed_state[current_wan_unit] = CONNED; // for cleaning the redirect rules.

#if 0
		// in the function safe_leave(), couldn't set any nvram using nvram_set().
		// So the notify mechanism would be invalid.
#if 0
		handle_wan_line(current_wan_unit, rule_setup);
#else // or
		start_nat_rules();
#endif
#else
		// Couldn't directly use nvram_set().
		// Must use the command: nvram, and set the nat_state.
		char buf[16];
		FILE *fp = fopen(NAT_RULES, "r");

		memset(buf, 0, 16);
		if(fp != NULL){
			fclose(fp);

			if (!lstat(NAT_RULES, &s) && S_ISLNK(s.st_mode) &&
			    (len = readlink(NAT_RULES, ln, sizeof(ln))) > 0) {
				ln[len] = '\0';
				fn = ln;
			}

			sprintf(buf, "nat_state=%d", NAT_STATE_NORMAL);
			eval("nvram", "set", buf);

			if(wanduck_debug & 1)
				_dprintf("%s: apply the nat_rules(%s): %s!\n", __FUNCTION__, fn, buf);
			logmessage("wanduck exit", "apply the nat_rules(%s)!", fn);

			setup_ct_timeout(TRUE);
			setup_udp_timeout(TRUE);

			eval("iptables-restore", NAT_RULES);
		}
		else{
			sprintf(buf, "nat_state=%d", NAT_STATE_INITIALIZING);
			eval("nvram", "set", buf);

			if(wanduck_debug & 1)
				_dprintf("%s: initial the nat_rules: %s!\n", __FUNCTION__, buf);
			logmessage("wanduck exit", "initial the nat_rules!");
		}
#endif
	}

	remove(WANDUCK_PID_FILE);

	if(wanduck_debug & 1)
		_dprintf("\n# return(exit wanduck)\n");
	exit(0);
}

void get_related_nvram(){
	sw_mode = nvram_get_int("sw_mode");
	wanduck_debug = nvram_get_int("wanduck_debug");

	if(nvram_match("x_Setting", "1"))
		isFirstUse = 0;
	else
		isFirstUse = 1;

#ifdef RTCONFIG_WIRELESSREPEATER
	if(strlen(nvram_safe_get("wlc_ssid")) > 0)
		setAP = 1;
	else
		setAP = 0;
#endif

#ifdef RTCONFIG_DUALWAN
	memset(dualwan_mode, 0, 8);
	snprintf(dualwan_mode, sizeof(dualwan_mode), "%s", nvram_safe_get("wans_mode"));
	snprintf(dualwan_wans, sizeof(dualwan_wans), "%s", nvram_safe_get("wans_dualwan"));
	wans_dualwan = (strstr(nvram_safe_get("wans_dualwan"), "none") != NULL) ? 0 : 1;

	memset(wandog_target, 0, PATH_MAX);
	if(sw_mode == SW_MODE_ROUTER){
		wandog_enable = nvram_get_int("wandog_enable");
		scan_interval = nvram_get_int("wandog_interval");
		max_disconn_count = nvram_get_int("wandog_maxfail");
		wandog_delay = nvram_get_int("wandog_delay");

		if((!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
				&& wandog_enable == 1
				){
			strcpy(wandog_target, nvram_safe_get("wandog_target"));
		}

		if(!strcmp(dualwan_mode, "fb")){
			max_fb_count = nvram_get_int("wandog_fb_count");
			max_fb_wait_time = scan_interval*max_fb_count;
		}
	}
	else{
		wandog_enable = 0;
		scan_interval = DEFAULT_SCAN_INTERVAL;
		max_disconn_count = DEFAULT_MAX_DISCONN_COUNT;
		wandog_delay = -1;
	}
#else
	wandog_enable = 0;
	scan_interval = DEFAULT_SCAN_INTERVAL;
	max_disconn_count = DEFAULT_MAX_DISCONN_COUNT;
#endif
	max_wait_time = scan_interval*max_disconn_count;

#ifdef RTCONFIG_DUALWAN
	if((wanduck_debug & 2) && wans_dualwan == 1){
		_dprintf("# wanduck: Got dualwan information:\n", current_lan_unit);
		_dprintf("# wanduck:       wans_dualwan=%d.\n", wans_dualwan);
		_dprintf("# wanduck:      wandog_enable=%d.\n", wandog_enable);
		_dprintf("# wanduck:      wandog_target=%s.\n", wandog_target);
		_dprintf("# wanduck:  max_disconn_count=%d.\n", max_disconn_count);
		_dprintf("# wanduck:       wandog_delay=%d.\n", wandog_delay);
		_dprintf("# wanduck:      scan_interval=%d.\n", scan_interval);
		_dprintf("# wanduck:      max_wait_time=%d.\n", max_wait_time);
	}
#endif

}

void get_lan_nvram(){
	char nvram_name[16];

	current_lan_unit = nvram_get_int("lan_unit");

	memset(prefix_lan, 0, 8);
	if(current_lan_unit < 0)
		strcpy(prefix_lan, "lan_");
	else
		sprintf(prefix_lan, "lan%d_", current_lan_unit);

	memset(current_lan_ifname, 0, 16);
	strcpy(current_lan_ifname, nvram_safe_get(strcat_r(prefix_lan, "ifname", nvram_name)));

	memset(current_lan_proto, 0, 16);
	strcpy(current_lan_proto, nvram_safe_get(strcat_r(prefix_lan, "proto", nvram_name)));

	memset(current_lan_ipaddr, 0, 16);
	strcpy(current_lan_ipaddr, nvram_safe_get(strcat_r(prefix_lan, "ipaddr", nvram_name)));

	memset(current_lan_netmask, 0, 16);
	strcpy(current_lan_netmask, nvram_safe_get(strcat_r(prefix_lan, "netmask", nvram_name)));

	memset(current_lan_gateway, 0, 16);
	strcpy(current_lan_gateway, nvram_safe_get(strcat_r(prefix_lan, "gateway", nvram_name)));

	memset(current_lan_dns, 0, 256);
	strcpy(current_lan_dns, nvram_safe_get(strcat_r(prefix_lan, "dns", nvram_name)));

	memset(current_lan_subnet, 0, 11);
	strcpy(current_lan_subnet, nvram_safe_get(strcat_r(prefix_lan, "subnet", nvram_name)));

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER){
		wlc_state = nvram_get_int("wlc_state");
		got_notify = 1;
	}
#endif

	if(wanduck_debug & 1)
		_dprintf("# wanduck: Got LAN(%d) information:\n", current_lan_unit);
	if(wanduck_debug & 2){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER){
			_dprintf("# wanduck:   ipaddr=%s.\n", current_lan_ipaddr);
			_dprintf("# wanduck:wlc_state=%d.\n", wlc_state);
		}
		else
#endif
		{
			_dprintf("# wanduck:   ifname=%s.\n", current_lan_ifname);
			_dprintf("# wanduck:    proto=%s.\n", current_lan_proto);
			_dprintf("# wanduck:   ipaddr=%s.\n", current_lan_ipaddr);
			_dprintf("# wanduck:  netmask=%s.\n", current_lan_netmask);
			_dprintf("# wanduck:  gateway=%s.\n", current_lan_gateway);
			_dprintf("# wanduck:      dns=%s.\n", current_lan_dns);
			_dprintf("# wanduck:   subnet=%s.\n", current_lan_subnet);
		}
	}
}

static void get_network_nvram(int signo){
	if(sw_mode == SW_MODE_AP)
		get_lan_nvram();
#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER)
		get_lan_nvram();
	else if(sw_mode == SW_MODE_HOTSPOT)
		wlc_state = nvram_get_int("wlc_state");
#endif
}

/* 67u,87u,3200: have each led on every port.
 * 88u,3100,5300: have one led to hint wan port but this led is the union of all ports
 * force led_on on usb modem case */

void enable_wan_led()
{
	int usb_wan = get_dualwan_by_unit(wan_primary_ifunit()) == WANS_DUALWAN_IF_USB ? 1:0;

	if(usb_wan) {
		switch (get_model()) {
#ifdef RTAC68U
			case MODEL_RTAC68U:
				if (!is_ac66u_v2_series())
					break;
#endif
			case MODEL_RTAC87U:
				eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01ff");
				eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01fe");
				break;
		}
	}

	eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01ff");
	eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01ff");
}

void disable_wan_led()
{
	eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01fe");
	eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01fe");
}

static void wan_led_control(int sig) {
#if 0
#if defined(RTAC68U) || defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300) || defined(RTAC5300R)
#ifdef RTAC68U
	if (!is_ac66u_v2_series())
		return;
#endif
	char buf[16];
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get("wans_dualwan"));
	if (strcmp(buf, "wan none") != 0){
		logmessage("DualWAN", "skip single wan wan_led_control()");
		return;
	}
#endif
#endif
#ifdef RTCONFIG_USB_MODEM
	int unit;

	if ((unit = get_usbif_dualwan_unit()) >= 0)
		link_wan[unit] = is_usb_modem_ready();
	else
		if(wanduck_debug & 1)
			_dprintf("# wanduck: nvram changed: Don't enable the USB line!\n");

	if(wanduck_debug & 1)
		_dprintf("# wanduck: nvram changed: x_Setting=%d, link_modem=%d.\n", !isFirstUse, link_wan[unit]);
#endif
#if defined(RTAC68U) ||  defined(RTAC87U) || defined(DSL_AC68U)
	if(nvram_match("AllLED", "1")
#ifdef RTAC68U
		&& is_ac66u_v2_series()
#endif
	) {
#if defined(RTAC68U) || defined(RTAC87U)
		if (rule_setup) {
			led_control(LED_WAN, LED_ON);
			disable_wan_led();
		} else {
			led_control(LED_WAN, LED_OFF);
			enable_wan_led();
		}
#elif defined(DSL_AC68U)
		if (rule_setup) {
			led_control(LED_WAN, LED_OFF);
		} else {
			led_control(LED_WAN, LED_ON);
		}
#endif
	}
#endif
}

int get_packets_of_net_dev(const char *net_dev, unsigned long *rx_packets, unsigned long *tx_packets){
	FILE *fp;
	char buf[256];
	char *ifname;
	char *ptr;
	int i, got_packets = 0;

	if((fp = fopen(PROC_NET_DEV, "r")) == NULL){
		if(wanduck_debug & 1)
			_dprintf("get_packets_of_net_dev: Can't open the file: %s.\n", PROC_NET_DEV);
		return got_packets;
	}

	// headers.
	for(i = 0; i < 2; ++i){
		if(fgets(buf, sizeof(buf), fp) == NULL){
			if(wanduck_debug & 1)
				_dprintf("get_packets_of_net_dev: Can't read out the headers of %s.\n", PROC_NET_DEV);
			fclose(fp);
			return got_packets;
		}
	}

	while(fgets(buf, sizeof(buf), fp) != NULL){
		if((ptr = strchr(buf, ':')) == NULL)
			continue;

		*ptr = 0;
		if((ifname = strrchr(buf, ' ')) == NULL)
			ifname = buf;
		else
			++ifname;

		if(strcmp(ifname, net_dev))
			continue;

		// <rx bytes, packets, errors, dropped, fifo errors, frame errors, compressed, multicast><tx ...>
		if(sscanf(ptr+1, "%*u%lu%*u%*u%*u%*u%*u%*u%*u%lu", rx_packets, tx_packets) != 2){
			if(wanduck_debug & 1)
				_dprintf("get_packets_of_net_dev: Can't read the packet's number in %s.\n", PROC_NET_DEV);
			fclose(fp);
			return got_packets;
		}

		got_packets = 1;
		break;
	}
	fclose(fp);

	return got_packets;
}

char *organize_tcpcheck_cmd(char *dns_list, char *cmd, int size){
	char buf[256], *next;
	int port;

	if(cmd == NULL || size <= 0)
		return NULL;

	memset(cmd, 0, size);

	sprintf(cmd, "/sbin/tcpcheck %d", TCPCHECK_TIMEOUT);

	foreach(buf, dns_list, next){
#if defined(RTCONFIG_DNSCRYPT)
		port = (nvram_match("dnscrypt_proxy", "1") ? nvram_get_int("dnscrypt1_port") : 53);
#elif defined(RTCONFIG_STUBBY)
		port = (nvram_match("stubby_proxy", "1") ? nvram_get_int("stubby_port") : 53);
#else
		port = 53;
#endif
		sprintf(cmd, "%s %s:%d", cmd, buf, port);
	}

	sprintf(cmd, "%s >>%s", cmd, DETECT_FILE);

	return cmd;
}

int do_ping_detect(int wan_unit){
#ifdef RTCONFIG_DUALWAN
	char cmd[256];
	char prefix_wan[8], wan_iface[32], nvram_name[32];
	static int last_status = 1;

	/* Check for valid domain to avoid shell escaping */
	if (!is_valid_domainname(wandog_target)) {
		if(wanduck_debug & 1)
			_dprintf("wanduck: %s %s...invalid target\n", __FUNCTION__, wandog_target);
		return -1;
	}

	/* Get wan interface */
	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);
	memset(wan_iface, 0, 32);
	strcpy(wan_iface, nvram_safe_get(strcat_r(prefix_wan, "ifname", nvram_name)));
	if (!strcmp(wan_iface, "\0"))
		return 0;

	sprintf(cmd, "ping -c 1 -W 4 -s 32 %s -I %s %s >/dev/null && touch %s", nvram_get_int("ttl_spoof_enable") ? "" : "-t128", wan_iface, wandog_target, PING_RESULT_FILE);
	system(cmd);

	if(check_if_file_exist(PING_RESULT_FILE)){
		if(wanduck_debug & 1)
			_dprintf("wanduck: %s %s from WAN(%d) %s...ok\n", __FUNCTION__, wandog_target, wan_unit, wan_iface);
		unlink(PING_RESULT_FILE);
		last_status = 1;
		return 1;
	}
#else
	return 1;
#endif
	if(wanduck_debug & 1)
		_dprintf("wanduck: %s %s from WAN(%d) %s...failed\n", __FUNCTION__, wandog_target, wan_unit, wan_iface);
	if (last_status == 1) {
		last_status = 0;
		return 1;
	}
	else
		return 0;
}

int do_dns_detect(){
	char *test_url = "www.asus.com www.google.com www.baidu.com www.yandex.com";
	char word[64], *next;

	foreach(word, test_url, next){
		if(gethostbyname(word) != NULL){
			if(wanduck_debug & 1)
				_dprintf("wanduck: %s checking %s...success.\n", __FUNCTION__, word);
			nvram_set_int("link_internet", 1);
			return 1;
		}
		if(wanduck_debug & 1)
			_dprintf("wanduck: %s checking %s...failed.\n", __FUNCTION__, word);
	}

	return 0;
}

int do_tcp_dns_detect(int wan_unit){
	FILE *fp = NULL;
	char line[80], cmd[PATH_MAX];
	char prefix_wan[8], nvram_name[16], wan_dns[256];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_dns, 0, 256);
#if defined(RTCONFIG_DNSCRYPT)
	if (nvram_match("dnscrypt_proxy", "1")) {
		if (nvram_match("dnscrypt1_ipv6", "0"))
			strcpy(wan_dns, "127.0.0.1");
		else
			return 1;  //if can't test, assume up
	}
	else
#elif defined(RTCONFIG_STUBBY)
	if (nvram_match("stubby_proxy", "1")) {
		if (nvram_get_int("stubby_ipv4"))
			strcpy(wan_dns, "127.0.0.1");
		else
			return 1;  //if can't test, assume up
	}
	else
#endif
		strcpy(wan_dns, nvram_safe_get(strcat_r(prefix_wan, "dns", nvram_name)));

	remove(DETECT_FILE);

	if(organize_tcpcheck_cmd(wan_dns, cmd, PATH_MAX) == NULL){
		if(wanduck_debug & 1)
			_dprintf("wanduck: No tcpcheck cmd.\n");
		return 0;
	}
	system(cmd);

	if((fp = fopen(DETECT_FILE, "r")) == NULL){
		if(wanduck_debug & 1)
			_dprintf("wanduck: No file: %s.\n", DETECT_FILE);
		return 0;
	}

	while(fgets(line, sizeof(line), fp) != NULL){
		if(strstr(line, "alive")){
			if(wanduck_debug & 1)
				_dprintf("wanduck: %s WAN(%d) %s", __FUNCTION__, wan_unit, line);
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);

	return 0;
}

int detect_internet(int wan_unit){
	int link_internet;
#ifndef NO_DETECT_INTERNET
	unsigned long rx_packets, tx_packets;
#endif
	char prefix_wan[8], wan_ifname[16];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_ifname, 0, 16);
	strcpy(wan_ifname, get_wan_ifname(wan_unit));

#ifdef RTCONFIG_DUALWAN
	if(wan_unit != current_wan_unit && wans_dualwan == 1){
		if(wanduck_debug & 1)
			_dprintf("wanduck: %s current WAN unit WAN(%d), checking WAN(%d)\n", __FUNCTION__, current_wan_unit, wan_unit);
	}
	else
#endif
		if(wanduck_debug & 1)
			_dprintf("wanduck: %s checking WAN(%d)\n", __FUNCTION__, wan_unit);

	if(wanduck_debug & 2) {
		do_dns_detect();
		do_tcp_dns_detect(wan_unit);
		do_ping_detect(wan_unit);
	}

	if(
#ifdef RTCONFIG_DUALWAN
			strcmp(dualwan_mode, "lb") &&
#endif
			!found_default_route(wan_unit)
			)
		link_internet = DISCONN;
#ifndef NO_DETECT_INTERNET
	else if(!get_packets_of_net_dev(wan_ifname, &rx_packets, &tx_packets) || rx_packets <= RX_THRESHOLD) {
		link_internet = DISCONN;
		if(wanduck_debug & 1)
			_dprintf("wanduck: disconnect by packet transfer check.\n");
	}
	else if(!isFirstUse && (!do_dns_detect() && !do_tcp_dns_detect(wan_unit) && !do_ping_detect(wan_unit))) {
		link_internet = DISCONN;
		if(wanduck_debug & 1)
			_dprintf("wanduck: disconnect by dns/ping check.\n");
	}
#endif
#ifdef RTCONFIG_DUALWAN
	else if((!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb")) && ((wans_dualwan == 1 && wandog_enable == 1) || nvram_match("ping_debug", "1"))
			&& !isFirstUse && !do_ping_detect(wan_unit)) {
		link_internet = DISCONN;
		if(wanduck_debug & 1)
			_dprintf("wanduck: %sWAN(%d) disconnect by wandog.\n", (wans_dualwan == 1 ? "dualwan " : ""), wan_unit);
	}
#endif
	else
		link_internet = CONNED;

	if(link_internet == DISCONN){
		nvram_set_int("link_internet", 0);
		record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NO_INTERNET_ACTIVITY);

		if(!(nvram_get_int("web_redirect") & WEBREDIRECT_FLAG_NOINTERNET)) {
			nvram_set_int("link_internet", 1);
			link_internet = CONNED;
		}
	}
	else{
		nvram_set_int("link_internet", 1);
		record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NONE);
	}

	if(wanduck_debug & 1)
		_dprintf("wanduck: link_internet=%d.\n", link_internet);
	return link_internet;
}

int passivesock(char *service, int protocol_num, int qlen){
	//struct servent *pse;
	struct sockaddr_in sin;
	int s, type, on;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	// map service name to port number
	if((sin.sin_port = htons((u_short)atoi(service))) == 0){
		perror("cannot get service entry");

		return -1;
	}

	if(protocol_num == IPPROTO_UDP)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

	s = socket(PF_INET, type, protocol_num);
	if(s < 0){
		perror("cannot create socket");

		return -1;
	}

	on = 1;
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0){
		perror("cannot set socket's option: SO_REUSEADDR");
		close(s);

		return -1;
	}

	if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		perror("cannot bind port");
		close(s);

		return -1;
	}

	if(type == SOCK_STREAM && listen(s, qlen) < 0){
		perror("cannot listen to port");
		close(s);

		return -1;
	}

	return s;
}

int check_ppp_exist(){
	DIR *dir;
	struct dirent *dent;
	char task_file[64], cmdline[64];
	int pid, fd;

	if((dir = opendir("/proc")) == NULL){
		perror("open proc");
		return 0;
	}

	while((dent = readdir(dir)) != NULL){
		if((pid = atoi(dent->d_name)) > 1){
			memset(task_file, 0, 64);
			sprintf(task_file, "/proc/%d/cmdline", pid);
			if((fd = open(task_file, O_RDONLY)) > 0){
				memset(cmdline, 0, 64);
				read(fd, cmdline, 64);
				close(fd);

				if(strstr(cmdline, "pppd")
						|| strstr(cmdline, "l2tpd")
						){
					closedir(dir);
					return 1;
				}
			}
			else
				printf("cannot open %s\n", task_file);
		}
	}
	closedir(dir);

	return 0;
}

int chk_proto(int wan_unit, int wan_state){
	int wan_sbstate = nvram_get_int(nvram_sbstate[wan_unit]);
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_proto, 0, 16);
	strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_HOTSPOT){
		if(wan_state == WAN_STATE_STOPPED) {
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
	}
	else
#endif
	// Start chk_proto() in SW_MODE_ROUTER.
#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(wan_unit)) {
		ppp_fail_state = pppstatus();

		if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_PPP_AUTH_FAIL);

			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_STOPPED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
	}
	else
#endif
	if(!strcmp(wan_proto, "dhcp")
			|| !strcmp(wan_proto, "static")){
		if(wan_state == WAN_STATE_STOPPED) {
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
	}
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			){
		ppp_fail_state = pppstatus();

		if(wan_state != WAN_STATE_CONNECTED
				&& ppp_fail_state == WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			// PPP is into the idle mode.
			if(wan_state == WAN_STATE_STOPPED) // Sometimes ip_down() didn't set it.
				record_wan_state_nvram(wan_unit, -1, -1, WAN_STOPPED_REASON_PPP_LACK_ACTIVITY);
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_PPP_AUTH_FAIL);

			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_STOPPED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
	}

	return CONNED;
}

int if_wan_phyconnected(int wan_unit){
	char *wired_link_nvram;
#ifdef RTCONFIG_WIRELESSREPEATER
	int link_ap = 0;
#endif
	int link_changed = 0;
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	if(wan_unit)
		wired_link_nvram = "link_wan1";
	else
		wired_link_nvram = "link_wan";

#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(wan_unit)) {
		if(link_wan[wan_unit] != nvram_get_int(wired_link_nvram)){
			nvram_set_int(wired_link_nvram, link_wan[wan_unit]);

			if(link_wan[wan_unit] == 2)
				logmessage("wanduck", "The local subnet is the same with the USB ethernet.");
			else if(strcmp(nvram_safe_get("usb_modem_act_type"), "ncm"))
				link_changed = 1;
		}

		if(link_wan[wan_unit] == 2)
			return SET_ETH_MODEM;
	}
	else
#endif
	if (dualwan_unit__nonusbif(wan_unit)) {
		memset(prefix_wan, 0, 8);
		sprintf(prefix_wan, "wan%d_", wan_unit);

		memset(wan_proto, 0, 16);
		strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

		// check wan port.
		link_wan[wan_unit] = get_wanports_status(wan_unit);

#ifdef RTCONFIG_LANWAN_LED
		if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_WAN){
			if(link_wan[wan_unit]) led_control(LED_WAN, LED_ON);
			else led_control(LED_WAN, LED_OFF);
		}
#endif

		if(link_wan[wan_unit] != nvram_get_int(wired_link_nvram)){
			if(link_wan[wan_unit])
				nvram_set_int(wired_link_nvram, CONNED);
			else
				nvram_set_int(wired_link_nvram, DISCONN);

			link_changed = 1;
		}
	}

#ifdef RTCONFIG_LANWAN_LED
	if(get_lanports_status()) led_control(LED_LAN, LED_ON);
	else led_control(LED_LAN, LED_OFF);
#endif

#ifdef RTCONFIG_LAN4WAN_LED
	LanWanLedCtrl();
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	// check if set AP.
	if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT){
		link_ap = (wlc_state == WLC_STATE_CONNECTED);
		if(link_ap != nvram_get_int("link_ap"))
			nvram_set_int("link_ap", link_ap);
	}
#endif

	if(sw_mode == SW_MODE_ROUTER){
		// this means D2C because of reconnecting the WAN port.
		if (link_changed) {
#ifdef RTCONFIG_USB_MODEM
			if(dualwan_unit__usbif(wan_unit) && link_wan[wan_unit]){
				return PHY_RECONN;
			}
			else
#endif
			// WAN port was disconnected, arm reconnect
			if(!link_setup[wan_unit] && !link_wan[wan_unit]){
				link_setup[wan_unit] = 1;
			} else
			// WAN port was connected, fire reconnect if armed
			if (link_setup[wan_unit]) {
				link_setup[wan_unit] = 0;
				// Only handle this case when WAN proto is DHCP or Static
				if (strcmp(wan_proto, "static") == 0) {
					disconn_case[wan_unit] = CASE_DISWAN;
					return PHY_RECONN;
				} else
				if (strcmp(wan_proto, "dhcp") == 0) {
					disconn_case[wan_unit] = CASE_DHCPFAIL;
					return PHY_RECONN;
				}
			}
		}
		else if(!link_wan[wan_unit]){
#ifdef RTCONFIG_DUALWAN
			if(strcmp(dualwan_mode, "lb"))
#endif
				nvram_set_int("link_internet", 0);

			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case[wan_unit] = CASE_DISWAN;
			}

			return DISCONN;
		}
	}
	else if(sw_mode == SW_MODE_AP){
#if 0
		if(!link_wan[wan_unit]){
			// ?: type error?
			nvram_set_int("link_internet", 0);

			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case[wan_unit] = CASE_DISWAN;
				return DISCONN;
			}
		}
#else
		if(nvram_get_int("link_internet") != 1)
			nvram_set_int("link_internet", 1);

		return CONNED;
#endif
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else{ // sw_mode == SW_MODE_REPEATER, SW_MODE_HOTSPOT.
		if(!link_ap){
			if(nvram_get_int("link_internet") != 0)
				nvram_set_int("link_internet", 0);

			if(sw_mode == SW_MODE_HOTSPOT)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			disconn_case[wan_unit] = CASE_AP_DISCONN;
			return DISCONN;
		}
		else if(sw_mode == SW_MODE_REPEATER){
			if(nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED){
				if(nvram_get_int("link_internet") != 0)
					nvram_set_int("link_internet", 0);

				return DISCONN;
			}
			else{
				if(nvram_get_int("link_internet") != 1)
					nvram_set_int("link_internet", 1);

				return CONNED;
			}
		}
	}
#endif

	return CONNED;
}

int if_wan_connected(int wan_unit, int wan_state){
	if(chk_proto(wan_unit, wan_state) != CONNED)
		return DISCONN;
	else if(sw_mode == SW_MODE_ROUTER) // TODO: temparily let detect_internet() service in SW_MODE_ROUTER.
		return detect_internet(wan_unit);

	return CONNED;
}

void handle_wan_line(int wan_unit, int action){
	char cmd[32];
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	// Redirect rules.
	if(action){
		stop_nat_rules();
	}
	/*
	 * When C2C and remove the redirect rules,
	 * it means dissolve the default state.
	 */
	else if(conn_changed_state[wan_unit] == D2C || conn_changed_state[wan_unit] == CONNED){
//		start_nat_rules();
		start_firewall(wan_unit, 0);  //must restart firewall to handle vpn and dnscrypt/stubby

		memset(prefix_wan, 0, 8);
		sprintf(prefix_wan, "wan%d_", wan_unit);

		memset(wan_proto, 0, 16);
		strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

		if(!strcmp(wan_proto, "static")){
			/* Sync time */
			refresh_ntpc();
		}

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		if(check_if_dir_exist("/opt/lib/ipkg")){
			if(wanduck_debug & 1)
				_dprintf("wanduck: update the APP's lists...\n");
			notify_rc("start_apps_update");
		}
#endif
	}
	else{ // conn_changed_state[wan_unit] == PHY_RECONN
		if(wanduck_debug & 1)
			_dprintf("\n# wanduck(%d): Try to restart_wan_if.\n", action);
		memset(cmd, 0, 32);
		sprintf(cmd, "restart_wan_if %d", wan_unit);
		notify_rc_and_wait(cmd);
	}
}

void close_socket(int sockfd, char type){
	close(sockfd);
	FD_CLR(sockfd, &allset);
	client[fd_i].sfd = -1;
	client[fd_i].type = 0;
}

int build_socket(char *http_port, char *dns_port, int *hd, int *dd){
	if((*hd = passivesock(http_port, IPPROTO_TCP, 10)) < 0){
		if(wanduck_debug & 1)
			_dprintf("Fail to socket for httpd port: %s.\n", http_port);
		return -1;
	}

	if((*dd = passivesock(dns_port, IPPROTO_UDP, 10)) < 0){
		if(wanduck_debug & 1)
			_dprintf("Fail to socket for DNS port: %s.\n", dns_port);
		return -1;
	}

	return 0;
}

void send_page(int wan_unit, int sfd, char *file_dest, char *url){
	char buf[2*MAXLINE];
	time_t now;
	char timebuf[100];
	char dut_addr[64];

	memset(buf, 0, sizeof(buf));
	now = uptime();
	(void)strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

#ifdef NO_IOS_DETECT_INTERNET
	// disable iOS popup window. Jerry5 2012.11.27
	if (!strcmp(url,"www.apple.com/library/test/success.html") && nvram_get_int("disiosdet") == 1){
		sprintf(buf, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", buf, "HTTP/1.0 200 OK\r\n", "Server: Apache/2.2.3 (Oracle)\r\n", "Content-Type: text/html; charset=UTF-8\r\n", "Cache-Control: max-age=557\r\n","Expires: ", timebuf, "\r\n", "Date: ", timebuf, "\r\n", "Content-Length: 127\r\n", "Connection: close\r\n\r\n","<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n","<HTML>\n","<HEAD>\n\t","<TITLE>Success</TITLE>\n","</HEAD>\n","<BODY>\n","Success\n","</BODY>\n","</HTML>\n");
	}
	else{
#endif

	sprintf(buf, "%s%s%s%s%s%s", buf, "HTTP/1.0 302 Moved Temporarily\r\n", "Server: wanduck\r\n", "Date: ", timebuf, "\r\n");

	memset(dut_addr, 0, 64);

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
		strcpy(dut_addr, DUT_DOMAIN_NAME);
	else
#endif
	if(isFirstUse)
		strcpy(dut_addr, DUT_DOMAIN_NAME);
	else
		strcpy(dut_addr, nvram_safe_get("lan_ipaddr"));

	// TODO: Only send pages for the wan(0)'s state.
#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(wan_unit)) {
		if(conn_changed_state[wan_unit] == SET_ETH_MODEM)
			sprintf(buf, "%s%s%s%s%s%s%d%s%d%s%s" ,buf , "Connection: close\r\n", "Location:", (nvram_get_int("login_port")==nvram_get_int("https_lanport") ? "https://" : "http://"), dut_addr, ":", (nvram_get_int("login_port") ? : nvram_get_int("http_lanport")), "/error_page.htm?flag=", CASE_THESAMESUBNET, "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
		else{
			close_socket(sfd, T_HTTP);
			return;
		}
	}
	else
#endif
	if((conn_changed_state[wan_unit] == C2D || conn_changed_state[wan_unit] == DISCONN) && disconn_case[wan_unit] == CASE_THESAMESUBNET)
		sprintf(buf, "%s%s%s%s%s%s%d%s%d%s%s" ,buf , "Connection: close\r\n", "Location:", (nvram_get_int("login_port")==nvram_get_int("https_lanport") ? "https://" : ((nvram_get_int("http_enable")+1)&1 ? "http://" : "https://")), dut_addr, ":", (nvram_get_int("login_port") ? : nvram_get_int("http_lanport")), "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	else if(isFirstUse){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
			sprintf(buf, "%s%s%s%s%s%s%d%s%s%s" ,buf , "Connection: close\r\n", "Location:", (nvram_get_int("login_port")==nvram_get_int("https_lanport") ? "https://" : ((nvram_get_int("http_enable")+1)&1 ? "http://" : "https://")), dut_addr, ":", (nvram_get_int("login_port") ? : nvram_get_int("http_lanport")), "/QIS_wizard.htm?flag=sitesurvey", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
		else
#endif
			sprintf(buf, "%s%s%s%s%s%s%d%s%s%s" ,buf , "Connection: close\r\n", "Location:", (nvram_get_int("login_port")==nvram_get_int("https_lanport") ? "https://" : ((nvram_get_int("http_enable")+1)&1 ? "http://" : "https://")), dut_addr, ":", (nvram_get_int("login_port") ? : nvram_get_int("http_lanport")), "/QIS_wizard.htm?flag=welcome", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	}
	else if(conn_changed_state[wan_unit] == C2D || conn_changed_state[wan_unit] == DISCONN)
		sprintf(buf, "%s%s%s%s%s%s%d%s%d%s%s" ,buf , "Connection: close\r\n", "Location:", (nvram_get_int("login_port")==nvram_get_int("https_lanport") ? "https://" : ((nvram_get_int("http_enable")+1)&1 ? "http://" : "https://")), dut_addr, ":", (nvram_get_int("login_port") ? : nvram_get_int("http_lanport")), "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
#ifdef RTCONFIG_WIRELESSREPEATER
	else
		sprintf(buf, "%s%s%s%s%s%s%d%s", buf, "Connection: close\r\n", "Location:", (nvram_get_int("login_port")==nvram_get_int("https_lanport") ? "https://" : ((nvram_get_int("http_enable")+1)&1 ? "http://" : "https://")), dut_addr, ":", (nvram_get_int("login_port") ? : nvram_get_int("http_lanport")), "/index.asp\r\nContent-Type: text/plain\r\n\r\n<html></html>\r\n");
#endif

#ifdef NO_IOS_DETECT_INTERNET
	}
#endif
	write(sfd, buf, strlen(buf));
	close_socket(sfd, T_HTTP);
}

void parse_dst_url(char *page_src){
	int i, j;
	char dest[PATHLEN], host[64];
	char host_strtitle[7], *hp;

	j = 0;
	memset(dest, 0, sizeof(dest));
	memset(host, 0, sizeof(host));
	memset(host_strtitle, 0, sizeof(host_strtitle));

	for(i = 0; i < strlen(page_src); ++i){
		if(i >= PATHLEN)
			break;

		if(page_src[i] == ' ' || page_src[i] == '?'){
			dest[j] = '\0';
			break;
		}

		dest[j++] = page_src[i];
	}

	host_strtitle[0] = '\n';
	host_strtitle[1] = 'H';
	host_strtitle[2] = 'o';
	host_strtitle[3] = 's';
	host_strtitle[4] = 't';
	host_strtitle[5] = ':';
	host_strtitle[6] = ' ';

	if((hp = strstr(page_src, host_strtitle)) != NULL){
		hp += 7;
		j = 0;
		for(i = 0; i < strlen(hp); ++i){
			if(i >= 64)
				break;

			if(hp[i] == '\r' || hp[i] == '\n'){
				host[j] = '\0';
				break;
			}

			host[j++] = hp[i];
		}
	}

	memset(dst_url, 0, sizeof(dst_url));
	sprintf(dst_url, "%s/%s", host, dest);
}

void handle_http_req(int sfd, char *line){
	int len;

	if(!strncmp(line, "GET /", 5)){
		parse_dst_url(line+5);

		len = strlen(dst_url);
		if((dst_url[len-4] == '.') &&
				(dst_url[len-3] == 'i') &&
				(dst_url[len-2] == 'c') &&
				(dst_url[len-1] == 'o')){
			close_socket(sfd, T_HTTP);

			return;
		}

		send_page(current_wan_unit, sfd, NULL, dst_url);
	}
	else
		close_socket(sfd, T_HTTP);
}

void handle_dns_req(int sfd, unsigned char *request, int maxlen, struct sockaddr *pcliaddr, int clen){
#if !defined(RTCONFIG_FINDASUS)
	const static unsigned char auth_name_srv[] = {
		0x00, 0x00, 0x06, 0x00, 0x01, 0x00,
		0x00, 0x2a, 0x30, 0x00, 0x40, 0x01, 0x61, 0x0c,
		0x72, 0x6f, 0x6f, 0x74, 0x2d, 0x73, 0x65, 0x72,
		0x76, 0x65, 0x72, 0x73, 0x03, 0x6e, 0x65, 0x74,
		0x00, 0x05, 0x6e, 0x73, 0x74, 0x6c, 0x64, 0x0c,
		0x76, 0x65, 0x72, 0x69, 0x73, 0x69, 0x67, 0x6e,
		0x2d, 0x67, 0x72, 0x73, 0x03, 0x63, 0x6f, 0x6d,
		0x00, 0x78, 0x1b, 0x1a, 0xfd, 0x00, 0x00, 0x07,
		0x08, 0x00, 0x00, 0x03, 0x84, 0x00, 0x09, 0x3a,
		0x80, 0x00, 0x01, 0x51, 0x80
	};
#endif
	unsigned char reply_content[MAXLINE], *ptr, *end;
	char *nptr, *nend;
	dns_header *d_req, *d_reply;
	dns_queries queries;
	dns_answer answer;
	uint16_t opcode;

	/* validation */
	d_req = (dns_header *)request;
	if (maxlen <= sizeof(dns_header) ||			/* incomplete header */
	    d_req->flag_set.flag_num & htons(0x8000) ||		/* not query */
	    d_req->questions == 0)				/* no questions */
		return;
	opcode = d_req->flag_set.flag_num & htons(0x7800);
	ptr = request + sizeof(dns_header);
	end = request + maxlen;

	/* query, only first so far */
	memset(&queries, 0, sizeof(queries));
	nptr = queries.name;
	nend = queries.name + sizeof(queries.name) - 1;
	while (ptr < end) {
		size_t len = *ptr++;
		if (len > 63 || end - ptr < (len ? : 4))
			return;
		if (len == 0) {
			memcpy(&queries.type, ptr, 2);
			memcpy(&queries.ip_class, ptr + 2, 2);
			ptr += 4;
			break;
		}
		if (nptr < nend && *queries.name)
			*nptr++ = '.';
		if (nptr < nend)
			nptr = stpncpy(nptr, (char *)ptr, min(len, nend - nptr));
		ptr += len;
	}
	if (queries.type == 0 || queries.ip_class == 0 || strlen(queries.name) > 1025)
		return;
	maxlen = ptr - request;

	/* reply */
	if (maxlen > sizeof(reply_content))
		return;
	ptr = memcpy(reply_content, request, maxlen) + maxlen;
	end = reply_content + sizeof(reply_content);

	/* header */
	d_reply = (dns_header *)reply_content;
	d_reply->flag_set.flag_num = htons(0x8180);
	d_reply->questions = htons(1);
	d_reply->answer_rrs = htons(0);
	d_reply->auth_rrs = htons(0);
	d_reply->additional_rss = htons(0);

	/* answer */
	memset(&answer, 0, sizeof(answer));
	answer.name = htons(0xc00c);
	answer.type = queries.type;
	answer.ip_class = queries.ip_class;
	answer.ttl = htonl(0);
	answer.data_len = htons(4);

	sw_mode = nvram_get_int("sw_mode");
	if (opcode != 0) {
		/* not implemented, non-Query op */
		d_reply->flag_set.flag_num = htons(0x8184) | opcode;
	} else if (queries.ip_class == htons(1) && queries.type == htons(1)) {
		/* class IN type A */
		if (strcasecmp(queries.name, router_name) == 0
#ifdef RTCONFIG_FINDASUS
		 || strcasecmp(queries.name, "findasus.local") == 0
#endif
		) {
			/* no error, authoritative */
			d_reply->flag_set.flag_num = htons(0x8580);
			d_reply->answer_rrs = htons(1);
			if ((sw_mode == SW_MODE_REPEATER || ((sw_mode == SW_MODE_AP) && nvram_get_int("wlc_psta"))) && /* client mode */
			    nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED)
				answer.addr = inet_addr_(nvram_default_get("lan_ipaddr"));
			else
				answer.addr = inet_addr_(nvram_safe_get("lan_ipaddr"));
#if !defined(RTCONFIG_FINDASUS)
		} else if (strcasecmp(queries.name, "findasus.local") == 0) {
			/* non existent domain */
			d_reply->flag_set.flag_num = htons(0x8183);
			d_reply->auth_rrs = htons(1);
#endif
		} else if (*queries.name) {
			/* no error */
			d_reply->answer_rrs = htons(1);
			answer.addr = htonl(0x0a000001);	// 10.0.0.1
		}
	} else {
		/* not implemented */
		d_reply->flag_set.flag_num = htons(0x8184);
	}

	if (d_reply->answer_rrs) {
		if (end - ptr < sizeof(answer))
			return;
		ptr = memcpy(ptr, &answer, sizeof(answer)) + sizeof(answer);
	}
#if !defined(RTCONFIG_FINDASUS)
	if (d_reply->auth_rrs) {
		if (end - ptr < sizeof(auth_name_srv))
			return;
		ptr = memcpy(ptr, auth_name_srv, sizeof(auth_name_srv)) + sizeof(auth_name_srv);
	}
#endif

	sendto(sfd, reply_content, ptr - reply_content, 0, pcliaddr, clen);
}

void run_http_serv(int sockfd){
	ssize_t n;
	char line[MAXLINE];

	memset(line, 0, sizeof(line));

	if((n = read(sockfd, line, MAXLINE)) == 0){	// client close
		close_socket(sockfd, T_HTTP);

		return;
	}
	else if(n < 0){
		perror("wanduck read");
		return;
	}
	else{
		if(client[fd_i].type == T_HTTP)
			handle_http_req(sockfd, line);
		else
			close_socket(sockfd, T_HTTP);
	}
}

void run_dns_serv(int sockfd){
	unsigned char line[MAXLINE];
	struct sockaddr_in cliaddr;
	int n, clilen = sizeof(cliaddr);

	if((n = recvfrom(sockfd, line, MAXLINE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen)) == 0)	// client close
		return;
	else if(n < 0){
		perror("wanduck serv dns");
		return;
	}
	else
		handle_dns_req(sockfd, line, n, (struct sockaddr *)&cliaddr, clilen);
}

void record_wan_state_nvram(int wan_unit, int state, int sbstate, int auxstate){
	if(state != -1 && state != nvram_get_int(nvram_state[wan_unit]))
		nvram_set_int(nvram_state[wan_unit], state);

	if(sbstate != -1 && sbstate != nvram_get_int(nvram_sbstate[wan_unit]))
		nvram_set_int(nvram_sbstate[wan_unit], sbstate);

	if(auxstate != -1 && auxstate != nvram_get_int(nvram_auxstate[wan_unit]))
		nvram_set_int(nvram_auxstate[wan_unit], auxstate);
}

void record_conn_status(int wan_unit){
	struct sysinfo s_info;
	char prefix_wan[8], nvram_name[16], wan_proto[16];
	char log_title[32];
	char buf[32];
	int conn_wanup;
	long int timenow, wan_t0, wan_uptime, wan_bootdelay;

	memset(log_title, 0, 32);
	memset(buf, 0, 32);
#ifdef RTCONFIG_DUALWAN
	if(!strcmp(dualwan_mode, "lb") || !strcmp(dualwan_mode, "fb"))
		sprintf(log_title, "WAN(%d) Connection", wan_unit);
	else
#endif
		strcpy(log_title, "WAN Connection");

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_proto, 0, 16);
	strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

	conn_wanup = 0;
	if(conn_changed_state[wan_unit] == DISCONN || conn_changed_state[wan_unit] == C2D){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(disconn_case[wan_unit] == CASE_AP_DISCONN){
			if(disconn_case_old[wan_unit] == CASE_AP_DISCONN)
				return;
			disconn_case_old[wan_unit] = CASE_AP_DISCONN;

			logmessage(log_title, "Don't connect the AP yet.");
		}
		else
#endif
		if(disconn_case[wan_unit] == CASE_DISWAN){
			if(disconn_case_old[wan_unit] == CASE_DISWAN)
				return;
			disconn_case_old[wan_unit] = CASE_DISWAN;

			logmessage(log_title, "Ethernet link down.");
		}
		else if(disconn_case[wan_unit] == CASE_PPPFAIL){
			if(disconn_case_old[wan_unit] == CASE_PPPFAIL)
				return;
			disconn_case_old[wan_unit] = CASE_PPPFAIL;

			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				logmessage(log_title, "VPN authentication failed.");
			else if(ppp_fail_state == WAN_STOPPED_REASON_PPP_NO_ACTIVITY)
				logmessage(log_title, "No response from ISP.");
			else
				logmessage(log_title, "Fail to connect with some issues.");
		}
		else if(disconn_case[wan_unit] == CASE_DHCPFAIL){
			if(disconn_case_old[wan_unit] == CASE_DHCPFAIL)
				return;
			disconn_case_old[wan_unit] = CASE_DHCPFAIL;

			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_WIRELESSREPEATER
					|| sw_mode == SW_MODE_HOTSPOT
#endif
					)
				logmessage(log_title, "ISP's DHCP did not function properly.");
			else
				logmessage(log_title, "Detected that the WAN Connection Type was PPPoE. But the PPPoE Setting was not complete.");
		}
		else if(disconn_case[wan_unit] == CASE_MISROUTE){
			if(disconn_case_old[wan_unit] == CASE_MISROUTE)
				return;
			disconn_case_old[wan_unit] = CASE_MISROUTE;

			logmessage(log_title, "The router's ip was the same as gateway's ip. It led to your packages couldn't dispatch to internet correctly.");
		}
		else if(disconn_case[wan_unit] == CASE_THESAMESUBNET){
			if(disconn_case_old[wan_unit] == CASE_MISROUTE)
				return;
			disconn_case_old[wan_unit] = CASE_MISROUTE;

			logmessage(log_title, "The LAN's subnet may be the same with the WAN's subnet.");
		}
		else{	// disconn_case[wan_unit] == CASE_OTHERS
			if(disconn_case_old[wan_unit] == CASE_OTHERS)
				return;
			disconn_case_old[wan_unit] = CASE_OTHERS;

			logmessage(log_title, "WAN was exceptionally disconnected.");
		}
		//mark wan as down
		if((wan_unit == current_wan_unit)
#ifdef RTCONFIG_DUALWAN
			|| ((!strcmp(dualwan_mode, "lb") && (wan_unit == WAN_UNIT_FIRST)))
#endif
		){
			timenow = (long int) (time(0));
			wan_uptime = atol(nvram_get("wan_uptime"));
			wan_t0 = atol(nvram_get("wan_t0"));
			sysinfo(&s_info);
			if(wan_t0 > 0){
				wan_uptime = wan_uptime + (timenow - wan_t0);
			}
			else if(wan_t0 == 0){
				wan_uptime = wan_uptime + s_info.uptime;
			}
		//logmessage(log_title, "wan_unit=%d current_wan_unit=%d timenow=%ld system_uptime=%ld wan_t0=%ld wan_uptime=%ld", wan_unit, current_wan_unit, timenow, s_info.uptime, wan_t0, wan_uptime);
			sprintf(buf, "wan_uptime=%ld", wan_uptime);
			eval("nvram", "set", buf);
			sprintf(buf, "wan_t0=%d", -1);
			eval("nvram", "set", buf);
			return;
		}
	}
	else if(conn_changed_state[wan_unit] == D2C){
		if(disconn_case_old[wan_unit] == 10)
			return;
		disconn_case_old[wan_unit] = 10;
		conn_wanup = 1;

		logmessage(log_title, "WAN was restored.");
	}
	else if(conn_changed_state[wan_unit] == PHY_RECONN){
		if(disconn_case_old[wan_unit] == PHY_RECONN)
			return;
		disconn_case_old[wan_unit] = PHY_RECONN;
		//conn_wanup = 1;

		logmessage(log_title, "Ethernet link up.");
	}

	// save new wan start time
	if(conn_wanup
		// && ((wan_unit == current_wan_unit)
//#ifdef RTCONFIG_DUALWAN
//		|| ((!strcmp(dualwan_mode, "lb") && (wan_unit == WAN_UNIT_FIRST))))
//#endif
	){
		timenow = (long int) (time(0));
		wan_bootdelay = atol(nvram_get("wan_bootdly"));
		sysinfo(&s_info);
		if(nvram_match("ntp_sync", "1")) {
			sprintf(buf, "wan_t0=%ld", timenow);
			eval("nvram", "set", buf);
		}
		else {
			sprintf(buf, "wan_bootdly=%ld", wan_bootdelay + s_info.uptime);
			eval("nvram", "set", buf);
			sprintf(buf, "wan_t0=%d", 0);
			eval("nvram", "set", buf);
		}
#ifdef RTCONFIG_DNSCRYPT
		if (nvram_match("dnscrypt_proxy", "1")) {
			memset(buf, 0, 32);
			sprintf(buf, "restart_dnscrypt 0");
			notify_rc_and_wait(buf);
		}
#endif
#ifdef RTCONFIG_STUBBY
		if (nvram_match("stubby_proxy", "1")) {
			memset(buf, 0, 32);
			sprintf(buf, "restart_stubby 0");
			notify_rc_and_wait(buf);
		}
#endif
	}
}

int get_disconn_count(int wan_unit){
	return changed_count[wan_unit];
}

void set_disconn_count(int wan_unit, int flag){
	changed_count[wan_unit] = flag;
}

int switch_wan_line(const int wan_unit){
#ifdef RTCONFIG_USB_MODEM
	int retry, lock;
#endif
	char cmd[32];
	int unit;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		if(unit == wan_unit)
			break;
	if(unit == WAN_UNIT_MAX)
		return 0;

	if(wan_primary_ifunit() == wan_unit) // Already have no running modem.
		return 0;
#ifdef RTCONFIG_USB_MODEM
	else if (dualwan_unit__usbif(wan_unit)) {
		if(!link_wan[wan_unit])
			return 0; // No modem in USB ports.
	}
#endif

	if(wanduck_debug & 1)
		_dprintf("%s: wan(%d) Starting...\n", __FUNCTION__, wan_unit);
	// Set the modem to be running.
	set_wan_primary_ifunit(wan_unit);

#ifdef RTCONFIG_USB_MODEM
	if (nvram_invmatch("modem_enable", "4") && dualwan_unit__usbif(wan_unit)) {
		// Wait the PPP config file to be done.
		retry = 0;
		while((lock = file_lock("3g")) == -1 && retry < MAX_WAIT_FILE)
			sleep(1);

		if(lock == -1){
			if(wanduck_debug & 1)
				_dprintf("(%d): No pppd conf file and turn off the state of USB Modem.\n", wan_unit);
			set_wan_primary_ifunit(0);
			return 0;
		}
		else
			file_unlock(lock);
	}
#endif

	// TODO: don't know if it's necessary?
	// clean or restart the other line.
#ifdef RTCONFIG_DUALWAN
	if(!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
#endif
	{
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
			if(unit == wan_unit)
				continue;

			memset(cmd, 0, 32);
			sprintf(cmd, "restart_wan_if %d", !wan_unit);
TRACE_PT("%s.\n", cmd);
			notify_rc_and_wait(cmd);
			sleep(1);
		}
	}

	// restart the primary line.
	memset(cmd, 0, 32);
	if(get_wan_state(wan_unit) == WAN_STATE_CONNECTED && wans_dualwan == 0)
		sprintf(cmd, "restart_wan_line %d", wan_unit);
	else
		sprintf(cmd, "restart_wan_if %d", wan_unit);  // always restart wan_if for dualwan
TRACE_PT("%s.\n", cmd);
	notify_rc_and_wait(cmd);

#ifdef RTCONFIG_DUALWAN
	if(sw_mode == SW_MODE_ROUTER
			&& (!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
			){
		delay_detect = 1;
	}
#endif

	if(wanduck_debug & 1)
		_dprintf("%s: wan(%d) End.\n", __FUNCTION__, wan_unit);
	return 1;
}

int wanduck_main(int argc, char *argv[]){
	char *http_servport, *dns_servport;
	unsigned int clilen;
	struct sockaddr_in cliaddr;
	struct timeval  tval;
	int nready, maxi, sockfd;
	int wan_unit;
	char prefix_wan[8];
	char cmd[32];
#ifdef RTCONFIG_WIRELESSREPEATER
	char domain_mapping[64];
#endif
#ifdef RTCONFIG_DSL
	int usb_switched_back_dsl = 0;
#endif
#ifdef RTCONFIG_DUALWAN
#if defined(RTAC68U) ||  defined(RTAC87U)
	int wanred_led_status = 0;	/* 1 is no internet, 2 is internet ok */
	int u, link_status;
#endif
#endif
#ifdef RTCONFIG_USB_MODEM
	char tmp2[100];
#endif
	unsigned int now;

	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, safe_leave);
	signal(SIGCHLD, chld_reap);
	signal(SIGUSR1, get_network_nvram);
	signal(SIGUSR2, wan_led_control);

	if(argc < 3){
		http_servport = DFL_HTTP_SERV_PORT;
		dns_servport = DFL_DNS_SERV_PORT;
	}
	else{
		if(atoi(argv[1]) <= 0)
			http_servport = DFL_HTTP_SERV_PORT;
		else
			http_servport = (char *)argv[1];

		if(atoi(argv[2]) <= 0)
			dns_servport = DFL_DNS_SERV_PORT;
		else
			dns_servport = (char *)argv[2];
	}

	if(build_socket(http_servport, dns_servport, &http_sock, &dns_sock) < 0){
		if(wanduck_debug & 1)
			_dprintf("\n*** Fail to build socket! ***\n");
		exit(0);
	}

	FILE *fp = fopen(WANDUCK_PID_FILE, "w");

	if(fp != NULL){
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	maxfd = (http_sock > dns_sock)?http_sock:dns_sock;
	maxi = -1;

	FD_ZERO(&allset);
	FD_SET(http_sock, &allset);
	FD_SET(dns_sock, &allset);

	for(fd_i = 0; fd_i < MAX_USER; ++fd_i){
		client[fd_i].sfd = -1;
		client[fd_i].type = 0;
	}

	rule_setup = 0;
	got_notify = 0;
	clilen = sizeof(cliaddr);

	sprintf(router_name, "%s", DUT_DOMAIN_NAME);

	nvram_set_int("link_wan", 0);
	nvram_set_int("link_wan1", 0);
	nvram_set_int("link_internet", 2);
#ifdef RTCONFIG_WIRELESSREPEATER
	nvram_set_int("link_ap", 2);
#endif

	for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
		link_setup[wan_unit] = 0;
		link_wan[wan_unit] = DISCONN;

		changed_count[wan_unit] = S_IDLE;
		disconn_case[wan_unit] = CASE_NONE;

		memset(prefix_wan, 0, 8);
		sprintf(prefix_wan, "wan%d_", wan_unit);

		strcat_r(prefix_wan, "state_t", nvram_state[wan_unit]);
		strcat_r(prefix_wan, "sbstate_t", nvram_sbstate[wan_unit]);
		strcat_r(prefix_wan, "auxstate_t", nvram_auxstate[wan_unit]);

		set_disconn_count(wan_unit, S_IDLE);
	}

	loop_sec = uptime();

#ifdef RTCONFIG_USB_MODEM
	nvram_set_int(strcat_r(prefix_wan, "is_usb_modem_ready", tmp2), link_wan[wan_unit]);
#endif
	get_related_nvram(); // initial the System's variables.
	get_lan_nvram(); // initial the LAN's variables.

#ifdef RTCONFIG_DUALWAN
#if 1
	WAN_FB_UNIT = WAN_UNIT_FIRST;
#else
	WAN_FB_UNIT = WAN_UNIT_SECOND;
#endif

	if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
		cross_state = DISCONN;
		for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
			conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
			if(conn_state[wan_unit] == CONNED)
				conn_state[wan_unit] = if_wan_connected(wan_unit, nvram_get_int(nvram_state[wan_unit]));

			conn_changed_state[wan_unit] = conn_state[wan_unit];

			if(conn_state[wan_unit] == CONNED && cross_state != CONNED)
				cross_state = CONNED;

			conn_state_old[wan_unit] = conn_state[wan_unit];

			record_conn_status(wan_unit);

			if(cross_state == CONNED)
				set_disconn_count(wan_unit, S_IDLE);
			else
				set_disconn_count(wan_unit, S_COUNT);
		}
	}
	else if(sw_mode == SW_MODE_ROUTER
			&& (!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
			){
		if(wandog_delay > 0){
			if(wanduck_debug & 1)
				_dprintf("wanduck: delay %d seconds...\n", wandog_delay);
			sleep(wandog_delay);
			delay_detect = 0;
		}

		// To check the phy connection of the standby line.
		for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
			conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
		}

		current_wan_unit = wan_primary_ifunit();
		other_wan_unit = !current_wan_unit;

		if(conn_state[current_wan_unit] == CONNED)
			conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, nvram_get_int(nvram_state[current_wan_unit]));

		conn_changed_state[current_wan_unit] = conn_state[current_wan_unit];

		cross_state = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

		record_conn_status(current_wan_unit);
	}
	else
#endif
	if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
			|| sw_mode == SW_MODE_HOTSPOT
#endif
			){
		current_wan_unit = wan_primary_ifunit();
		other_wan_unit = !current_wan_unit;

		conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);
		if(conn_state[current_wan_unit] == CONNED)
			conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, nvram_get_int(nvram_state[current_wan_unit]));

		conn_changed_state[current_wan_unit] = conn_state[current_wan_unit];

		cross_state = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

		record_conn_status(current_wan_unit);
	}
	else{ // sw_mode == SW_MODE_AP, SW_MODE_REPEATER.
		current_wan_unit = WAN_UNIT_FIRST;

		conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);

		cross_state = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER)
			record_conn_status(current_wan_unit);
#endif
	}

	/*
	 * Because start_wanduck() is run early than start_wan()
	 * and the redirect rules is already set before running wanduck,
	 * handle_wan_line() must not be run at the first detect.
	 */
	if(cross_state == DISCONN){
		if(wanduck_debug & 1)
			_dprintf("\n# Enable direct rule\n");
		rule_setup = 1;
	}
	else if(cross_state == CONNED && isFirstUse){
		if(wanduck_debug & 1)
			_dprintf("\n#CONNED : Enable direct rule\n");
		rule_setup = 1;
	}

	int first_loop = 1;
	unsigned int diff;
	for(;;){
		if(!first_loop){
			now = uptime();
			diff = now-loop_sec;

			if(diff < scan_interval){
				rset = allset;
				tval.tv_sec = scan_interval-diff;
				tval.tv_usec = 0;

				goto WANDUCK_SELECT;
			}

			loop_sec = now;
		}
		else
			first_loop = 0;

		rset = allset;
		tval.tv_sec = scan_interval;
		tval.tv_usec = 0;

		get_related_nvram();

#ifdef RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
#ifdef RTCONFIG_USB_MODEM
				if(dualwan_unit__usbif(wan_unit) && !link_wan[wan_unit]){
					if_wan_phyconnected(wan_unit);
					continue;
				}
#endif

				current_state[wan_unit] = nvram_get_int(nvram_state[wan_unit]);

				if(current_state[wan_unit] == WAN_STATE_DISABLED){
					//record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

					disconn_case[wan_unit] = CASE_OTHERS;
					conn_state[wan_unit] = DISCONN;
				}
				else{
					conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
					if(conn_state[wan_unit] == CONNED)
						conn_state[wan_unit] = if_wan_connected(wan_unit, current_state[wan_unit]);
				}

				if(conn_state[wan_unit] != conn_state_old[wan_unit]){
					if(conn_state[wan_unit] == PHY_RECONN){
						conn_changed_state[wan_unit] = PHY_RECONN;
					}
#ifdef RTCONFIG_USB_MODEM
					else if(conn_state[wan_unit] == SET_ETH_MODEM){
						conn_changed_state[wan_unit] = SET_ETH_MODEM;
					}
#endif
					else if(conn_state[wan_unit] == DISCONN){
						conn_changed_state[wan_unit] = C2D;

#ifdef RTCONFIG_USB_MODEM
						if (dualwan_unit__usbif(wan_unit))
							set_disconn_count(wan_unit, max_disconn_count);
						else
#endif
						set_disconn_count(wan_unit, S_COUNT);
					}
					else if(conn_state[wan_unit] == CONNED){
						if(rule_setup == 1 && !isFirstUse){
							if(wanduck_debug & 1)
								_dprintf("\n# DualWAN: Disable direct rule(D2C)\n");
							rule_setup = 0;
						}

						conn_changed_state[wan_unit] = D2C;

						set_disconn_count(wan_unit, S_IDLE);
					}
					else
						conn_changed_state[wan_unit] = CONNED;

					conn_state_old[wan_unit] = conn_state[wan_unit];

					record_conn_status(wan_unit);
				}

				if(get_disconn_count(wan_unit) != S_IDLE){
					if(conn_state[wan_unit] == PHY_RECONN)
						set_disconn_count(wan_unit, max_disconn_count);

					if(get_disconn_count(wan_unit) >= max_disconn_count){
						set_disconn_count(wan_unit, S_IDLE);

						memset(cmd, 0, 32);
						sprintf(cmd, "restart_wan_if %d", wan_unit);
						notify_rc_and_period_wait(cmd, 30);

						memset(cmd, 0, 32);
						sprintf(cmd, "restart_wan_line %d", !wan_unit);
						notify_rc(cmd);
					}
					else
						set_disconn_count(wan_unit, get_disconn_count(wan_unit)+1);

					if(wanduck_debug & 1)
						_dprintf("%s: wan(%d) disconn count = %d/%d ...\n", __FUNCTION__, wan_unit, get_disconn_count(wan_unit), max_disconn_count);
				}
			}
		}
		else if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "fo")){
			if(delay_detect == 1 && wandog_delay > 0){
				if(wanduck_debug & 1)
					_dprintf("wanduck: delay %d seconds...\n", wandog_delay);
				sleep(wandog_delay);
				delay_detect = 0;
			}

			// To check the phy connection of the standby line.
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
				conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
			}

			current_wan_unit = wan_primary_ifunit();
			other_wan_unit = !current_wan_unit;

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else{
				if(conn_state[current_wan_unit] == CONNED)
					conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, current_state[current_wan_unit]);
			}

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#ifdef RTCONFIG_USB_MODEM
			else if(conn_state[current_wan_unit] == SET_ETH_MODEM){
				conn_changed_state[current_wan_unit] = SET_ETH_MODEM;

				conn_state_old[current_wan_unit] = DISCONN;

				// The USB modem is a router type dongle, and must let the local subnet not be the "192.168.1.x".
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
			else if(conn_state[current_wan_unit] == CONNED){
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];
				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

				if(disconn_case[current_wan_unit] == CASE_THESAMESUBNET){
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED
						&& get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
						)
					set_disconn_count(current_wan_unit, S_COUNT);
			}

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					if(wanduck_debug & 1)
						_dprintf("# wanduck: wait time for switching: %d/%d.\n", get_disconn_count(current_wan_unit)*scan_interval, max_wait_time);
				}
				else{
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_COUNT: changed_count[] >= max_disconn_count.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "fb")){
			if(delay_detect == 1 && wandog_delay > 0){
				if(wanduck_debug & 1)
					_dprintf("wanduck: delay %d seconds...\n", wandog_delay);
				sleep(wandog_delay);
				delay_detect = 0;
			}

			// To check the phy connection of the standby line.
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
				conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
			}

			current_wan_unit = wan_primary_ifunit();
			other_wan_unit = !current_wan_unit;

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;

				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else{
				if(conn_state[current_wan_unit] == CONNED)
					conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, current_state[current_wan_unit]);

				if(other_wan_unit == WAN_FB_UNIT && conn_state[other_wan_unit] == CONNED){
					current_state[other_wan_unit] = nvram_get_int(nvram_state[other_wan_unit]);
					conn_state[other_wan_unit] = if_wan_connected(other_wan_unit, current_state[other_wan_unit]);
					if(wanduck_debug & 1)
						_dprintf("wanduck: detect the fail-back line(%d): %d.\n", other_wan_unit+1, conn_state[other_wan_unit]);
				}
			}

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#ifdef RTCONFIG_USB_MODEM
			else if(conn_state[current_wan_unit] == SET_ETH_MODEM){
				conn_changed_state[current_wan_unit] = SET_ETH_MODEM;

				conn_state_old[current_wan_unit] = DISCONN;

				// The USB modem is a router type dongle, and must let the local subnet not be the "192.168.1.x".
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
			else if(conn_state[current_wan_unit] == CONNED){
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

				if(disconn_case[current_wan_unit] == CASE_THESAMESUBNET){
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED
						&& get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
						)
					set_disconn_count(current_wan_unit, S_COUNT);
			}

			if(other_wan_unit == WAN_FB_UNIT){
				if(conn_state[other_wan_unit] == CONNED
						&& get_disconn_count(other_wan_unit) == S_IDLE
						)
					set_disconn_count(other_wan_unit, S_COUNT);
				else if(conn_state[other_wan_unit] == DISCONN)
					set_disconn_count(other_wan_unit, S_IDLE);
			}
			else
				set_disconn_count(other_wan_unit, S_IDLE);

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					if(wanduck_debug & 1)
						_dprintf("# wanduck: wait time for switching: %d/%d.\n", get_disconn_count(current_wan_unit)*scan_interval, max_wait_time);
				}
				else{
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_COUNT: changed_count[] >= max_disconn_count.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}

			if(get_disconn_count(other_wan_unit) != S_IDLE){
				if(get_disconn_count(other_wan_unit) < max_fb_count){
					set_disconn_count(other_wan_unit, get_disconn_count(other_wan_unit)+1);
					if(wanduck_debug & 1)
						_dprintf("# wanduck: wait time for returning: %d/%d.\n", get_disconn_count(other_wan_unit)*scan_interval, max_fb_wait_time);
				}
				else{
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_COUNT: changed_count[] >= max_fb_count.\n");
					set_disconn_count(other_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else
#endif // RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
				|| sw_mode == SW_MODE_HOTSPOT
#endif
				){
			current_wan_unit = wan_primary_ifunit();
			other_wan_unit = !current_wan_unit;

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);
#ifdef RTCONFIG_LANWAN_LED
				led_control(LED_WAN, LED_OFF);
#endif

				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;

				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else{
				conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);
				if(conn_state[current_wan_unit] == CONNED)
					conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, current_state[current_wan_unit]);
			}

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#ifdef RTCONFIG_USB_MODEM
			else if(conn_state[current_wan_unit] == SET_ETH_MODEM){
				conn_changed_state[current_wan_unit] = SET_ETH_MODEM;

				conn_state_old[current_wan_unit] = DISCONN;

				// The USB modem is a router type dongle, and must let the local subnet not be the "192.168.1.x".
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
			else if(conn_state[current_wan_unit] == CONNED){
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

#ifdef RTCONFIG_DSL /* Paul add 2013/7/29, for Non-DualWAN 3G/4G WAN -> DSL WAN, auto Fail-Back feature */
			if (nvram_match("dsltmp_adslsyncsts","up") && current_wan_unit == WAN_UNIT_SECOND){
				if(wanduck_debug & 1)
					_dprintf("\n# wanduck: adslsync up.\n");
				set_disconn_count(current_wan_unit, S_IDLE);
				link_wan[current_wan_unit] = DISCONN;
				conn_state[current_wan_unit] = DISCONN;
				usb_switched_back_dsl = 1;
				max_disconn_count = 1;
			}
			else
				set_disconn_count(current_wan_unit, S_IDLE);
#else
			set_disconn_count(current_wan_unit, S_IDLE);
#endif
			}
			else if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

				if(disconn_case[current_wan_unit] == CASE_THESAMESUBNET){
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED)
					set_disconn_count(current_wan_unit, S_COUNT);
			}

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					if(wanduck_debug & 1)
						_dprintf("# wanduck: wait time for switching: %d/%d.\n", get_disconn_count(current_wan_unit)*scan_interval, max_wait_time);
				}
				else{
					if(wanduck_debug & 1)
						_dprintf("# wanduck: set S_COUNT: changed_count[] >= max_disconn_count.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else{ // sw_mode == SW_MODE_AP, SW_MODE_REPEATER.
			current_wan_unit = WAN_UNIT_FIRST;
			conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);

			if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;
			}
			else{
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;
			}

			conn_state_old[current_wan_unit] = conn_state[current_wan_unit];
		}

#ifdef RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
			;
		}
		else
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER){
			if(!got_notify)
				; // do nothing.
			else if(conn_changed_state[current_wan_unit] == DISCONN || conn_changed_state[current_wan_unit] == C2D || isFirstUse){
				//if(rule_setup == 0){
					if(conn_changed_state[current_wan_unit] == DISCONN){
						if(wanduck_debug & 1)
							_dprintf("\n# mode(%d): Enable direct rule(DISCONN)\n", sw_mode);
					}
					else if(conn_changed_state[current_wan_unit] == C2D){
						if(wanduck_debug & 1)
							_dprintf("\n# mode(%d): Enable direct rule(C2D)\n", sw_mode);
					}
					else
						if(wanduck_debug & 1)
							_dprintf("\n# mode(%d): Enable direct rule(isFirstUse)\n", sw_mode);
					rule_setup = 1;

					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");
					// Drop the DHCP server from PAP.
					eval("ebtables", "-t", "filter", "-I", "FORWARD", "-i", nvram_safe_get(wlc_nvname("ifname")), "-j", "DROP");
					f_write_string("/proc/net/dnsmqctrl", "", 0, 0);

					stop_nat_rules();
				//}
				got_notify = 0;
			}
			else{
				//if(rule_setup == 1 && !isFirstUse){
				if(!isFirstUse){
					if(wanduck_debug & 1)
						_dprintf("\n# mode(%d): Disable direct rule(CONNED)\n", sw_mode);
					rule_setup = 0;

					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");
					eval("ebtables", "-t", "broute", "-I", "BROUTING", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
					sprintf(domain_mapping, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
					f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);

					start_nat_rules();
				}

				got_notify = 0;
			}
		}
		else
#endif
		if(sw_mode == SW_MODE_AP){
			; // do nothing.
		}
		else if(conn_changed_state[current_wan_unit] == C2D || (conn_changed_state[current_wan_unit] == CONNED && isFirstUse)){
			if(rule_setup == 0){
				if(conn_changed_state[current_wan_unit] == C2D){
					if (nvram_match("led_disable", "0")) {
#ifdef RTCONFIG_DSL /* Paul add 2012/10/18 */
					led_control(LED_WAN, LED_OFF);
#endif
						if(
#ifdef RTAC68U
							is_ac66u_v2_series()
#else
							1
#endif // RTAC68U
#ifdef RTCONFIG_DUALWAN
							&& (strcmp(dualwan_mode, "lb") == 0 ||
								 strcmp(dualwan_mode, "fb") == 0 ||
								 strcmp(dualwan_mode, "fo") == 0)
#endif // RTCONFIG_DUALWAN
						){
							logmessage("DualWAN", "skip single wan wan_led_control - WANRED off\n");
							if(nvram_match("AllLED", "1")){
								led_control(LED_WAN, LED_ON);
								disable_wan_led();
							}
						}
					}
					if(wanduck_debug & 1)
						_dprintf("\n# Enable direct rule(C2D)\n");
				}
				else
					if(wanduck_debug & 1)
						_dprintf("\n# Enable direct rule(isFirstUse)\n");
				rule_setup = 1;

				handle_wan_line(current_wan_unit, rule_setup);

				if(conn_changed_state[current_wan_unit] == C2D
#ifdef RTCONFIG_DUALWAN
						&& strcmp(dualwan_mode, "off")
#endif
						){
#ifdef RTCONFIG_USB_MODEM
					// the current line is USB and be plugged off.
					if (!link_wan[current_wan_unit] && dualwan_unit__usbif(current_wan_unit)) {
						if(wanduck_debug & 1)
							_dprintf("\n# wanduck(C2D): Modem was plugged off and try to Switch the other line.\n");
						switch_wan_line(other_wan_unit);

#ifdef RTCONFIG_DSL /* Paul add 2013/7/29, for Non-DualWAN 3G/4G WAN -> DSL WAN, auto Fail-Back feature */
#ifndef RTCONFIG_DUALWAN
						if (nvram_match("dsltmp_adslsyncsts","up") && usb_switched_back_dsl == 1){
							if(wanduck_debug & 1)
								_dprintf("\n# wanduck: usb_switched_back_dsl: 1.\n");
							link_wan[WAN_UNIT_SECOND] = CONNED;
							max_disconn_count = max_wait_time/scan_interval;
							usb_switched_back_dsl = 0;
						}
#endif
#endif
					}
					else
#endif
					// C2D: Try to prepare the backup line.
					if(link_wan[other_wan_unit] == 1){
						if(get_wan_state(other_wan_unit) != WAN_STATE_CONNECTED){
							if(wanduck_debug & 1)
								_dprintf("\n# wanduck(C2D): Try to prepare the backup line.\n");
							memset(cmd, 0, 32);
							sprintf(cmd, "restart_wan_if %d", other_wan_unit);
							notify_rc_and_wait(cmd);
						}
					}
				}
			}
		}
		else if(conn_changed_state[current_wan_unit] == D2C || conn_changed_state[current_wan_unit] == CONNED){
			if(rule_setup == 1 && !isFirstUse){
				if (nvram_match("led_disable", "0")) {
					if(nvram_match("AllLED", "1"))
						led_control(LED_WAN, LED_ON);
					else
						led_control(LED_WAN, LED_OFF);
				if(nvram_match("AllLED", "1")
#ifdef RTAC68U
					&& is_ac66u_v2_series()
#endif
				){
						led_control(LED_WAN, LED_OFF);
						enable_wan_led();
					}
				}
				if(wanduck_debug & 1)
					_dprintf("\n# Disable direct rule(D2C)\n");
				rule_setup = 0;
				handle_wan_line(current_wan_unit, rule_setup);
			}
		}
		/*
		 * when all lines are plugged in and the currect line is disconnected over max_wait_time seconds,
		 * switch the connect to the other line.
		 */
		else if(conn_changed_state[current_wan_unit] == DISCONN){
			if(get_disconn_count(current_wan_unit) >= max_disconn_count
#ifdef RTCONFIG_USB_MODEM
					|| (!link_wan[current_wan_unit] && dualwan_unit__usbif(current_wan_unit))
#endif
					)
			{
				if(current_wan_unit){
					if(wanduck_debug & 1)
						_dprintf("# Switching the connect to the first WAN line...\n");
				}
				else
					if(wanduck_debug & 1)
						_dprintf("# Switching the connect to the second WAN line...\n");
				set_disconn_count(other_wan_unit, S_IDLE);
				switch_wan_line(other_wan_unit);
			}
		}
		// phy connected -> disconnected -> connected
		else if(conn_changed_state[current_wan_unit] == PHY_RECONN){
			handle_wan_line(current_wan_unit, 0);
		}

#ifdef RTCONFIG_DUALWAN
		if(!strcmp(dualwan_mode, "fb") && other_wan_unit == WAN_FB_UNIT && conn_state[other_wan_unit] == CONNED
				&& get_disconn_count(other_wan_unit) >= max_fb_count
				){
			if(wanduck_debug & 1)
				_dprintf("# wanduck: returning the connect to the %d WAN line...\n", other_wan_unit);
			set_disconn_count(other_wan_unit, S_IDLE);
			switch_wan_line(other_wan_unit);
		}
#endif

#if defined(RTAC68U) || defined(RTAC87U)
		if (strcmp(dualwan_wans, "wan none")) {
			if(nvram_match("AllLED", "1")
#ifdef RTAC68U
				&& is_ac66u_v2_series()
#endif
			){
				link_status = 0;
				for (u = WAN_UNIT_FIRST; !link_status && u < WAN_UNIT_MAX; ++u) {
					if(conn_state[u] == CONNED)
						link_status++;
				}

				if(link_status) {
					if(wanred_led_status != 2 ){
						led_control(LED_WAN, LED_OFF);
						enable_wan_led();
						wanred_led_status = 2;
					}
				}else{
					if(wanred_led_status != 1 ){
						led_control(LED_WAN, LED_ON);
						disable_wan_led();
						wanred_led_status = 1;
					}
				}
			}
		}
#endif

		start_demand_ppp(current_wan_unit, 1);

WANDUCK_SELECT:
		if((nready = select(maxfd+1, &rset, NULL, NULL, &tval)) <= 0)
			continue;

		if(FD_ISSET(dns_sock, &rset)){
			run_dns_serv(dns_sock);
			if(--nready <= 0)
				continue;
		}
		else if(FD_ISSET(http_sock, &rset)){
			if((cur_sockfd = accept(http_sock, (struct sockaddr *)&cliaddr, &clilen)) <= 0){
				perror("http accept");
				continue;
			}

			for(fd_i = 0; fd_i < MAX_USER; ++fd_i){
				if(client[fd_i].sfd < 0){
					client[fd_i].sfd = cur_sockfd;
					client[fd_i].type = T_HTTP;
					break;
				}
			}

			if(fd_i == MAX_USER){
				if(wanduck_debug & 1)
					_dprintf("# wanduck servs full\n");
				close(cur_sockfd);
				continue;
			}

			FD_SET(cur_sockfd, &allset);
			if(cur_sockfd > maxfd)
				maxfd = cur_sockfd;
			if(fd_i > maxi)
				maxi = fd_i;

			if(--nready <= 0)
				continue;	// no more readable descriptors
		}

		// polling
		for(fd_i = 0; fd_i <= maxi; ++fd_i){
			if((sockfd = client[fd_i].sfd) < 0)
				continue;

			if(FD_ISSET(sockfd, &rset)){
				int nread;
				ioctl(sockfd, FIONREAD, &nread);
				if(nread == 0){
					close_socket(sockfd, T_HTTP);
					continue;
				}

				cur_sockfd = sockfd;

				run_http_serv(sockfd);

				if(--nready <= 0)
					break;
			}
		}
	}

	if(wanduck_debug & 1)
		_dprintf("# wanduck exit error\n");
	exit(1);
}
