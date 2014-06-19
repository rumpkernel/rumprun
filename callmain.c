/*-
 * Copyright (c) 2014 Citrix.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <xen/xen.h>

extern int main(int argc, char **argv);

static void parseargs(void *cmdline, int *nargs, char **outarray) {
	char *p = cmdline;
	char *out = 0;
	int quote = -1; /* -1 means outside arg, 0 or '"' or '\'' inside */

	*nargs = 0;

	for (;;) {
		int ac = *p++;
		int rc = ac;
		if (ac == '\\')
			rc = *p++;
		if (!rc || ac==' ' || ac=='\n' || ac=='\t') {
			/* any kind of delimiter */
			if (!rc || quote==0) {
				/* ending an argument */
				if (out)
					*out++ = 0;
				quote = -1;
			}
			if (!rc)
				/* definitely quit then */
				break;
			if (quote<0)
				/* not in an argument now */
				continue;
		}
		if (quote<0) {
			/* starting an argument */
			if (outarray)
				outarray[*nargs] = out = p-1;
			(*nargs)++;
			quote = 0;
		}
		if (quote > 0 && ac == quote) {
			quote = 0;
			continue;
		}
		if (ac == '\'' || ac == '"') {
			quote = ac;
			continue;
		}
		if (out)
			*out++ = rc;
	}
}

int
__default_app_main(start_info_t *si)
{
	char argv0[] = "rumpuser-xen";
	int nargs;
	char **argv;

	parseargs(si->cmd_line, &nargs, 0);
	argv = malloc(sizeof(*argv) * (nargs+3));
	argv[0] = argv0;
	parseargs(si->cmd_line, &nargs, argv+1);
	argv[nargs+1] = 0;
	argv[nargs+2] = 0;

	return main(nargs+1, argv);
}

__weak_alias(app_main,__default_app_main)

/*
 * Local variables:
 *  c-file-style: "linux"
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
