/*
 * This file is a product of Bernhard Drahota and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Bernhard Drahota to assist in its use, correction,
 * modification or enhancement.
 *
 * BERNHARD DRAHOTA SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Bernhard Drahota be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if B. Drahota has been advised of the possibility of such damages.
 */
#include <xview/regexpr.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdarg.h>

char regexpr_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: regexpr.c,v 1.7 2025/04/09 19:56:18 dra Exp $";

typedef struct _regexp_context {
	regex_t pattbuf;
} context_t;

const char *xv_compile_regexp(char *instring, xv_regexp_context *ctxt)
{
	static char errbuf[200];
	int status;
	context_t *cont;

	if (! *ctxt) {
		cont = (context_t *)calloc(1, sizeof(context_t));
		*ctxt = cont;
	}
	else cont = *ctxt;

	status = regcomp(&cont->pattbuf, instring, 0);
	if (! status) {
		return (const char *)0;
	}

	regerror(status, &cont->pattbuf, errbuf, sizeof(errbuf));

	return errbuf;
}

char *xv_match_regexp(char *string, xv_regexp_context ctxt, ...)
{
	int retval;
	unsigned i;
#define MAXMATCH 10
	regmatch_t matches[MAXMATCH];
	va_list ap;
	char *p;

	retval = regexec(&ctxt->pattbuf, string, MAXMATCH, matches, 0);

	if (retval != 0) return (char *)0;

	va_start(ap, ctxt);
	/* zum Index 0 gehoert der ganze Match */
	i = 1;
	while ((p = va_arg(ap,char*))) {
		if (i < MAXMATCH && matches[i].rm_so >= 0) {
			char *s, *t, *e;

			t = p - 1;
			s = string + matches[i].rm_so - 1;
			e = string + matches[i].rm_eo;
			while (s < e) *++t = *++s;
			*t = '\0';
		}
		else
			*p = '\0';

		i++;
	}
	va_end(ap);

	return string + matches[0].rm_so;
}

void xv_free_regexp(xv_regexp_context ctxt)
{
	if (ctxt) {
		regfree(&ctxt->pattbuf);
		free((char *)ctxt);
	}
}
