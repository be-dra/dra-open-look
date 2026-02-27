#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)filter.c 20.28 93/06/28 DRA: $Id: filter.c,v 2.4 2026/02/26 17:55:54 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include <ctype.h>
#include <string.h>
#include <xview_private/io_stream.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <xview/str_utils.h>
#include <xview/sun.h>
#include <xview/xv_error.h>
#include <xview_private/filter.h>
/*
 * This module parses a file which specifies the mapping from key stations to
 * filters to be invoked from the editor. The form of each entry in the file
 * is: key_station filter_name program_name arguments
 * 
 * where key_station is either Cnnn, or identifier(nn), e.g. L1, KEY_LEFT(1).
 * 
 * Each entry must be separated from the next by a blank line. Comments may
 * appear in the file, as indicated by either the appearance of a '#' at the
 * beginning of a line, or the C convention using /'s and *'s.
 * 
 * The parser returns an array of pointers to structures (filter_rec) containing
 * the key name, the integer, the filter name, and an argc-argv array
 * describing the program call.
 * 
 * Unresolved issues:  reporting error positions.
 */

char *stream_fgets(char *, int, STREAM *);

static int any_shell_meta(char *);
static enum CharClass breakProc(char);
static struct CharAction digits(char);


struct filter_rec **xv_parse_filter_table(STREAM *in, char *filename)	/* use this to label errors */
{
	struct filter_rec *rec_array[50];
	char *arg_array[20];
	STREAM *scratch_stream = NULL;
	char scratch[256], scratch1[32];
	int keynum, i, j, k;
	struct filter_rec *rec;
	struct filter_rec **val;
	struct posrec pos;

	for (i = 0;; i++) {
		if (stream_get_token(in, scratch, breakProc) == NULL)
			break;
		rec = (struct filter_rec *)xv_malloc(sizeof(struct filter_rec));
		if (rec == NULL) {
			char dummy[128];

			(void)sprintf(dummy,
					XV_MSG("while parsing filter file %s"), filename);
			xv_error(XV_NULL,
					ERROR_LAYER, ERROR_SYSTEM, ERROR_STRING, dummy, NULL);
			return ((struct filter_rec **)NULL);
		}
		rec->key_name = xv_strsave(scratch);
		if (stream_get_sequence(in, scratch, digits) != NULL)
			keynum = atoi(scratch);
		else if (!strequal(stream_get_token(in, scratch, breakProc), ")")) {
			(void)stream_get_sequence(in, scratch, digits);
			keynum = atoi(scratch);
			if (!strequal(stream_get_token(in, scratch, breakProc), ")"))
				goto error;
		}
		else
			goto error;

		rec->key_num = keynum;
		(void)stream_get_token(in, scratch, xv_white_space);
		rec->class = xv_strsave(scratch);
		(void)stream_getc(in);	/* discard the newline */
		scratch_stream = string_input_stream(stream_fgets(scratch, 256, in),
				scratch_stream);

		/*
		 * So what if someone fails to specify a command after the key
		 * definition
		 */
		if (!strcmp(scratch, "\n")) {
			char dummy[128];

			(void)sprintf(dummy,
					XV_MSG("filter file %s: missing command-line"), filename);
			xv_error(XV_NULL, ERROR_STRING, dummy, NULL);

			pos = stream_get_pos(in);
			goto errorNoSkip;
		}
		/*
		 * storage for stream allocated first time it is needed. subsequent
		 * times, will be reused
		 */
		if (any_shell_meta(scratch)) {
			char *shell;

			if ((shell = getenv("SHELL")) == NULL)
				shell = "/bin/sh";
			rec->call = (char **)xv_calloc(4, (unsigned)sizeof(char *));
			rec->call[0] = shell;
			rec->call[1] = "-c";
			rec->call[2] = xv_strsave(scratch);
		}
		else {
			/* tokenize the call, copying results into arg_array */
			for (j = 0, k = 0;
					(stream_get_token(scratch_stream,
									scratch1, xv_white_space) != NULL); j++)
				arg_array[j] = xv_strsave(scratch1);
			/*
			 * allocate a new array of appropriate size (j+1 so client will
			 * know where the array ends)
			 */
			rec->call = (char **)xv_calloc((unsigned)j + 1,
					(unsigned)sizeof(char *));
			/* and copy the strings from arg_array into it */
			for (k = 0; k < j; k++)
				rec->call[k] = arg_array[k];
		}
		rec_array[i] = rec;
		continue;
	  error:
		pos = stream_get_pos(in);
		for (; (stream_fgets(scratch, 256, in) != NULL);)
			/* skip characters until you see a blank line */
			if (scratch[0] == '\n')
				break;
		{
			char dummy[128];

			(void)sprintf(dummy,
					XV_MSG("problem parsing filter file %s"), filename);
			xv_error(XV_NULL, ERROR_STRING, dummy, NULL);
		}
	  errorNoSkip:
		{
			char dummy[128];

			if (pos.lineno != -1)
				(void)sprintf(dummy,
						XV_MSG("problem on line number %d"), pos.lineno);
			else
				(void)sprintf(dummy,
						XV_MSG("problem near character position %d"),
						pos.charpos);
			i--;
			xv_error(XV_NULL, ERROR_STRING, dummy, NULL);
		}
	}
	val = (struct filter_rec **)xv_calloc((unsigned)i + 1,
			(unsigned)sizeof(struct filter_rec *));
	if (val == NULL) {
		char dummy[128];

		(void)sprintf(dummy, XV_MSG("while parsing filter file %s"), filename);
		xv_error(XV_NULL, ERROR_LAYER, ERROR_SYSTEM, ERROR_STRING, dummy, NULL);
		return ((struct filter_rec **)NULL);
	}
	for (j = 0; j < i; j++)
		val[j] = rec_array[j];

	if (scratch_stream) {
		if (scratch_stream->client_data) xv_free(scratch_stream->client_data);
		xv_free((char *)scratch_stream);
	}
	return (val);
}

void xv_free_filter_table(struct filter_rec **table)
{
	int i;

	for (i = 0; table[i] != NULL; i++) {
		free((char *)table[i]->call);
		if (table[i]->key_name) xv_free(table[i]->key_name);
		if (table[i]->class) xv_free(table[i]->class);
		free((char *)table[i]);
	}
	free((char *)table);
}

static struct CharAction digits(char c)
{
    struct CharAction val;

    if (isdigit(c)) {
	val.include = True;
	val.stop = False;
    } else {
	val.include = False;
	val.stop = True;
    }
    return (val);
}


static enum CharClass breakProc(char c)
{
    switch (c) {
      case ' ':
      case '\n':
      case '\t':
	return (Sepr);
      case '(':
      case ')':
	return (Break);
      default:
	return (isdigit(c) ? Break : Other);
    }
}

/*
 * Are there any shell meta-characters in string s?
 */
static int any_shell_meta(char *s)
{

    while (*s) {
	if (XV_INDEX("~{[*?$`'\"\\", *s))
	    return (1);
	s++;
    }
    return (0);
}
