#!/bin/sh

# Remove ACME shell script
# https://github.com/acmesh-official/acme.sh
# https://github.com/acmesh-official/acme.sh/archive/master.tar.gz

dal(){
        sed -i "/^# ${1} start/,/^# ${1} end/d;" ${2} >/dev/null 2>&1
} # delete added lines

installdir="/jffs/acme.sh"
fprofile="/jffs/configs/profile.add"
fcron="/jffs/scripts/services-start"

if [ -f ${installdir}/acme.sh ];
then
	echo "*****************"
	echo "Uninstall acme.sh"
	echo "*****************"
	echo "WARNING: Continuing will delete any already installed certs/keys"
	while true; do
		read -p "Do you wish to continue [Y/N]?" yn
		case $yn in
			[Yy]* ) break;;
			[Nn]* ) exit;;
			* ) echo "Please answer Y/N";;
		esac
	done
else
	echo "acme.sh does not appear to be installed - Exiting"
	exit 1
fi

echo "Removing cron job for auto cert updates"
cru l | grep -q \"LetsEncrypt\" && cru d \"LetsEncrypt\"
dal "acme.sh" ""${fcron}""
lines=$(wc -l "${fcron}" | awk '{print $1;}')
if [ $lines -eq 1 ];
then
	echo
	echo "${fcron} now appears to be unused"
	while true; do
		read -p "Do you wish to delete it [Y/N]?" yn
		case $yn in
			[Yy]* ) rm "${fcron}" >/dev/null 2>&1
					break
					;;
			[Nn]* ) break
					;;
			* ) echo "Please answer Y/N";;
		esac
	done
fi
rm /jffs/scripts/renew-acme.sh >/dev/null 2>&1

echo "Removing profile support for acme.sh"
dal "acme.sh" "${fprofile}"
lines=$(wc -l "${fprofile}" | awk '{print $1;}')
if [ $lines -eq 0 ];
then
	echo
	echo "${fprofile} now appears to be unused"
	while true; do
		read -p "Do you wish to delete it [Y/N]?" yn
		case $yn in
			[Yy]* ) rm "${fprofile}" >/dev/null 2>&1
					break
					;;
			[Nn]* ) break
					;;
			* ) echo "Please answer Y/N";;
		esac
	done
fi

echo
echo "Removing ${installdir}"
rm -rf ${installdir} >/dev/null 2>&1

echo
echo "Please reboot router to complete acme.sh uninstall"
echo

exit
