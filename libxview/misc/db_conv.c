#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)db_conv.c 50.14 93/06/28 DRA: RCS $Id: db_conv.c,v 4.2 2024/05/03 11:03:37 dra Exp $ ";
#endif
#endif

#include <xview_private/i18n_impl.h>
#include <xview/xview.h>

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#ifdef OW_I18N
Pkg_private int db_cvt_string_to_wcs(char *from_value, Attr_attribute *to_value)
{
    static wchar_t *cvtbufptr = NULL;
    static int	buflen = 0;
    int		newlen, status;

    if (buflen < (newlen=strlen(from_value)+1)) {
        if (cvtbufptr != NULL) {
	    xv_free (cvtbufptr);
	}
	cvtbufptr = (wchar_t *)xv_calloc(1, newlen*sizeof(wchar_t));
	buflen = newlen;
    }

    status = mbstowcs(cvtbufptr, from_value, newlen);
    *to_value = (Attr_attribute) cvtbufptr;
    return ((status == -1) || (status > newlen-1) ? XV_ERROR : XV_OK);
}
#endif /* OW_I18N */

Pkg_private int db_cvt_string_to_long(char *,Attr_attribute *);

Pkg_private int db_cvt_string_to_long(char *from_value,Attr_attribute *to_value)
{
    char  	*ptr; 

    *to_value = (Attr_attribute)strtol(from_value, &ptr, 10);
    return (((ptr == from_value) || (*ptr != '\0')) ? XV_ERROR : XV_OK);
}

Pkg_private int db_cvt_string_to_int(char *, Attr_attribute *);

Pkg_private int db_cvt_string_to_int(char *from_value, Attr_attribute *to_value)
{
    char  	*ptr; 
    int		tmp;

    tmp = (int)strtol(from_value, &ptr, 10);
    *to_value = (Attr_attribute)tmp;
    return(((ptr == from_value) || (*ptr != '\0')) ? XV_ERROR : XV_OK);
}


Pkg_private int db_cvt_string_to_bool(char *,Attr_attribute *);
Pkg_private int db_cvt_string_to_char(char *,Attr_attribute *);

Pkg_private int db_cvt_string_to_bool(char *from_value,Attr_attribute *to_value)
{
#define DB_BOOL_VALUES	16
	static char *db_bool_table[DB_BOOL_VALUES] = {
		"true", "false",
		"yes", "no",
		"on", "off",
		"enabled", "disabled",
		"set", "reset",
		"set", "cleared",
		"activated", "deactivated",
		"1", "0",
	};
	int i;
	char chr1, chr2;
	char *symbol1, *symbol2;

	for (i = 0; i < DB_BOOL_VALUES; i++) {
		symbol1 = *(db_bool_table + i);
		symbol2 = from_value;
		while ((chr1 = *symbol1++) != '\0') {
			chr2 = *symbol2++;
			if (('A' <= chr2) && (chr2 <= 'Z'))
				chr2 += 'a' - 'A';
			if (chr1 != chr2)
				break;
		}
		if (chr1 == '\0') {
			*to_value = (i % 2) ? False : True;
			return (XV_OK);
		}
	}
	return (XV_ERROR);
}


Pkg_private int db_cvt_string_to_char(char *from_value,Attr_attribute *to_value)
{
    *to_value = from_value[0];
    return(XV_OK);
}
