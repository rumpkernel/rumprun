/*	$NetBSD: content-bozo.c,v 1.8 2013/07/11 07:44:19 mrg Exp $	*/

/*	$eterna: content-bozo.c,v 1.17 2011/11/18 09:21:15 mrg Exp $	*/

/*
 * Copyright (c) 1997-2013 Matthew R. Green
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

/* this code implements content-type handling for bozohttpd */

#include <sys/param.h>

#include <errno.h>
#include <string.h>

#include "bozohttpd.h"

/*
 * this map and the functions below map between filenames and the
 * content type and content encoding definitions.  this should become
 * a configuration file, perhaps like apache's mime.types (but that
 * has less info per-entry).
 */

static bozo_content_map_t static_content_map[] = {
	{ ".html",	5, "text/html",			"",		"", NULL },
	{ ".htm",	4, "text/html",			"",		"", NULL },
	{ ".gif",	4, "image/gif",			"",		"", NULL },
	{ ".jpeg",	5, "image/jpeg",		"",		"", NULL },
	{ ".jpg",	4, "image/jpeg",		"",		"", NULL },
	{ ".jpe",	4, "image/jpeg",		"",		"", NULL },
	{ ".png",	4, "image/png",			"",		"", NULL },
	{ ".mp3",	4, "audio/mpeg",		"",		"", NULL },
	{ ".css",	4, "text/css",			"",		"", NULL },
	{ ".txt",	4, "text/plain",		"",		"", NULL },
	{ ".swf",	4, "application/x-shockwave-flash","",		"", NULL },
	{ ".dcr",	4, "application/x-director",	"",		"", NULL },
	{ ".pac",	4, "application/x-ns-proxy-autoconfig", "",	"", NULL },
	{ ".pa",	3, "application/x-ns-proxy-autoconfig", "",	"", NULL },
	{ ".tar",	4, "multipart/x-tar",		"",		"", NULL },
	{ ".gtar",	5, "multipart/x-gtar",		"",		"", NULL },
	{ ".tar.Z",	6, "multipart/x-tar",		"x-compress",	"compress", NULL },
	{ ".tar.gz",	7, "multipart/x-tar",		"x-gzip",	"gzip", NULL },
	{ ".taz",	4, "multipart/x-tar",		"x-gzip",	"gzip", NULL },
	{ ".tgz",	4, "multipart/x-tar",		"x-gzip",	"gzip", NULL },
	{ ".tar.z",	6, "multipart/x-tar",		"x-pack",	"x-pack", NULL },
	{ ".Z",		2, "application/x-compress",	"x-compress",	"compress", NULL },
	{ ".gz",	3, "application/x-gzip",	"x-gzip",	"gzip", NULL },
	{ ".z",		2, "unknown",			"x-pack",	"x-pack", NULL },
	{ ".bz2",	4, "application/x-bzip2",	"x-bzip2",	"x-bzip2", NULL },
	{ ".ogg",	4, "application/x-ogg",		"",		"", NULL },
	{ ".mkv",	4, "video/x-matroska",		"",		"", NULL },
	{ ".xbel",	5, "text/xml",			"",		"", NULL },
	{ ".xml",	4, "text/xml",			"",		"", NULL },
	{ ".xsl",	4, "text/xml",			"",		"", NULL },
	{ ".hqx",	4, "application/mac-binhex40",	"",		"", NULL },
	{ ".cpt",	4, "application/mac-compactpro","",		"", NULL },
	{ ".doc",	4, "application/msword",	"",		"", NULL },
	{ ".bin",	4, "application/octet-stream",	"",		"", NULL },
	{ ".dms",	4, "application/octet-stream",	"",		"", NULL },
	{ ".lha",	4, "application/octet-stream",	"",		"", NULL },
	{ ".lzh",	4, "application/octet-stream",	"",		"", NULL },
	{ ".exe",	4, "application/octet-stream",	"",		"", NULL },
	{ ".class",	6, "application/octet-stream",	"",		"", NULL },
	{ ".oda",	4, "application/oda",		"",		"", NULL },
	{ ".pdf",	4, "application/pdf",		"",		"", NULL },
	{ ".ai",	3, "application/postscript",	"",		"", NULL },
	{ ".eps",	4, "application/postscript",	"",		"", NULL },
	{ ".ps",	3, "application/postscript",	"",		"", NULL },
	{ ".ppt",	4, "application/powerpoint",	"",		"", NULL },
	{ ".rtf",	4, "application/rtf",		"",		"", NULL },
	{ ".bcpio",	6, "application/x-bcpio",	"",		"", NULL },
	{ ".torrent",	8, "application/x-bittorrent",	"",		"", NULL },
	{ ".vcd",	4, "application/x-cdlink",	"",		"", NULL },
	{ ".cpio",	5, "application/x-cpio",	"",		"", NULL },
	{ ".csh",	4, "application/x-csh",		"",		"", NULL },
	{ ".dir",	4, "application/x-director",	"",		"", NULL },
	{ ".dxr",	4, "application/x-director",	"",		"", NULL },
	{ ".dvi",	4, "application/x-dvi",		"",		"", NULL },
	{ ".hdf",	4, "application/x-hdf",		"",		"", NULL },
	{ ".cgi",	4, "application/x-httpd-cgi",	"",		"", NULL },
	{ ".skp",	4, "application/x-koan",	"",		"", NULL },
	{ ".skd",	4, "application/x-koan",	"",		"", NULL },
	{ ".skt",	4, "application/x-koan",	"",		"", NULL },
	{ ".skm",	4, "application/x-koan",	"",		"", NULL },
	{ ".latex",	6, "application/x-latex",	"",		"", NULL },
	{ ".mif",	4, "application/x-mif",		"",		"", NULL },
	{ ".nc",	3, "application/x-netcdf",	"",		"", NULL },
	{ ".cdf",	4, "application/x-netcdf",	"",		"", NULL },
	{ ".patch",	6, "application/x-patch",	"",		"", NULL },
	{ ".sh",	3, "application/x-sh",		"",		"", NULL },
	{ ".shar",	5, "application/x-shar",	"",		"", NULL },
	{ ".sit",	4, "application/x-stuffit",	"",		"", NULL },
	{ ".sv4cpio",	8, "application/x-sv4cpio",	"",		"", NULL },
	{ ".sv4crc",	7, "application/x-sv4crc",	"",		"", NULL },
	{ ".tar",	4, "application/x-tar",		"",		"", NULL },
	{ ".tcl",	4, "application/x-tcl",		"",		"", NULL },
	{ ".tex",	4, "application/x-tex",		"",		"", NULL },
	{ ".texinfo",	8, "application/x-texinfo",	"",		"", NULL },
	{ ".texi",	5, "application/x-texinfo",	"",		"", NULL },
	{ ".t",		2, "application/x-troff",	"",		"", NULL },
	{ ".tr",	3, "application/x-troff",	"",		"", NULL },
	{ ".roff",	5, "application/x-troff",	"",		"", NULL },
	{ ".man",	4, "application/x-troff-man",	"",		"", NULL },
	{ ".me",	3, "application/x-troff-me",	"",		"", NULL },
	{ ".ms",	3, "application/x-troff-ms",	"",		"", NULL },
	{ ".ustar",	6, "application/x-ustar",	"",		"", NULL },
	{ ".src",	4, "application/x-wais-source",	"",		"", NULL },
	{ ".zip",	4, "application/zip",		"",		"", NULL },
	{ ".au",	3, "audio/basic",		"",		"", NULL },
	{ ".snd",	4, "audio/basic",		"",		"", NULL },
	{ ".mpga",	5, "audio/mpeg",		"",		"", NULL },
	{ ".mp2",	4, "audio/mpeg",		"",		"", NULL },
	{ ".aif",	4, "audio/x-aiff",		"",		"", NULL },
	{ ".aiff",	5, "audio/x-aiff",		"",		"", NULL },
	{ ".aifc",	5, "audio/x-aiff",		"",		"", NULL },
	{ ".ram",	4, "audio/x-pn-realaudio",	"",		"", NULL },
	{ ".rpm",	4, "audio/x-pn-realaudio-plugin","",		"", NULL },
	{ ".ra",	3, "audio/x-realaudio",		"",		"", NULL },
	{ ".wav",	4, "audio/x-wav",		"",		"", NULL },
	{ ".pdb",	4, "chemical/x-pdb",		"",		"", NULL },
	{ ".xyz",	4, "chemical/x-pdb",		"",		"", NULL },
	{ ".ief",	4, "image/ief",			"",		"", NULL },
	{ ".tiff",	5, "image/tiff",		"",		"", NULL },
	{ ".tif",	4, "image/tiff",		"",		"", NULL },
	{ ".ras",	4, "image/x-cmu-raster",	"",		"", NULL },
	{ ".pnm",	4, "image/x-portable-anymap",	"",		"", NULL },
	{ ".pbm",	4, "image/x-portable-bitmap",	"",		"", NULL },
	{ ".pgm",	4, "image/x-portable-graymap",	"",		"", NULL },
	{ ".ppm",	4, "image/x-portable-pixmap",	"",		"", NULL },
	{ ".rgb",	4, "image/x-rgb",		"",		"", NULL },
	{ ".xbm",	4, "image/x-xbitmap",		"",		"", NULL },
	{ ".xpm",	4, "image/x-xpixmap",		"",		"", NULL },
	{ ".xwd",	4, "image/x-xwindowdump",	"",		"", NULL },
	{ ".rtx",	4, "text/richtext",		"",		"", NULL },
	{ ".tsv",	4, "text/tab-separated-values",	"",		"", NULL },
	{ ".etx",	4, "text/x-setext",		"",		"", NULL },
	{ ".sgml",	5, "text/x-sgml",		"",		"", NULL },
	{ ".sgm",	4, "text/x-sgml",		"",		"", NULL },
	{ ".mpeg",	5, "video/mpeg",		"",		"", NULL },
	{ ".mpg",	4, "video/mpeg",		"",		"", NULL },
	{ ".mpe",	4, "video/mpeg",		"",		"", NULL },
	{ ".mp4",	4, "video/mp4",			"",		"", NULL },
	{ ".qt",	3, "video/quicktime",		"",		"", NULL },
	{ ".mov",	4, "video/quicktime",		"",		"", NULL },
	{ ".avi",	4, "video/x-msvideo",		"",		"", NULL },
	{ ".movie",	6, "video/x-sgi-movie",		"",		"", NULL },
	{ ".ice",	4, "x-conference/x-cooltalk",	"",		"", NULL },
	{ ".wrl",	4, "x-world/x-vrml",		"",		"", NULL },
	{ ".vrml",	5, "x-world/x-vrml",		"",		"", NULL },
	{ NULL,		0, NULL,		NULL,		NULL, NULL }
};

static bozo_content_map_t *
search_map(bozo_content_map_t *map, const char *name, size_t len)
{
	for ( ; map && map->name; map++) {
		if (map->namelen < len &&
		    strcasecmp(map->name, name + (len - map->namelen)) == 0)
			return map;
	}
	return NULL;
}

/* match a suffix on a file - dynamiconly means no static content search */
bozo_content_map_t *
bozo_match_content_map(bozohttpd_t *httpd, const char *name,
			const int dynamiconly)
{
	bozo_content_map_t	*map;
	size_t			 len;

	len = strlen(name);
	if ((map = search_map(httpd->dynamic_content_map, name, len)) != NULL) {
		return map;
	}
	if (!dynamiconly) {
		if ((map = search_map(static_content_map, name, len)) != NULL) {
			return map;
		}
	}
	return NULL;
}

/*
 * given the file name, return a valid Content-Type: value.
 */
/* ARGSUSED */
const char *
bozo_content_type(bozo_httpreq_t *request, const char *file)
{
	bozohttpd_t *httpd = request->hr_httpd;
	bozo_content_map_t	*map;

	map = bozo_match_content_map(httpd, file, 0);
	if (map)
		return map->type;
	return httpd->consts.text_plain;
}

/*
 * given the file name, return a valid Content-Encoding: value.
 */
const char *
bozo_content_encoding(bozo_httpreq_t *request, const char *file)
{
	bozohttpd_t *httpd = request->hr_httpd;
	bozo_content_map_t	*map;

	map = bozo_match_content_map(httpd, file, 0);
	if (map)
		return (request->hr_proto == httpd->consts.http_11) ?
		    map->encoding11 : map->encoding;
	return NULL;
}

#ifndef NO_DYNAMIC_CONTENT

bozo_content_map_t *
bozo_get_content_map(bozohttpd_t *httpd, const char *name)
{
	bozo_content_map_t	*map;

	if ((map = bozo_match_content_map(httpd, name, 1)) != NULL)
		return map;
	
	httpd->dynamic_content_map_size++;
	httpd->dynamic_content_map = bozorealloc(httpd,
		httpd->dynamic_content_map,
		(httpd->dynamic_content_map_size + 1) * sizeof *map);
	if (httpd->dynamic_content_map == NULL)
		bozo_err(httpd, 1, "out of memory allocating content map");
	map = &httpd->dynamic_content_map[httpd->dynamic_content_map_size];
	map->name = map->type = map->encoding = map->encoding11 =
		map->cgihandler = NULL;
	map->namelen = 0;
	map--;

	return map;
}

/*
 * mime content maps look like:
 *	".name type encoding encoding11"
 * where any of type, encoding or encoding11 a dash "-" means "".
 * eg the .gtar, .tar.Z from above  could be written like:
 *	".gtar multipart/x-gtar - -"
 *	".tar.Z multipart/x-tar x-compress compress"
 * or
 *	".gtar multipart/x-gtar"
 *	".tar.Z multipart/x-tar x-compress compress"
 * NOTE: we destroy 'arg'
 */
void
bozo_add_content_map_mime(bozohttpd_t *httpd, const char *cmap0,
		const char *cmap1, const char *cmap2, const char *cmap3)
{
	bozo_content_map_t *map;

	debug((httpd, DEBUG_FAT,
		"add_content_map: name %s type %s enc %s enc11 %s ",
		cmap0, cmap1, cmap2, cmap3));

	map = bozo_get_content_map(httpd, cmap0);
#define CHECKMAP(s)	(!s || ((s)[0] == '-' && (s)[1] == '\0') ? "" : (s))
	map->name = CHECKMAP(cmap0);
	map->namelen = strlen(map->name);
	map->type = CHECKMAP(cmap1);
	map->encoding = CHECKMAP(cmap2);
	map->encoding11 = CHECKMAP(cmap3);
#undef CHECKMAP
	map->cgihandler = NULL;
}
#endif /* NO_DYNAMIC_CONTENT */
