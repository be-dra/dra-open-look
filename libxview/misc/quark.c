#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#) quark.c 50.11 93/06/28 DRA: RCS $Id: quark.c,v 4.2 2024/09/15 09:19:47 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

#include  <X11/X.h>
#include  <X11/Xlib.h>
#include  <X11/Xresource.h>
#include  <xview/xview.h>
#include  <xview/pkg.h>

Xv_private Xv_opaque db_name_from_qlist(XrmQuarkList qlist);
/* 
 *    Utilities to deal with quark lists and such.
 */
Xv_private Xv_opaque db_name_from_qlist(XrmQuarkList qlist)
{   
    register int    i;  

    if (qlist == NULL) return XV_NULL;
    
    for (i = 0; qlist[i] != NULLQUARK; i++) 
                ;
    if (i != 0) 
        return (Xv_opaque)XrmQuarkToString(qlist[i - 1]);
    else 
        return XV_NULL;
}

Xv_private XrmQuarkList db_qlist_from_name(char *name, XrmQuarkList parent_quarks);

Xv_private XrmQuarkList db_qlist_from_name(char *name, XrmQuarkList parent_quarks)
{
	register int i;
	unsigned num_quarks = 0;
	XrmQuarkList quarks;

	if (name == NULL)
		return NULL;

	if (parent_quarks != NULL) {
		for (; parent_quarks[num_quarks] != NULLQUARK; num_quarks++);
		quarks = (XrmQuarkList) xv_calloc(num_quarks + 2,
										(unsigned)sizeof(XrmQuark));
		for (i = 0; i < num_quarks; i++) quarks[i] = parent_quarks[i];
	}
	else {
		quarks = (XrmQuarkList) xv_calloc(2, (unsigned)sizeof(XrmQuark));
		i = 0;
	}

	quarks[i++] = XrmStringToQuark(name);
	quarks[i] = NULLQUARK;

	return quarks;
}


#ifdef OW_I18N
Pkg_private int db_cvt_string_to_wcs(char *, Attr_attribute *);
#endif
Pkg_private int db_cvt_string_to_long(char *,Attr_attribute *);
Pkg_private int db_cvt_string_to_int(char *, Attr_attribute *);
Pkg_private int db_cvt_string_to_bool(char *,Attr_attribute *);
Pkg_private int db_cvt_string_to_char(char *,Attr_attribute *);

static Attr_attribute resource_type_conv(char *str,
						Attr_base_cardinality xv_type, Attr_attribute def_val)
{
	Attr_attribute to_val;

	switch (xv_type) {
		case ATTR_LONG:
			db_cvt_string_to_long(str, &to_val);
			return (to_val);

		case ATTR_X:
		case ATTR_Y:
		case ATTR_INT:
			db_cvt_string_to_int(str, &to_val);
			return (to_val);

		case ATTR_BOOLEAN:
			db_cvt_string_to_bool(str, &to_val);
			return (to_val);

		case ATTR_CHAR:
			db_cvt_string_to_char(str, &to_val);
			return (to_val);

		case ATTR_STRING:
			to_val = (Xv_opaque) str;
			return (to_val);

#ifdef OW_I18N
		case ATTR_WSTRING:
			db_cvt_string_to_wcs(str, &to_val);
			return (to_val);
#endif

		default:
			return (def_val);
	}
}

Xv_private Attr_attribute db_get_data(XID db, XrmQuarkList instance_qlist,
							char *attr_name, Attr_attribute attr,
							Attr_attribute default_value);

Xv_private Attr_attribute db_get_data(XID db, XrmQuarkList instance_qlist,
							char *attr_name, Attr_attribute attr,
							Attr_attribute default_value)
{
	Attr_attribute result;
	XrmRepresentation quark_type;
	XrmValue value;
	XrmQuark *qlist;
	Attr_base_cardinality type;
	register int i = 0;
	unsigned num_quarks = 0;

	if (instance_qlist) {
		/*
		 * Figure out how many quarks in list
		 */
		for (num_quarks = 0; instance_qlist[num_quarks] != NULLQUARK;
				num_quarks++);

		/*
		 * Alloc quark array
		 * The additional two quarks - for attr_name and NULLQUARK
		 */
		qlist = (XrmQuark *) xv_calloc(num_quarks + 2, (unsigned)sizeof(XrmQuark));

		/*
		 * Copy quark array
		 */
		for (i = 0; instance_qlist[i] != NULLQUARK; i++)
			qlist[i] = instance_qlist[i];
	}
	else {
		/*
		 * If no instance_qlist, alloc quarks for attr_name, NULLQUARK
		 */
		qlist = (XrmQuark *) xv_calloc(2, (unsigned)sizeof(XrmQuark));
	}

	qlist[i++] = XrmStringToQuark(attr_name);
	qlist[i] = NULLQUARK;

	/*
	 * Get type of attribute
	 */
	type = ATTR_WHICH_TYPE(attr);

	if (XrmQGetResource((XrmDatabase)db, qlist, qlist, &quark_type, &value)) {
		result = resource_type_conv(value.addr, type, default_value);
	}
	else {
		result = default_value;
	}

	free((char *)qlist);

	return result;
}
