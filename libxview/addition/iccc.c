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
#include <unistd.h>
#include <xview/iccc.h>
#include <xview/server.h>
#include <xview/frame.h>
#include <xview/cms.h>
#include <xview_private/svr_impl.h>
#include <xview_private/i18n_impl.h>
#include <sys/utsname.h>			/* System V dependent */
#include <pwd.h>				/* System V dependent */

#ifndef lint
char iccc_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: iccc.c,v 4.11 2025/03/08 13:37:48 dra Exp $";
#endif

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]

#define ADONE ATTR_CONSUME(*attrs);break

typedef int (*convert_func)(Iccc,Atom *,Xv_opaque *, unsigned long *,int *);

typedef struct {
	Xv_opaque public_self;
	Xv_opaque srv;
	Atom *targets;
	unsigned num_targets_alloc, num_targets_used;
	convert_func appl_converter;
} Iccc_private;

#define ICCCPRIV(_x_) XV_PRIVATE(Iccc_private, Xv_iccc_public, _x_)
#define ICCCPUB(_x_) XV_PUBLIC(_x_)

static int convert_to_targets(Iccc_private *priv, Atom *type, Xv_opaque *value,
							unsigned long *length, int *format)
{
	*value = (Xv_opaque)priv->targets;
	*type = XA_ATOM;
	*length = priv->num_targets_used;
	*format = 32;
	return TRUE;
}

static int convert_to_font(Iccc_private *priv, Atom *type, Xv_opaque *value,
							unsigned long *length, int *format)
{
	static Font font;
	Xv_opaque xvfont = xv_get(xv_get(ICCCPUB(priv), XV_OWNER), XV_FONT);

	if (!xvfont) return FALSE;

	font = (Font)xv_get(xvfont, XV_XID);

	*type = XA_FONT;
	*format = 32;
	*length = 1;
	*value = (Xv_opaque)&font;
	return TRUE;
}

static int convert_to_foreground(Iccc_private *priv, Atom *type,
						Xv_opaque *value, unsigned long *length, int *format)
{
	static unsigned long pix;
	Xv_window win;

	win = xv_get(ICCCPUB(priv), XV_OWNER);
	pix = (unsigned long)xv_get(xv_get(win, WIN_CMS), CMS_PIXEL,
								(int)xv_get(win, WIN_FOREGROUND_COLOR));

	*type = (Atom)xv_get(priv->srv, SERVER_ATOM, "PIXEL");
	*format = 32;
	*length = 1;
	*value = (Xv_opaque)&pix;
	return TRUE;
}

static int convert_to_background(Iccc_private *priv, Atom *type,
				Xv_opaque *value, unsigned long *length, int *format)
{
	static unsigned long pix;
	Xv_window win;

	win = xv_get(ICCCPUB(priv), XV_OWNER);
	pix = (unsigned long)xv_get(xv_get(win, WIN_CMS), CMS_PIXEL,
								(int)xv_get(win, WIN_BACKGROUND_COLOR));

	*type = (Atom)xv_get(priv->srv, SERVER_ATOM, "PIXEL");
	*format = 32;
	*length = 1;
	*value = (Xv_opaque)&pix;
	return TRUE;
}


static int convert_to_colormap(Iccc_private *priv, Atom *type,
				Xv_opaque *value, unsigned long *length, int *format)
{
	static Colormap cm;

	cm = (Colormap)xv_get(xv_get(xv_get(ICCCPUB(priv), XV_OWNER), WIN_CMS),
															XV_XID);

	*type = XA_COLORMAP;
	*format = 32;
	*length = 1;
	*value = (Xv_opaque)&cm;
	return TRUE;
}

static int convert_to_name(Iccc_private *priv, Atom *type, Xv_opaque *value,
				unsigned long *length, int *format)
{
	Xv_window win = xv_get(ICCCPUB(priv), XV_OWNER);

	while (win && ! xv_get(win, XV_IS_SUBTYPE_OF, FRAME_BASE))
		win = xv_get(win, XV_OWNER);

	if (win) {
		char *title = (char *)xv_get(win, XV_LABEL);

		*type = XA_STRING;
		*format = 8;
		*length = strlen(title);
		*value = (Xv_opaque)title;
		return TRUE;
	}

	return FALSE;
}

#ifdef FY_NGHYFRINACH
static int convert_to_window(Iccc_private *priv, Atom *type, Xv_opaque *value,
				unsigned long *length, int *format)
{
	static Window win;

	win = (Window)xv_get(xv_get(ICCCPUB(priv), XV_OWNER), XV_XID);

	/* type = target !!! */
	*format = 32;
	*length = 1;
	*value = (Xv_opaque)&win;
	return TRUE;
}

static int convert_to_class(Iccc_private *priv, Atom *type, Xv_opaque *value,
				unsigned long *length, int *format)
{
	return FALSE;
}


static int convert_to_client_window(Iccc_private *priv, Atom *type,
				Xv_opaque *value, unsigned long *length, int *format)
{
	return FALSE;
}

#endif /* FY_NGHYFRINACH */

static int convert_sel_end(Iccc_private *priv, Atom *type, Xv_opaque *data,
				unsigned long *length, int *format)
{
	xv_set(ICCCPUB(priv), SEL_OWN, FALSE, NULL);
	*format = 32;
	*length = 0;
	*data = 0;
	*type = (Atom)xv_get(priv->srv, SERVER_ATOM, "NULL");
	return TRUE;
}

static int convert_sel_error(Iccc_private *priv, Atom *type, Xv_opaque *data,
				unsigned long *length, int *format)
{
	Frame fram;

	fram = xv_get(xv_get(ICCCPUB(priv), XV_OWNER), WIN_FRAME);
	if (fram && xv_get(fram, FRAME_SHOW_FOOTER)) {
		xv_set(fram,
				FRAME_LEFT_FOOTER, XV_MSG("data transfer failure"),
				WIN_ALARM,
				NULL);
	}

	return convert_sel_end(priv, type, data, length, format);
}

static int convert_dnd_reject(Iccc_private *priv, Atom *type, Xv_opaque *data,
				unsigned long *length, int *format)
{
	Frame fram;

	fram = xv_get(xv_get(ICCCPUB(priv), XV_OWNER), WIN_FRAME);
	if (fram && xv_get(fram, FRAME_SHOW_FOOTER)) {
		xv_set(fram,
				FRAME_LEFT_FOOTER, XV_MSG("drop rejected"),
				WIN_ALARM,
				NULL);
	}

	return convert_sel_end(priv, type, data, length, format);
}

static struct {
	char *name;
	int (*convert)(Iccc_private *,Atom *,Xv_opaque *, unsigned long *,int *);
} my_targets[] = {
	{ "TARGETS", convert_to_targets },
	{ "FOREGROUND", convert_to_foreground },
	{ "BACKGROUND", convert_to_background },
	{ "COLORMAP", convert_to_colormap },
	{ "_SUN_SELECTION_ERROR", convert_sel_error },
	{ "_DRA_DROP_REJECTED", convert_dnd_reject },
#ifdef FY_NGHYFRINACH
	{ "WINDOW", convert_to_window },
	{ "DRAWABLE", convert_to_window },
	{ "CLASS", convert_to_class },
	{ "CLIENT_WINDOW", convert_to_client_window },
#endif /* FY_NGHYFRINACH */
	{ "NAME", convert_to_name },
	{ "FONT", convert_to_font },
	{ "MULTIPLE", NULL },
	{ "TIMESTAMP", NULL }
};

#define NUM_OWN_TARGETS (sizeof(my_targets) / sizeof(my_targets[0]))

static void create_selection_items(Iccc self, Xv_server srv)
{
	struct passwd *pw;
	struct utsname name;
	long pid;

	uname(&name);
	xv_create(self, SELECTION_ITEM,
				SEL_TYPE_NAME, "OWNER_OS",
				SEL_LENGTH, strlen(name.sysname),
				SEL_DATA, name.sysname,
				SEL_REPLY_TYPE, XA_STRING,
				NULL);
	xv_create(self, SELECTION_ITEM,
				SEL_TYPE_NAME, "HOST_NAME",
				SEL_LENGTH, strlen(name.nodename),
				SEL_DATA, name.nodename,
				SEL_REPLY_TYPE, XA_STRING,
				NULL);
	xv_create(self, SELECTION_ITEM,
				SEL_TYPE_NAME, "_SUN_FILE_HOST_NAME",
				SEL_LENGTH, strlen(name.nodename),
				SEL_DATA, name.nodename,
				SEL_REPLY_TYPE, XA_STRING,
				NULL);

	if ((pw = getpwuid(getuid()))) {
		xv_create(self, SELECTION_ITEM,
					SEL_TYPE_NAME, "USER",
					SEL_LENGTH, strlen(pw->pw_name),
					SEL_DATA, pw->pw_name,
					SEL_REPLY_TYPE, XA_STRING,
					NULL);
	}
	pid = (long)getpid();
	xv_create(self, SELECTION_ITEM,
				SEL_TYPE_NAME, "PROCESS",
				SEL_LENGTH, 1,
				SEL_DATA, &pid,
				SEL_FORMAT, 32,
				SEL_REPLY_TYPE, XA_INTEGER,
				NULL);
	xv_create(self, SELECTION_ITEM,
				SEL_TYPE_NAME, "TASK",
				SEL_LENGTH, 1,
				SEL_DATA, &pid,
				SEL_FORMAT, 32,
				SEL_REPLY_TYPE, XA_INTEGER,
				NULL);
	xv_create(self, SELECTION_ITEM,
				SEL_COPY, FALSE,
				SEL_TYPE_NAME, "_SUN_DRAGDROP_ACK",
				SEL_LENGTH, 0L,
				SEL_DATA, NULL,
				SEL_FORMAT, 32,
				SEL_REPLY_TYPE, xv_get(srv, SERVER_ATOM, "NULL"),
				NULL);
}

static int convert_request(Iccc_private *priv, Atom *type, Xv_opaque *value,
				unsigned long *length, int *format)
{
	int i;

	SERVERTRACE((300, "convert_request called for target '%s'\n",
					(char *)xv_get(priv->srv, SERVER_ATOM_NAME, *type)));
	for (i = 0; i < NUM_OWN_TARGETS; i++) {
		if (*type == (Atom)xv_get(priv->srv,SERVER_ATOM,my_targets[i].name)) {
			if (my_targets[i].convert) {
				return (*my_targets[i].convert)(priv, type, value,
													length, format);
			}
			else break;
		}
	}

	if (priv->appl_converter) {
		if ((*(priv->appl_converter))(ICCCPUB(priv),type,value,length,format))
			return TRUE;
	}

	SERVERTRACE((303, "calling sel_convert_proc\n"));
	return sel_convert_proc(ICCCPUB(priv), type, value, length, format);
}

static void add_target(Iccc_private *priv, Atom newatom)
{
/* 	DTRACE(DTL_SET, "add_target: %ld\n", newatom); */
	if (priv->num_targets_used + 1 >= priv->num_targets_alloc) {
		priv->num_targets_alloc *= 2;
		priv->targets = (Atom *)realloc(priv->targets,
									priv->num_targets_alloc * sizeof(Atom));
	}

	priv->targets[priv->num_targets_used++] = newatom;
}

static void remove_target(Iccc_private *priv, Atom old)
{
	int i;

	for (i = priv->num_targets_used - 1; i >= 0; i--) {
		if (priv->targets[i] == old) {
			for (; i < --priv->num_targets_used; i++) {
				priv->targets[i] = priv->targets[i + 1];
			}
			break;
		}
	}
}

static int iccc_init(Xv_window owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	Xv_iccc_public *self = (Xv_iccc_public *)slf;
	Iccc_private *priv = xv_alloc(Iccc_private);
	int i;

	if (!priv) return XV_ERROR;

	priv->public_self = slf;
	self->private_data = (Xv_opaque)priv;

	priv->num_targets_alloc = NUM_OWN_TARGETS * 2;
	priv->targets = (Atom *)calloc((unsigned long)priv->num_targets_alloc,
														sizeof(Atom));
	if (!priv->targets) return XV_ERROR;

	priv->srv = XV_SERVER_FROM_WINDOW(owner);
	
	for (i = 0; i < NUM_OWN_TARGETS; i++) {
		priv->targets[i] = (Atom)xv_get(priv->srv, SERVER_ATOM,
									my_targets[i].name);
	}
	priv->num_targets_used = NUM_OWN_TARGETS;

	return XV_OK;
}

static Xv_opaque iccc_set(Iccc self, Attr_avlist avlist)
{
	Attr_attribute *attrs, *list;
	Iccc_private *priv = ICCCPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case ICCC_APPL_CONVERT_PROC:
			priv->appl_converter = (convert_func)A1;
			ADONE;
		case ICCC_ADD_TARGET:
			add_target(priv, (Atom)A1);
			ADONE;
		case ICCC_ADD_TARGETS:
			list = attrs + 1;
			while (*list) {
				add_target(priv, (Atom)*list);
				++list;
			}
			ADONE;
		case ICCC_ADD_TARGET_NAME:
			add_target(priv, (Atom)xv_get(priv->srv, SERVER_ATOM, A1));
			ADONE;
		case ICCC_ADD_TARGET_NAMES:
			list = attrs + 1;
			while (*list) {
				add_target(priv,
					(Atom)xv_get(priv->srv, SERVER_ATOM, *list));
				++list;
			}
			ADONE;
		case ICCC_REMOVE_TARGET:
			remove_target(priv, (Atom)A1);
			ADONE;
		case ICCC_REMOVE_TARGETS:
			list = attrs + 1;
			while (*list) {
				remove_target(priv, (Atom)*list);
				++list;
			}
			ADONE;
		case ICCC_REMOVE_TARGET_NAME:
			remove_target(priv, (Atom)xv_get(priv->srv, SERVER_ATOM, A1));
			ADONE;
		case ICCC_REMOVE_TARGET_NAMES:
			list = attrs + 1;
			while (*list) {
				remove_target(priv,
					(Atom)xv_get(priv->srv, SERVER_ATOM, *list));
				++list;
			}
			ADONE;
		case SEL_NEXT_ITEM:
			/* Spezialhack der eigenen libxview: der will (vorher) wissen, 
			 * was er alles anbieten kann (fuer das gar lustige Xdnd...)
			 */
			{
				Atom *list = (Atom *)A1;
				int i, j;

				for (i = 0; list[i] != 0; i++);
				for (j = 0; j < priv->num_targets_used; j++, i++) {
					list[i] = priv->targets[j];
				}
			}
			break;
		case XV_END_CREATE:
			create_selection_items(self, priv->srv);
			if ((Xv_opaque)xv_get(self, SEL_CONVERT_PROC) ==
				(Xv_opaque)sel_convert_proc) {
				xv_set(self, SEL_CONVERT_PROC, xv_iccc_convert, NULL);
			}
			break;
		default: xv_check_bad_attr(ICCC, A0);
			break;
	}

	return XV_OK;
}

static Xv_opaque iccc_get(Iccc self, int *status, Attr_attribute attr,
				va_list vali)
{
	Iccc_private *priv = ICCCPRIV(self);
	iccc_convert_t *cvt;

	*status = XV_OK;
	switch ((int)attr) {
		case ICCC_CONVERT:
			cvt = va_arg(vali, iccc_convert_t *);
			return (Xv_opaque)convert_request(priv, cvt->type, cvt->value,
											cvt->length, cvt->format);
		default:
			*status = xv_check_bad_attr(ICCC, attr);
			return (Xv_opaque)XV_OK;

	}
}

int xv_iccc_convert(Xv_opaque self, Atom *type, Xv_opaque *value,
				unsigned long *length, int *format)
{
	iccc_convert_t cvt;

	cvt.type = type;
	cvt.value = value;
	cvt.length = length;
	cvt.format = format;

	return (int)xv_get(self, ICCC_CONVERT, &cvt);
}

static int iccc_destroy(Iccc self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Iccc_private *priv = ICCCPRIV(self);

		if (priv->targets) free((char *)priv->targets);
		free((char *)priv);
	}
	return XV_OK;
}

const Xv_pkg xv_iccc_pkg = {
	"Iccc",
	ATTR_PKG_ICCC,
	sizeof(Xv_iccc_public),
	DRAGDROP,
	iccc_init,
	iccc_set,
	iccc_get,
	iccc_destroy,
	0
};
