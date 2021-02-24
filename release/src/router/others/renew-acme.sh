#!/bin/sh

# Auto update of LetsEncrypt certs
/jffs/acme.sh/acme.sh --cron --home /jffs/acme.sh >/tmp/acme.log

# Add additional cert post-processing here

exit
