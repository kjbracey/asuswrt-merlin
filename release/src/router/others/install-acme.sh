#!/bin/sh

# Install ACME shell script
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
	echo "acme.sh is already installed."
	echo "WARNING: Continuing will delete any already installed certs/keys"
	while true; do
		read -p "Do you wish to continue [Y/N]?" yn
		case $yn in
			[Yy]* ) break;;
			[Nn]* ) exit;;
			* ) echo "Please answer Y/N";;
		esac
	done
fi

echo "Downloading from https://github.com/acmesh-official/acme.sh"
cd /jffs
curl -s -L -O https://github.com/acmesh-official/acme.sh/archive/master.tar.gz
tar -xzf master.tar.gz && rm master.tar.gz

echo "Installing acme.sh to ${installdir}"
rm -rf ${installdir}
cd /jffs/acme.sh-master
./acme.sh --install --home ${installdir} >/dev/null 2>&1
cp /usr/sbin/dns_asusapi.sh /jffs/acme.sh/dnsapi
cd ~
rm -rf /jffs/acme.sh-master

echo "Updating profile for acme.sh"
dal "acme.sh" "${fprofile}"
echo "# acme.sh start" >>"${fprofile}"
cat "${installdir}/acme.sh.env" >>"${fprofile}"
echo "# acme.sh end" >>"${fprofile}"

echo "Installing cron job for auto cert updates"
cp /usr/sbin/renew-acme.sh /jffs/scripts/renew-acme.sh
[ -f "${fcron}" ] || echo "#!/bin/sh" >"${fcron}"
dal "acme.sh" ""${fcron}""
eval $(date +Y=%Y\;m=%m\;d=%d\;H=%H\;M=%M)
cat "${fcron}" | tail -n 1 | grep -q "^exit"
if [ $? -eq 0 ]; then sed -i "$ d" "${fcron}"; fi
echo "# acme.sh start" >>"${fcron}"
echo "cru l | grep -q \"LetsEncrypt\" && cru d \"LetsEncrypt\"" >>"${fcron}"
echo "cru a LetsEncrypt \"$M 0 * * * /jffs/scripts/renew-acme.sh\"" >>"${fcron}"
echo "# acme.sh end" >>"${fcron}"

echo
echo "Please reboot router to complete installation"
echo

exit
