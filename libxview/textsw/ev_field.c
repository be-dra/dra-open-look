#ifndef lint
char     ev_field_c_sccsid[] = "@(#)ev_field.c 20.15 93/06/28 DRA: $Id: ev_field.c,v 4.1 2024/03/28 19:06:00 dra Exp $";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Procedure for doing field matching.
 */

#include <xview_private/primal.h>
#include <xview_private/txt_impl.h>
#include <xview_private/ev_impl.h>

#define BUF_SIZE        256

Pkg_private Es_index ev_match_field_in_esh(Es_handle esh, CHAR *symbol1, unsigned symbol1_len, CHAR *symbol2, unsigned symbol2_len, Es_index start_pattern, unsigned direction)
{

    int             match_done = 0;
    Es_index        pos, new_pos;
    int             buf_size = BUF_SIZE;
    CHAR            buf[BUF_SIZE];
    int             read;
    Es_index        result_pos = ES_CANNOT_SET;
    int             count = 0;
    int             buf_index, i, j, k, l;

    pos = new_pos = ((direction & EV_FIND_BACKWARD) ? --start_pattern : start_pattern);

    if (direction & EV_FIND_BACKWARD) {
	struct es_buf_object esbuf;
	esbuf.esh = esh;
	esbuf.buf = buf;
	esbuf.sizeof_buf = BUF_SIZE;
	esbuf.first = ES_INFINITY;

	while (!match_done) {
	    if (pos < 0)
		goto Return;
	    if (es_make_buf_include_index(&esbuf,
					  pos, BUF_SIZE - 1)) {
		Es_index        length = es_get_length(esh);

		if (esbuf.first == ES_CANNOT_SET)
		    goto Return;

		if (pos < length)
		    goto Return;

	    } else {
		new_pos = esbuf.first;
		read = esbuf.last_plus_one - new_pos;
		buf_index = pos - new_pos;
		if (read <= 0) {
		    goto Return;
		} else {
		    FOREVER {
			if (symbol1[symbol1_len - 1] == buf[buf_index]) {
			    k = symbol1_len - 2;
			    l = buf_index - 1;
			    while ((k >= 0) && (symbol1[k] == buf[l])) {
				k--;
				l--;
			    }
			    if (k < 0) {
				count++;
				buf_index = l;
			    } else {
				buf_index--;
			    }
			} else if (symbol2[symbol2_len - 1] == buf[buf_index]) {
			    i = symbol2_len - 2;
			    j = buf_index - 1;
			    while ((i >= 0) && (symbol2[i] == buf[j])) {
				i--;
				j--;
			    }
			    if (i < 0) {
				if (--count == 0) {
				    result_pos = new_pos + j + 1;
				    match_done = 1;
				} else {
				    buf_index = j;
				}
			    } else {
				buf_index--;
			    }
			} else {
			    buf_index--;
			}
			if (match_done || (buf_index < 0)) {
			    pos = new_pos - 1;
			    break;
			}
		    }

		}

	    }
	}

    } else {
	while (!match_done) {
	    pos = new_pos;
	    es_set_position(esh, pos);
	    new_pos = es_read(esh, buf_size, buf, &read);
	    buf_index = 0;
	    if (READ_AT_EOF(pos, new_pos, read))
		goto Return;
	    if (read > 0) {
		FOREVER {
		    if (symbol1[0] == buf[buf_index]) {
			k = 1;
			l = buf_index + 1;
			while ((k < symbol1_len) && (symbol1[k] == buf[l])) {
			    k++;
			    l++;
			}
			if (k == symbol1_len) {
			    count++;
			    buf_index = l;
			} else {
			    buf_index++;
			}
		    } else if (symbol2[0] == buf[buf_index]) {
			i = 1;
			j = buf_index + 1;
			while ((i < symbol2_len) && (symbol2[i] == buf[j])) {
			    i++;
			    j++;
			}
			if (i == symbol2_len) {
			    if (--count == 0) {
				result_pos = pos + j;
				match_done = 1;
			    } else {
				buf_index = j;
			    }

			} else {
			    buf_index++;
			}

		    } else {
			buf_index++;
		    }
		    if (match_done || (buf_index == read))
			break;

		}
	    }
	}
    }

    return (result_pos);
Return:
    return (result_pos);
}

Pkg_private Es_index ev_find_enclose_end_marker(Es_handle esh, CHAR *symbol1, unsigned symbol1_len, CHAR *symbol2, unsigned symbol2_len, Es_index start_pos)
{
    int             done = FALSE;
    CHAR            buf[BUF_SIZE];
    int             char_read = 0;
    Es_index        result_pos = ES_CANNOT_SET;
    Es_index        new_pos, pos = start_pos;


    while (!done && (pos >= 0) && (pos != ES_CANNOT_SET)) {
	es_set_position(esh, pos);
	new_pos = es_read(esh, BUF_SIZE, buf, &char_read);

	if (READ_AT_EOF(pos, new_pos, char_read) || (char_read <= 0)) {
	    done = TRUE;
	} else {
	    CHAR           *buf_ptr = buf;
	    int             i;
	    int             keep_looping = TRUE;


	    for (i = 0; (keep_looping); char_read--, i++) {
		if (STRNCMP(buf_ptr, symbol1, (size_t)symbol1_len) == 0) {
		    done = TRUE;
		    keep_looping = FALSE;
		    result_pos = pos + i + symbol1_len;
		} else if (STRNCMP(buf_ptr, symbol2, (size_t)symbol2_len) == 0) {
		    keep_looping = FALSE;
		    pos = ev_match_field_in_esh(esh, symbol2, symbol2_len, symbol1, symbol1_len,
						pos + i, EV_FIND_DEFAULT);
		} else {
		    if (char_read > 0)
			buf_ptr++;
		    else {
			pos = new_pos;
			break;
		    }
		}
	    }
	}
    }

    return (result_pos);
}

#undef BUF_SIZE
