#!/bin/sh

source /usr/sbin/helper.sh

PID=$$
SECS=1262278080

OPENSSL=/usr/sbin/openssl

if [ $(nvram get ntp_sync) = "1" ]
then
        DAYS=398
else
        DAYS=3653
fi

OPENSSLCNF="/etc/openssl.config.$PID"

cp -L /etc/ssl/openssl.cnf $OPENSSLCNF

NVCN=`nvram get https_crt_cn`
if [ "$NVCN" == "" ]; then
	NVCN=`nvram get lan_ipaddr`
fi

OLDIFS=$IFS
IFS=","
I=0

echo "$I.organizationName=O" >> $OPENSSLCNF
echo "$I.organizationName_value=$(uname -o)" >> $OPENSSLCNF

for CN in $NVCN; do
        echo "$I.commonName=CN" >> $OPENSSLCNF
        echo "$I.commonName_value=$CN" >> $OPENSSLCNF
		echo "$I.emailAddress=E" >> $OPENSSLCNF
		echo "$I.emailAddress_value=root@localhost" >> $OPENSSLCNF
        I=$(($I + 1))
done
IFS=$OLDIFS

pc_insert "[ CA_default ]" "copy_extensions = copy" $OPENSSLCNF
# Required extension
pc_insert "[ v3_ca ]" "extendedKeyUsage = serverAuth" $OPENSSLCNF
pc_insert "[ v3_ca ]" "subjectAltName = @alt_names" $OPENSSLCNF
#pc_insert "[ v3_ca ]" "certificatePolicies = 2.5.29.32.0" $OPENSSLCNF
pc_insert "[ v3_req ]" "subjectAltName = @alt_names" $OPENSSLCNF

# Complete SAN definitions
echo "[ alt_names ]" >> $OPENSSLCNF

I=0
# Add IPs to SAN
LANIP=`nvram get lan_ipaddr`
echo "IP.$I = $LANIP" >> $OPENSSLCNF
echo "DNS.$I = $LANIP" >> $OPENSSLCNF	# workaround for IE not supporting IP SAN
I=$(($I + 1))

# Add DNS names to SAN
SWMODE=$(nvram get sw_mode)
if [ $SWMODE -eq 1 ] # only add DUT_DOMAIN for router mode
then
	echo "DNS.$I = router.asus.com" >> $OPENSSLCNF
	I=$(($I + 1))
fi

# Add hostnames
LANDOMAIN=$(nvram get lan_domain)
LANHOSTNAME=$(nvram get lan_hostname)
COMPUTERNAME=$(nvram get computer_name)
if [ "$LANHOSTNAME" != "" ]
then
	echo "DNS.$I = $LANHOSTNAME" >> $OPENSSLCNF
	I=$(($I + 1))

	if [ "$LANDOMAIN" != "" ]
	then
		echo "DNS.$I = $LANHOSTNAME.$LANDOMAIN" >> $OPENSSLCNF
		I=$(($I + 1))
	fi
fi

if [ "$COMPUTERNAME" != "" ]
then
	echo "DNS.$I = $COMPUTERNAME" >> $OPENSSLCNF
	I=$(($I + 1))

	if [ "$LANDOMAIN" != "" ]
	then
		echo "DNS.$I = $COMPUTERNAME.$LANDOMAIN" >> $OPENSSLCNF
		I=$(($I + 1))
	fi
fi

# Add DDNS
DDNSHOSTNAME=$(nvram get ddns_hostname_x)
DDNSSERVER=$(nvram get ddns_server_x)
DDNSUSER=$(nvram get ddns_username_x)
if [ "$(nvram get ddns_enable_x)" == "1" -a "$DDNSSERVER" != "WWW.DNSOMATIC.COM" -a "$DDNSHOSTNAME" != "" ]
then
	if [ "$DDNSSERVER" == "WWW.NAMECHEAP.COM" -a "$DDNSUSER" != "" ]
	then
		echo "DNS.$I = $DDNSHOSTNAME.$DDNSUSER" >> $OPENSSLCNF
		I=$(($I + 1))
	else
		echo "DNS.$I = $DDNSHOSTNAME" >> $OPENSSLCNF
		I=$(($I + 1))
	fi
fi

# set working directory for httpd
cd /etc

# create the key and certificate request
#OPENSSL_CONF=$OPENSSLCNF openssl req -new -out /tmp/cert.csr -keyout /tmp/privkey.pem -newkey rsa:1024 -passout pass:password
# remove the passphrase from the key
#OPENSSL_CONF=$OPENSSLCNF openssl rsa -in /tmp/privkey.pem -out key.pem -passin pass:password
# convert the certificate request into a signed certificate
#OPENSSL_CONF=$OPENSSLCNF RANDFILE=/dev/urandom openssl x509 -in /tmp/cert.csr -out cert.pem -req -signkey key.pem -days 3653 -sha256

# create the key
$OPENSSL genpkey -out key.pem -algorithm rsa -pkeyopt rsa_keygen_bits:2048
# create certificate request and sign it
RANDFILE=/dev/urandom $OPENSSL req -new -x509 -key key.pem -sha256 -out cert.pem -days $DAYS -config $OPENSSLCNF

#	openssl x509 -in /etc/cert.pem -text -noout

# server.pem for WebDav SSL
cat key.pem cert.pem > server.pem

if [ "$(nvram get https_crt_save)" == "0" ]; then
	rm -f $OPENSSLCNF
fi
