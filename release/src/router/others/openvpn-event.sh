#!/bin/sh

scr_name="$(basename $0)[$$]"

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
