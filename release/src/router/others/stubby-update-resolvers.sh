#!/bin/sh

scr_name="$(basename $0)"

OLDIFS=$IFS
if [ "$1" == "default" ]; then
        echo "Setting default list of public stubby resolvers..."
        RESOLVERS_PATH="/rom"
        RESOLVERS_FILE="${RESOLVERS_PATH}/stubby-resolvers.csv"
        rm -f "$(nvram get stubby_csv)"
else

        RESOLVERS_PATH="/jffs/etc"
        RESOLVERS_FILE="${RESOLVERS_PATH}/stubby-resolvers.csv"
        RESOLVERS_FILE_TMP="${RESOLVERS_FILE}.tmp"
        RESOLVERS_URL="https://onedrive.live.com/download?cid=09332FC159A7E129&resid=9332FC159A7E129%214553&authkey=AJfTRCm21iM9XBY"	# stubby/stubby-resolvers.csv.txt

        mkdir -p "${RESOLVERS_PATH}"

        echo "Retrieving the list of public stubby resolvers..." | logger -s -t "$scr_name"
        /usr/sbin/curl -s -L -R "$RESOLVERS_URL" -o "$RESOLVERS_FILE_TMP"
        if [ $? -ne 0 ]; then
            echo "Download failed" | logger -s -t "$scr_name"
            exit 1
        fi
		mv -f "$RESOLVERS_FILE_TMP" "$RESOLVERS_FILE" > /dev/null 2>&1
fi

nvram set stubby_csv="$RESOLVERS_FILE"
nvram commit

sed -i '/^\"#/d' $RESOLVERS_FILE # exclude commented lines
resolvcnt=$(wc -l < $RESOLVERS_FILE)
let resolvcnt=$resolvcnt-1 # exclude header
echo "Stubby resolvers update complete ($resolvcnt entries)" | logger -s -t "$scr_name"

echo "Checking selected stubby resolvers..."

IFS="<"
resolverr=0
stubby_RESOLVERS=$(nvram get stubby_dns)
for ENTRY in $stubby_RESOLVERS
do
	stubby_IP=$(echo $ENTRY | cut -d ">" -f 2)
	if [ "x"$(cat "$RESOLVERS_FILE" | grep "$stubby_IP,") == "x" -a "$stubby_IP" != "" ]; then
		echo "A selected stubby resolver is no longer available!  Please update your selected servers." | logger -s -t "$scr_name"
		resolverr=1
		break
	fi
done
IFS=$OLDIFS
echo "Done"

if [ $resolverr -eq 1 ]; then
	exit 2
else
	exit 0
fi
