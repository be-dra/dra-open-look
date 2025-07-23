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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <xview/permprop.h>
#include <xview/server.h>
#include <X11/Xresource.h>
#include <xview_private/i18n_impl.h>

#ifndef lint
char permprop_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: permprop.c,v 4.5 2025/07/22 16:27:50 dra Exp $";
#endif

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]

#define ADONE ATTR_CONSUME(*attrs);break

typedef Xv_opaque (*convertfunc)(Xv_opaque, char *, Xv_opaque, int);

typedef int (*permprop_cbproc_t)(Perm_prop_frame pf, int is_triggered);

typedef struct {
	Xv_opaque                     public_self;
	permprop_cbproc_t             appl_set_defaults;
	char                          *file_name, *resource_prefix;
	permprop_writefileproc_t      write_file_proc;
	Xv_opaque                     resource_db;
	Permprop_res_res_t            *res_descr;
	int                           num_res;
	char                          use_xvwp;
} Permprop_private;

#define PERMPROPPRIV(_x_) XV_PRIVATE(Permprop_private, Xv_permprop, _x_)
#define PERMPROPPUB(_x_) XV_PUBLIC(_x_)

/*************************************************************************/
/*****           BEGIN   resource management                         *****/
/*************************************************************************/

/* ---------------------------------------------------------- */

static char *Permprop_res_returned_value = (char *)0;
static size_t Permprop_res_retvallen = 0;

/* ---------------------------------------------------------- */

Bool Permprop_res_exists(Xv_opaque db, char *name)
{
	char *type;
	XrmValue value;

	return XrmGetResource((XrmDatabase)db, name, name, &type, &value);
}

char *Permprop_res_get_string(Xv_opaque db, char *instance, char *default_string)
{
	char *type;
	int length;
	XrmValue value;
	char *begin_ptr;
	char *end_ptr;
	char *word_ptr;

	if (!XrmGetResource((XrmDatabase)db, instance, instance, &type, &value))
		return default_string;

	/* Strip all the leading blanks of *value.addr */
	begin_ptr = value.addr;
	while (isspace (*begin_ptr)) ++begin_ptr;

	length = value.size;
	if (length >= Permprop_res_retvallen) {
		if (Permprop_res_returned_value) xv_free(Permprop_res_returned_value);
		Permprop_res_retvallen = length + 50;
		Permprop_res_returned_value = xv_malloc(Permprop_res_retvallen);
	}

	word_ptr = Permprop_res_returned_value;
	end_ptr = value.addr + length;
	for (; begin_ptr < end_ptr; begin_ptr++) {
		*word_ptr=*begin_ptr;
		++word_ptr;
	}
	*word_ptr = '\0';

	return Permprop_res_returned_value;
}

static int Permprop_res_lookup_enum(char *name, Permprop_res_enum_pair *pairs)
{
	register Permprop_res_enum_pair *pair;	/* Current pair */

	for (pair = pairs; pair->name != NULL; pair++) {
		if (name == NULL) continue;
		if (0 == strcasecmp(name, pair->name)) break;
	}
	return pair->value;
}

static char *Permprop_res_convert_enum(int val, Permprop_res_enum_pair *pairs)
{
	register Permprop_res_enum_pair *pair;	/* Current pair */

	for (pair = pairs; pair->name != NULL; pair++) {
		if (pair->value == val) return pair->name;
	}
	xv_error(XV_NULL, ERROR_STRING, XV_MSG("enum value out of range"), NULL);
	return (char *)0;
}

static Permprop_res_enum_pair bools[] = {
	{ "True", (int) True },
	{ "False", (int) False },
	{ "Yes", (int) True },
	{ "No", (int) False },
	{ "On", (int) True },
	{ "Off", (int) False },
	{ "Enabled", (int) True },
	{ "Disabled", (int) False },
	{ "Set", (int) True },
	{ "Reset", (int) False },
	{ "Cleared", (int) False },
	{ "Activated", (int) True },
	{ "Deactivated", (int) False },
	{ "1", (int) True },
	{ "0", (int) False },
	{ NULL, -1 }
};

Bool Permprop_res_get_boolean(Xv_opaque db, char *name, Bool default_bool)
{
	char *string_value;	/* String value */
	register Bool value;	/* Value to return */

	string_value = Permprop_res_get_string(db, name, (char *) NULL);
	if (string_value == NULL) {
		return default_bool;
	}
	value = (Bool) Permprop_res_lookup_enum(string_value, bools);
	if ((int) value == -1) {
		char buffer[64];

		sprintf(buffer, XV_MSG("'%s' is an unrecognized boolean value"),
													string_value);
		xv_error(XV_NULL, ERROR_STRING, buffer, NULL);
		value = default_bool;
	}
	return value;
}

int Permprop_res_get_character(Xv_opaque db, char *name, int default_char)
{
	register char *string_value;	/* String value */

	string_value = Permprop_res_get_string(db, name, (char *) NULL);
	if (string_value == NULL) {
		return default_char;
	}
	if (strlen(string_value) != 1) {
		char buffer[64];

		sprintf(buffer,XV_MSG("'%s' is not a character constant"),string_value);
		xv_error(XV_NULL, ERROR_STRING, buffer, NULL);
		return default_char;
	}
	return string_value[0];
}

int Permprop_res_get_enum(Xv_opaque db, char *name, Permprop_res_enum_pair *pairs)
{
	return Permprop_res_lookup_enum(Permprop_res_get_string(db,name,(char*)0),pairs);
}

int Permprop_res_get_integer(Xv_opaque db, char *name, int default_integer)
{
	register char chr;	/* Temporary character */
	Bool error;	/* TRUE => an error has occurred */
	Bool negative;	/* TRUE => Negative number */
	register int number;	/* Resultant value */
	register char *cp;		/* character pointer */
	char *string_value;	/* String value */

	string_value = Permprop_res_get_string(db, name, (char *) NULL);
	if (string_value == NULL) {
		return default_integer;
	}
	/* Convert string into integer (with error chacking) */
	error = False;
	negative = False;
	number = 0;
	cp = string_value;
	chr = *cp++;
	if (chr == '-') {
		negative = True;
		chr = *cp++;
	}
	if (chr == '\0')
	error = True;
	while (chr != '\0') {
		if ((chr < '0') || (chr > '9')) {
			error = True;
			break;
		}
		number = number * 10 + chr - '0';
		chr = *cp++;
	}
	if (error) {
		char buffer[64];

		sprintf(buffer, XV_MSG("'%s' is not an integer"), string_value);
		xv_error(XV_NULL, ERROR_STRING, buffer, NULL);
		return default_integer;
	}
	if (negative)
	number = -number;
	return number;
}

void Permprop_res_set_string(Xv_opaque db, char *resource, char *value)
{
	XrmPutStringResource((XrmDatabase *)&db, resource, value ? value : "");
}

void Permprop_res_set_character(Xv_opaque db, char *resource, int value)
{
	char str[2];

	str[0] = value;
	str[1] = '\0';
	Permprop_res_set_string(db, resource, str);
}

void Permprop_res_set_boolean(Xv_opaque db, char *resource, Bool value)
{
	Permprop_res_set_string(db, resource, ((int)value) ? "True" : "False");
}

void Permprop_res_set_enum(Xv_opaque db, char *resource, int value, Permprop_res_enum_pair *pairs)
{
	Permprop_res_set_string(db, resource, Permprop_res_convert_enum(value, pairs));
}

void Permprop_res_set_integer(Xv_opaque db, char *resource, int value)
{
	char str[12];

	sprintf(str, "%d", value);
	Permprop_res_set_string(db, resource, str);
}

void Permprop_res_destroy_db(Xv_opaque db)
{
	XrmDestroyDatabase((XrmDatabase)db);
}

Xv_opaque Permprop_res_create_string_db(char *string)
{
	XrmDatabase new_db = NULL;

	if (! string) string = "__unknown_instance__: __unknown__";
	new_db = XrmGetStringDatabase(string);

	return (Xv_opaque)new_db;
}

Xv_opaque Permprop_res_create_file_db(char *filename)
{
	XrmDatabase new_db = NULL;

	if (! filename) return XV_NULL;

	new_db = XrmGetFileDatabase(filename);

	if (new_db) return (Xv_opaque)new_db;

	return Permprop_res_create_string_db((char *)0);
}

Xv_opaque Permprop_res_merge_dbs(Xv_opaque prio_db, Xv_opaque survive_db)
{
	XrmMergeDatabases((XrmDatabase)prio_db, (XrmDatabase *)&survive_db);
	return survive_db;
}

void Permprop_res_store_db(Xv_opaque db, char *filename)
{
	XrmPutFileDatabase((XrmDatabase)db, filename);
}

Xv_opaque Permprop_res_copy_db(Xv_opaque db)
{
	Xv_opaque new;
	char filename[30];

	strcpy(filename, "/tmp/pptfXXXXXX");
	close(mkstemp(filename));
	Permprop_res_store_db(db, filename);
	new = Permprop_res_create_file_db(filename);
	unlink(filename);

	return new;
}

static char *cat(char *s1, char *s2)
{
	char *new = xv_malloc(strlen(s1) + strlen(s2) + 2);

	strcpy(new, s1);
	xv_free(s1);
	strcat(new, s2);
	xv_free(s2);
	return new;
}

void Permprop_res_update_dbs(Xv_opaque *dbs, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt)
{
	Xv_opaque *xaddr, db;
	int *iaddr, i, j;
	char resnam[1000], *p, **args;

	if (! base) return;

	for (i = 0; i < res_cnt; i++) {
		sprintf(resnam, "%s%s", prefix, res[i].res_name);
		db = dbs[(int)res[i].category];

		switch (res[i].type) {
			case DAP_int:
				iaddr = (int *)(base + res[i].offset);
				Permprop_res_set_integer(db, resnam, (int)*iaddr);
				break;
			case DAP_bool:
				iaddr = (int *)(base + res[i].offset);
				Permprop_res_set_boolean(db, resnam, (int)*iaddr);
				break;
			case DAP_enum:
				iaddr = (int *)(base + res[i].offset);
				Permprop_res_set_enum(db, resnam, (int)*iaddr,
									(Permprop_res_enum_pair *)res[i].misc);
				break;
			case DAP_string:
				xaddr = (Xv_opaque *)(base + res[i].offset);
				Permprop_res_set_string(db, resnam, (char *)*xaddr);
				break;
			case DAP_stringlist:
				xaddr = (Xv_opaque *)(base + res[i].offset);
				args = (char **)*xaddr;
				p = xv_strsave("");
				if (args) {
					for (j = 0; args[j]; j++) {
						char *alloc_nl = xv_strsave("\n");
						p = cat(p, cat(alloc_nl, xv_strsave(args[j])));
					}
				}
				Permprop_res_set_string(db, resnam, p);
				xv_free(p);
				break;
			default:
				xaddr = (Xv_opaque *)(base + res[i].offset);
				if (res[i].misc) {
					convertfunc cb = (convertfunc)res[i].misc;

					(*cb)(db, resnam, *xaddr, TRUE);
				}
				break;
		}
	}
}

void Permprop_res_update_db(Xv_opaque db, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt)
{
	int i;
	Xv_opaque dbs[PERM_NUM_CATS];

	if (! base) return;

	for (i = 0; i < PERM_NUM_CATS; i++) dbs[i] = db;
	Permprop_res_update_dbs(dbs, prefix, base, res, res_cnt);
}

void Permprop_res_read_dbs(Xv_opaque *dbs, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt)
{
	Xv_opaque *xaddr = NULL, db;
	int *iaddr;
	int i, j, k;
	char resnam[1000], *p, *q, *fields[300], **args;

	if (! base) return;

	for (i = 0; i < res_cnt; i++) {
		sprintf(resnam, "%s%s", prefix, res[i].res_name);
		db = dbs[(int)res[i].category];

		switch (res[i].type) {
			case DAP_int:
				iaddr = (int *)(base + res[i].offset);
				*iaddr = Permprop_res_get_integer(db, resnam,
									(int)res[i].misc);
				break;
			case DAP_bool:
				iaddr = (int *)(base + res[i].offset);
				*iaddr = Permprop_res_get_boolean(db, resnam,
									(int)res[i].misc);
				break;
			case DAP_enum:
				iaddr = (int *)(base + res[i].offset);
				*iaddr = Permprop_res_get_enum(db, resnam,
									(Permprop_res_enum_pair *)res[i].misc);
				break;
			case DAP_string:
				xaddr = (Xv_opaque *)(base + res[i].offset);
				if (*xaddr) xv_free(*xaddr);

				p = Permprop_res_get_string(db, resnam, (char *)res[i].misc);
				if (p) p = xv_strsave(p);
				*xaddr = (Xv_opaque)p;
				break;
			case DAP_stringlist:
				xaddr = (Xv_opaque *)(base + res[i].offset);
				if (*xaddr) {
					char **sl = (char **)*xaddr;

					for (j = 0; sl[j]; j++) xv_free(sl[j]);
					xv_free(*xaddr);
				}

				args = (char **)0;
				p = Permprop_res_get_string(db, resnam, (char *)res[i].misc);
				if (p) {
					p = xv_strsave(p);

					j = 0;
					q = strtok(p, "\n");
					while ((j < 300) && q) {
						fields[j++] = xv_strsave(q);
						q = strtok((char *)0, "\n");
					}

					xv_free(p);

					if (j > 0) {
						args = (char **)xv_alloc_n(char *, (size_t)j + 2);
						for (k = 0; k < j; k++) args[k] = fields[k];
					}
				}

				*xaddr = (Xv_opaque)args;
				break;
			default:
				if (res[i].misc) {
					convertfunc cb = (convertfunc)res[i].misc;

					*xaddr = (*cb)(db, resnam, *xaddr, FALSE);
				}
				break;
		}
	}
}

void Permprop_res_read_db(Xv_opaque db, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt)
{
	int i;
	Xv_opaque dbs[PERM_NUM_CATS];

	if (! base) return;

	for (i = 0; i < PERM_NUM_CATS; i++) dbs[i] = db;
	Permprop_res_read_dbs(dbs, prefix, base, res, res_cnt);
}


/*************************************************************************/
/*****           END     resource management                         *****/
/*************************************************************************/

static int permprop_set_defaults(Perm_prop_frame self, int is_triggered)
{
	Permprop_private *priv = PERMPROPPRIV(self);
	int retval;

	if (priv->appl_set_defaults) {
		retval = (*(priv->appl_set_defaults))(self, is_triggered);
	}
	else retval = XV_OK;

	if (retval == XV_OK) {
		if (priv->use_xvwp) {
			Xv_server srv = XV_SERVER_FROM_WINDOW(self);
			Xv_opaque *dbs = (Xv_opaque *)xv_get(srv,SERVER_RESOURCE_DATABASES);

			Permprop_res_update_dbs(dbs,
						priv->resource_prefix ? priv->resource_prefix : "",
						(char *)xv_get(self, FRAME_PROPS_DEFAULT_DATA_ADDRESS),
						priv->res_descr, priv->num_res);

			xv_set(srv,SERVER_WRITE_RESOURCE_DATABASES, NULL);
			return XV_OK;
		}

		if (! priv->resource_db) {
			xv_error(self,
					ERROR_PKG, PERMANENT_PROPS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, "Have no resource database",
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					NULL);
			return XV_ERROR;
		}

		if (! priv->res_descr) {
			xv_error(self,
					ERROR_PKG, PERMANENT_PROPS,
					ERROR_LAYER, ERROR_PROGRAM,
					ERROR_STRING, "Have no resource description",
					ERROR_SEVERITY, ERROR_RECOVERABLE,
					NULL);
			return XV_ERROR;
		}

		Permprop_res_update_db(priv->resource_db,
						priv->resource_prefix ? priv->resource_prefix : "",
						(char *)xv_get(self, FRAME_PROPS_DEFAULT_DATA_ADDRESS),
						priv->res_descr, priv->num_res);
		if (priv->write_file_proc) {
			(*(priv->write_file_proc))(self);
		}
		else {
			if (! priv->file_name) {
				xv_error(self,
						ERROR_PKG, PERMANENT_PROPS,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, "Have no resource file name",
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						NULL);
				return XV_ERROR;
			}

			Permprop_res_store_db(priv->resource_db, priv->file_name);
		}
	}

	return retval;
}

/*ARGSUSED*/
static int permprop_init(Xv_window owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	Xv_permprop *self = (Xv_permprop *)slf;
	Permprop_private *priv = (Permprop_private *)xv_alloc(Permprop_private);
	Attr_attribute setdefattrs[9];

	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->use_xvwp = TRUE;

	setdefattrs[0] = (Attr_attribute)FRAME_PROPS_SET_DEFAULTS_PROC;
	setdefattrs[1] = (Attr_attribute)permprop_set_defaults;
	setdefattrs[2] = (Attr_attribute)NULL;
	xv_super_set_avlist(slf, PERMANENT_PROPS, setdefattrs);

	return XV_OK;
}

static Xv_opaque permprop_set(Xv_window self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Permprop_private *priv = PERMPROPPRIV(self);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case FRAME_PROPS_SET_DEFAULTS_PROC:
			priv->appl_set_defaults = (permprop_cbproc_t)A1;
			ADONE;

		case PERM_WRITE_FILE_PROC:
			priv->write_file_proc = (permprop_writefileproc_t)A1;
			priv->use_xvwp = FALSE;
			ADONE;

		case PERM_DATABASE:
			priv->resource_db = (Xv_opaque)A1;
			priv->use_xvwp = FALSE;
			ADONE;

		case PERM_FILE_NAME:
			if (priv->file_name) xv_free(priv->file_name);
			priv->file_name = (char *)0;
			if (A1) priv->file_name = xv_strsave((char *)A1);
			ADONE;

		case PERM_RESOURCE_PREFIX:
			if (priv->resource_prefix) xv_free(priv->resource_prefix);
			priv->resource_prefix = (char *)0;
			if (A1) priv->resource_prefix = xv_strsave((char *)A1);
			ADONE;

		case PERM_RESOURCE_DESCRIPTION:
			priv->res_descr = (Permprop_res_res_t *)A1;
			priv->num_res = (int)A2;
			ADONE;

		case PERM_RESET_FROM_FILE:
			if (! priv->file_name) {
				xv_error(self,
						ERROR_PKG, PERMANENT_PROPS,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, "Have no resource file name",
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						NULL);
				return XV_ERROR;
			}

			if (priv->resource_db) Permprop_res_destroy_db(priv->resource_db);
			priv->resource_db = Permprop_res_create_file_db(priv->file_name);
			ADONE;

		case PERM_RESET_FROM_DB:
			if (! priv->res_descr) {
				xv_error(self,
						ERROR_PKG, PERMANENT_PROPS,
						ERROR_LAYER, ERROR_PROGRAM,
						ERROR_STRING, "Have no resource description",
						ERROR_SEVERITY, ERROR_RECOVERABLE,
						NULL);
			}
			else {
				if (priv->use_xvwp) {
					Xv_server srv = XV_SERVER_FROM_WINDOW(self);
					Xv_opaque *dbs = (Xv_opaque *)xv_get(srv,
											SERVER_RESOURCE_DATABASES);

					Permprop_res_read_dbs(dbs, priv->resource_prefix,
							(char *)xv_get(self, FRAME_PROPS_DEFAULT_DATA_ADDRESS),
							priv->res_descr, priv->num_res);
					Permprop_res_read_dbs(dbs, priv->resource_prefix,
							(char *)xv_get(self, FRAME_PROPS_DATA_ADDRESS),
							priv->res_descr, priv->num_res);
				}
				else {
					Permprop_res_read_db(priv->resource_db, priv->resource_prefix,
							(char *)xv_get(self, FRAME_PROPS_DEFAULT_DATA_ADDRESS),
							priv->res_descr, priv->num_res);
					Permprop_res_read_db(priv->resource_db, priv->resource_prefix,
							(char *)xv_get(self, FRAME_PROPS_DATA_ADDRESS),
							priv->res_descr, priv->num_res);
				}
			}
			ADONE;

		case XV_END_CREATE:
			break;

		default: xv_check_bad_attr(PERMANENT_PROPS, A0);
			break;
	}

	return XV_OK;
}

static Xv_opaque permprop_get(Xv_window self, int *status, Attr_attribute attr,
												va_list vali)
{
	Permprop_private *priv = PERMPROPPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case FRAME_PROPS_SET_DEFAULTS_PROC:
			return (Xv_opaque)priv->appl_set_defaults;
		case PERM_WRITE_FILE_PROC:
			return (Xv_opaque)priv->write_file_proc;
		case PERM_DATABASE:
			return (Xv_opaque)priv->resource_db;
		case PERM_FILE_NAME:
			return (Xv_opaque)priv->file_name;
		case PERM_RESOURCE_PREFIX:
			return (Xv_opaque)priv->resource_prefix;
		default:
			*status = xv_check_bad_attr(PERMANENT_PROPS, attr);
			return (Xv_opaque)XV_OK;
	}
}

static void free_res_strings(Permprop_res_res_t *r, int nr, char *base)
{
	int i;

	if (! base) return;

	for (i = 0; i < nr; i++) {
		if (r[i].type == DAP_string) {
			char **saddr = (char **)(base + r[i].offset);

			if (*saddr) {
				xv_free(*saddr);
				*saddr = NULL;
			}
		}
	}
}

static int permprop_destroy(Xv_window self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Permprop_private *priv = PERMPROPPRIV(self);

		if (priv->resource_db) Permprop_res_destroy_db(priv->resource_db);
		if (priv->file_name) xv_free(priv->file_name);
		if (priv->resource_prefix) xv_free(priv->resource_prefix);

		free_res_strings(priv->res_descr, priv->num_res, 
					(char *)xv_get(self, FRAME_PROPS_DATA_ADDRESS));
		free_res_strings(priv->res_descr, priv->num_res, 
					(char *)xv_get(self, FRAME_PROPS_DEFAULT_DATA_ADDRESS));
		free_res_strings(priv->res_descr, priv->num_res, 
					(char *)xv_get(self, FRAME_PROPS_FACTORY_DATA_ADDRESS));

		memset((char *)priv, 0, sizeof(Permprop_private));
		xv_free(priv);
	}
	return XV_OK;
}

const Xv_pkg xv_permprop_pkg = {
	"PermanentPropertyFrame",
	ATTR_PKG_PERMPROP,
	sizeof(Xv_permprop),
	FRAME_PROPS,
	permprop_init,
	permprop_set,
	permprop_get,
	permprop_destroy,
	0
};
