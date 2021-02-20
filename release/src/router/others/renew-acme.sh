#!/bin/sh

# Auto update of LetsEncrypt certs
/jffs/acme.sh/acme.sh --cron --home /jffs/acme.sh | tee /tmp/acme.log | logger -t "ACME"

# Add additional cert post-processing here

exit
