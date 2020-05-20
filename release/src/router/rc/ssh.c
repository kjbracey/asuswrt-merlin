/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/

#include "rc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shutils.h>
#include <shared.h>

static inline int check_host_key(const char *ktype, const char *nvname, const char *hkfn)
{
	char jname[64];
	char dname[64];
	char *value;

	snprintf(&dname[0], sizeof(dname), "/etc/%s", hkfn);
	value = nvram_safe_get("sshd_authkeys");

	if (!strncmp(value, "/jffs", 5)) {	// authkeys moved to jffs, attempt to copy the rest of the dropbear keys from jffs
		snprintf(&jname[0], sizeof(jname), "/jffs/%s", hkfn);
		if(check_if_file_exist(jname)) {
			eval("cp", jname, dname);
			nvram_unset(nvname);  //key in jffs, release nvram
			return 1;
		}
		else {
			if (nvram_get_file(nvname, dname, 2048)) {  // copy key from nvram
				eval("cp", dname, jname);
				nvram_unset(nvname);  //key in jffs, release nvram
			}
			else {
				mkdir("/jffs/dropbear", 0700);
				eval("dropbearkey", "-t", (char *)ktype, "-f", dname);
				eval("cp", dname, jname);
				nvram_unset(nvname);  //new key in jffs, release nvram
				return 1;
			}
		}
	}
	else {
		unlink(dname);
		if (!nvram_get_file(nvname, dname, 2048)) {
			eval("dropbearkey", "-t", (char *)ktype, "-f", dname);
			if (nvram_set_file(nvname, dname, 2048)) {
				return 1;
			}
		}
	}

	return 0;
}

void start_sshd(void)
{
	int dirty = 0;
	char buf[3000];
	char *argv[24];
	int j, argc;
	char *p, *str, *token, *saveptr;
	char saddr[128];
	char timeout[8];

	if (!nvram_get_int("sshd_enable"))
		return;

	if (getpid() != 1) {
		notify_rc("start_sshd");
		return;
	}

	mkdir("/etc/dropbear", 0700);
	mkdir("/root/.ssh", 0700);

	f_write_string("/root/.ssh/authorized_keys", get_parsed_crt("sshd_authkeys", buf, sizeof(buf)), 0, 0700);

	dirty |= check_host_key("rsa", "sshd_hostkey", "dropbear/dropbear_rsa_host_key");
	dirty |= check_host_key("dss", "sshd_dsskey",  "dropbear/dropbear_dss_host_key");
	dirty |= check_host_key("ecdsa", "sshd_ecdsakey", "dropbear/dropbear_ecdsa_host_key");
    dirty |= check_host_key("ed25519", "sshd_ed25519key", "dropbear/dropbear_ed25519_host_key");
	if (dirty)
		nvram_commit_x();

/*
	xstart("dropbear", "-a", "-p", nvram_safe_get("sshd_port"), nvram_get_int("sshd_pass") ? "" : "-s");
*/
	argv[0] = "dropbear";
	argc = 1;

	if (((p = nvram_get("sshd_addr")) != NULL) && (*p)) {
		strlcpy(saddr, p, sizeof(saddr));
		for (j = 1, str = saddr; ; j++, str = NULL) {
			token = strtok_r(str, ", ", &saveptr);
			if (token == NULL)
				break;
			argv[argc++] = "-p";
			argv[argc++] = token;
			if (j >= 4) /* allow 4 entries */
				break;
		}
	} else {
		if (is_routing_enabled() && nvram_get_int("sshd_wan") != 1) {
			snprintf(saddr, sizeof(saddr), "%s:%d", nvram_safe_get("lan_ipaddr"), nvram_get_int("sshd_port") ? : 22);
			p = saddr;
		} else
			p = nvram_safe_get("sshd_port");
		argv[argc++]= "-p";
		argv[argc++] = p;
	}

	if (nvram_get_int("sshd_timeout")) {
		snprintf(timeout, sizeof(timeout), "%d", nvram_get_int("sshd_timeout") * 60);
		argv[argc++] = "-I";
		argv[argc++] = timeout;
	}

	if (!nvram_get_int("sshd_pass")) argv[argc++] = "-s";

	if (nvram_get_int("sshd_forwarding")) {
		argv[argc++] = "-a";
	} else {
		argv[argc++] = "-j";
		argv[argc++] = "-k";
	}

	if (((p = nvram_get("sshd_rwb")) != NULL) && (*p)) {
		argv[argc++] = "-W";
		argv[argc++] = p;
	}

	argv[argc] = NULL;
	_eval(argv, NULL, 0, NULL);

}

void stop_sshd(void)
{
	killall("dropbear", SIGTERM);
}
