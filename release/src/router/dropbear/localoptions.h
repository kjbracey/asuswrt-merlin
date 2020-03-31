/*
                     > > > Read This < < <

default_options.h  documents compile-time options, and provides default values.

Local customisation should be added to localoptions.h which is
used if it exists. Options defined there will override any options in this
file.

Options can also be defined with -DDROPBEAR_XXX=[0,1] in Makefile CFLAGS

IMPORTANT: Some options will require "make clean" after changes */

/* Override SSH 2.0 ident */
#define LOCAL_IDENT "SSH-2.0-dropbear"

/* The default path. This will often get replaced by the shell */
#define DEFAULT_PATH "/bin:/sbin:/usr/bin:/usr/sbin:/opt/bin:/opt/sbin:/opt/usr/bin:/opt/usr/sbin"

/* if you want to enable running an sftp server (such as the one included with
 * OpenSSH), set the path below and set DROPBEAR_SFTPSERVER.
 * The sftp-server program is not provided by Dropbear itself */
#define DROPBEAR_SFTPSERVER 1
#define SFTPSERVER_PATH "/opt/libexec/sftp-server"

/* Enable X11 Forwarding - server only */
#define DROPBEAR_X11FWD 0

/* Set NON_INETD_MODE if you require daemon functionality (ie Dropbear listens
 * on chosen ports and keeps accepting connections. This is the default.
 *
 * Set INETD_MODE if you want to be able to run Dropbear with inetd (or
 * similar), where it will use stdin/stdout for connections, and each process
 * lasts for a single connection. Dropbear should be invoked with the -i flag
 * for inetd, and can only accept IPv4 connections.
 *
 * Both of these flags can be defined at once, don't compile without at least
 * one of them. */
#define NON_INETD_MODE 1
#define INETD_MODE 0

/* Message integrity. sha2-256 is recommended as a default,
   sha1 for compatibility */
#define DROPBEAR_SHA1_HMAC 1
#define DROPBEAR_SHA1_96_HMAC 0
#define DROPBEAR_SHA2_256_HMAC 1
#define DROPBEAR_SHA2_512_HMAC 1

/* Default maximum number of failed authentication tries (server option) */
/* -T server option overrides */
#define MAX_AUTH_TRIES 3
