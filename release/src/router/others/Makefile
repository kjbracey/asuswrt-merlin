include ../common.mak

all:

install:
	install -d $(INSTALLDIR)/sbin
	install -d $(INSTALLDIR)/usr/sbin
	install -d $(INSTALLDIR)/lib

ifeq ($(TCONFIG_USB),y)
# Optware perl symlink
	install -d $(INSTALLDIR)/usr/bin
	ln -sf /opt/bin/perl $(INSTALLDIR)/usr/bin/perl
endif

# cron helper
	install -m 0700 cru $(INSTALLDIR)/usr/sbin

# playing with ipkg / from OpenWrt svn
#!!TB	install -m 0700 ipkg $(INSTALLDIR)/usr/sbin

# system info
	install -m 0700 sysinfo $(INSTALLDIR)/usr/sbin

# keep winscp happy
#	install -m 0700 groups $(INSTALLDIR)/usr/bin

# chpasswd script
	install -m 0700 chpasswd.sh $(INSTALLDIR)/usr/sbin

# dnscrypt resolver updater
ifeq ($(RTCONFIG_DNSCRYPT),y)
	install -m 0755 dnscrypt-update-resolvers.sh $(INSTALLDIR)/usr/sbin
endif

# stubby resolver updater
ifeq ($(RTCONFIG_STUBBY),y)
	install -m 0755 stubby-update-resolvers.sh $(INSTALLDIR)/usr/sbin
endif

# ipv6 hosts updater
ifeq ($(RTCONFIG_DNSMASQ),y)
	install -m 0755 v6hosts.sh $(INSTALLDIR)/usr/sbin
	install -m 0755 updatehosts.sh $(INSTALLDIR)/usr/sbin
endif

# acme.sh command line utility/cert renew
	install -m 0755 install-acme.sh $(INSTALLDIR)/usr/sbin
	install -m 0755 renew-acme.sh $(INSTALLDIR)/usr/sbin
	install -m 0755 dns_asusapi.sh $(INSTALLDIR)/usr/sbin

# Utility to prune logs
	install -m 0755 check-log $(INSTALLDIR)/usr/sbin

# Utilites to move certs
	install -m 0755 sshd2nvram $(INSTALLDIR)/usr/sbin
	install -m 0755 sshd2jffs $(INSTALLDIR)/usr/sbin
	install -m 0755 ovpn2nvram $(INSTALLDIR)/usr/sbin
	install -m 0755 ovpn2jffs $(INSTALLDIR)/usr/sbin
	install -m 0755 https2nvram $(INSTALLDIR)/usr/sbin
	install -m 0755 https2jffs $(INSTALLDIR)/usr/sbin

# Easy-rsa scripts (for OpenVPN)
ifeq ($(RTCONFIG_OPENVPN),y)
	install -m 0700 setuprsa.sh $(INSTALLDIR)/usr/sbin
	install -m 0700 -D updown.sh $(INSTALLDIR)/usr/sbin
	install -m 0700 -D vpnrouting.sh $(INSTALLDIR)/usr/sbin
	install -m 0700 -D ovpnauth.sh $(INSTALLDIR)/usr/sbin
endif

# Get external ip via stun
	install -m 0755 getextip.sh $(INSTALLDIR)/usr/sbin
	ln -sf getextip.sh $(INSTALLDIR)/usr/sbin/gettunnelip.sh

# Postconf helper script
	install -m 0755 helper.sh $(INSTALLDIR)/usr/sbin

# amtm script
	install -m 0700 amtm $(INSTALLDIR)/usr/sbin

# Conntrack connection counts script
	install -m 0755 conntrkcnt.sh $(INSTALLDIR)/usr/sbin

ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN11P),y)
# for read/write Ralink PHY registers
        install -D mii_mgr $(INSTALLDIR)/bin/mii_mgr
endif
clean:
