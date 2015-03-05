/*	$NetBSD: ssl-bozo.c,v 1.15 2012/03/14 23:47:19 joerg Exp $	*/

/*	$eterna: ssl-bozo.c,v 1.15 2011/11/18 09:21:15 mrg Exp $	*/

/*
 * Copyright (c) 1997-2011 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer and
 *    dedication in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* this code implements SSL for bozohttpd */

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "bozohttpd.h"

#ifndef NO_SSL_SUPPORT

#include <openssl/ssl.h>
#include <openssl/err.h>

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

/* this structure encapsulates the ssl info */
typedef struct sslinfo_t {
	SSL_CTX			*ssl_context;
	const SSL_METHOD	*ssl_method;
	SSL			*bozossl;
	char			*certificate_file;
	char			*privatekey_file;
} sslinfo_t;

/*
 * bozo_ssl_err
 *
 * bozo_ssl_err works just like bozo_err except in addition to printing
 * the error provided by the caller at the point of error it pops and
 * prints all errors from the SSL error queue.
 */
BOZO_PRINTFLIKE(3, 4) BOZO_DEAD static void
bozo_ssl_err(bozohttpd_t *httpd, int code, const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        if (httpd->logstderr || isatty(STDERR_FILENO)) {
                vfprintf(stderr, fmt, ap);
                fputs("\n", stderr);
        } else
                vsyslog(LOG_ERR, fmt, ap);
        va_end(ap);

	unsigned int sslcode = ERR_get_error();
	do {
		static const char sslfmt[] = "SSL Error: %s:%s:%s";

		if (httpd->logstderr || isatty(STDERR_FILENO)) {
			fprintf(stderr, sslfmt,
			    ERR_lib_error_string(sslcode),
			    ERR_func_error_string(sslcode),
			    ERR_reason_error_string(sslcode));
		} else {
			syslog(LOG_ERR, sslfmt,
			    ERR_lib_error_string(sslcode),
			    ERR_func_error_string(sslcode),
			    ERR_reason_error_string(sslcode));
		}
	} while (0 != (sslcode = ERR_get_error()));
	exit(code);
}

static BOZO_PRINTFLIKE(2, 0) int
bozo_ssl_printf(bozohttpd_t *httpd, const char * fmt, va_list ap)
{
	sslinfo_t	*sslinfo;
	char		*buf;
	int		 nbytes;

	sslinfo = httpd->sslinfo;
	/* XXX we need more elegant/proper handling of SSL_write return */
	if ((nbytes = vasprintf(&buf, fmt, ap)) != -1) 
		SSL_write(sslinfo->bozossl, buf, nbytes);

	free(buf);

	return nbytes;
}

static ssize_t
bozo_ssl_read(bozohttpd_t *httpd, int fd, void *buf, size_t nbytes)
{
	sslinfo_t	*sslinfo;
	ssize_t		 rbytes;

	USE_ARG(fd);
	sslinfo = httpd->sslinfo;
	/* XXX we need elegant/proper handling of SSL_read return */
	rbytes = (ssize_t)SSL_read(sslinfo->bozossl, buf, (int)nbytes);
	if (rbytes < 1) {
		if (SSL_get_error(sslinfo->bozossl, rbytes) ==
				SSL_ERROR_WANT_READ)
			bozo_warn(httpd, "SSL_ERROR_WANT_READ");
		else
			bozo_warn(httpd, "SSL_ERROR OTHER");
	}

	return rbytes;
}

static ssize_t
bozo_ssl_write(bozohttpd_t *httpd, int fd, const void *buf, size_t nbytes)
{
	sslinfo_t	*sslinfo;
	ssize_t		 wbytes;

	USE_ARG(fd);
	sslinfo = httpd->sslinfo;
	/* XXX we need elegant/proper handling of SSL_write return */
	wbytes = (ssize_t)SSL_write(sslinfo->bozossl, buf, (int)nbytes);

	return wbytes;
}

static int
bozo_ssl_flush(bozohttpd_t *httpd, FILE *fp)
{
	USE_ARG(httpd);
	USE_ARG(fp);
	/* nothing to see here, move right along */
	return 0;
}

void
bozo_ssl_init(bozohttpd_t *httpd)
{
	sslinfo_t	*sslinfo;

	sslinfo = httpd->sslinfo;
	if (sslinfo == NULL || !sslinfo->certificate_file)
		return;
	SSL_library_init();
	SSL_load_error_strings();

	sslinfo->ssl_method = SSLv23_server_method();
	sslinfo->ssl_context = SSL_CTX_new(sslinfo->ssl_method);

	/* XXX we need to learn how to check the SSL stack for more info */
	if (NULL == sslinfo->ssl_context)
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "SSL context creation failed");

	if (1 != SSL_CTX_use_certificate_chain_file(sslinfo->ssl_context,
	    sslinfo->certificate_file))
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "Unable to use certificate file '%s'",
		    sslinfo->certificate_file);

	if (1 != SSL_CTX_use_PrivateKey_file(sslinfo->ssl_context,
	    sslinfo->privatekey_file, SSL_FILETYPE_PEM))
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "Unable to use private key file '%s'",
		    sslinfo->privatekey_file);

	/* check consistency of key vs certificate */
	if (!SSL_CTX_check_private_key(sslinfo->ssl_context))
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "Check private key failed");
}

void
bozo_ssl_accept(bozohttpd_t *httpd)
{
	sslinfo_t	*sslinfo;

	sslinfo = httpd->sslinfo;
	if (sslinfo != NULL && sslinfo->ssl_context) {
		sslinfo->bozossl = SSL_new(sslinfo->ssl_context);
		SSL_set_rfd(sslinfo->bozossl, 0);
		SSL_set_wfd(sslinfo->bozossl, 1);
		SSL_accept(sslinfo->bozossl);
	}
}

void
bozo_ssl_destroy(bozohttpd_t *httpd)
{
	sslinfo_t	*sslinfo;

	sslinfo = httpd->sslinfo;
	if (sslinfo && sslinfo->bozossl)
		SSL_free(sslinfo->bozossl);
}

void
bozo_ssl_set_opts(bozohttpd_t *httpd, const char *cert, const char *priv)
{
	sslinfo_t	*sslinfo;

	if ((sslinfo = httpd->sslinfo) == NULL) {
		sslinfo = bozomalloc(httpd, sizeof(*sslinfo));
		if (sslinfo == NULL) {
			bozo_err(httpd, 1, "sslinfo allocation failed");
		}
		httpd->sslinfo = sslinfo;
	}
	sslinfo->certificate_file = strdup(cert);
	sslinfo->privatekey_file = strdup(priv);
	debug((httpd, DEBUG_NORMAL, "using cert/priv files: %s & %s",
		sslinfo->certificate_file,
		sslinfo->privatekey_file));
	if (!httpd->bindport) {
		httpd->bindport = strdup("https");
	}
}

#endif /* NO_SSL_SUPPORT */

int
bozo_printf(bozohttpd_t *httpd, const char *fmt, ...)
{
	va_list	args;
	int	cc;

	va_start(args, fmt);
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo) {
		cc = bozo_ssl_printf(httpd, fmt, args);
		va_end(args);
		return cc;
	}
#endif
	cc = vprintf(fmt, args);
	va_end(args);
	return cc;
}

ssize_t
bozo_read(bozohttpd_t *httpd, int fd, void *buf, size_t len)
{
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo) {
		return bozo_ssl_read(httpd, fd, buf, len);
	}
#endif
	return read(fd, buf, len);
}

ssize_t
bozo_write(bozohttpd_t *httpd, int fd, const void *buf, size_t len)
{
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo) {
		return bozo_ssl_write(httpd, fd, buf, len);
	}
#endif
	return write(fd, buf, len);
}

int
bozo_flush(bozohttpd_t *httpd, FILE *fp)
{
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo) {
		return bozo_ssl_flush(httpd, fp);
	}
#endif
	return fflush(fp);
}
