#ifndef permprop_INCLUDED
#define permprop_INCLUDED 1

/*
 * "@(#) %M% V%I% %E% %U% $Id: permprop.h,v 4.2 2025/03/08 13:21:40 dra Exp $"
 *
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

#include <xview/frame.h>
#include <xview/attrol.h>

extern const Xv_pkg xv_permprop_pkg;
#define PERMANENT_PROPS &xv_permprop_pkg
typedef Xv_opaque Perm_prop_frame;

typedef struct {
	Xv_propframe   parent_data;
    Xv_opaque      private_data;
} Xv_permprop;

#define	PERM_ATTR(_t, _o) ATTR(ATTR_PKG_PERMPROP, _t, _o)

typedef enum {
	PERM_WRITE_FILE_PROC     = PERM_ATTR(ATTR_FUNCTION_PTR, 1),      /* CSG */
	PERM_RESOURCE_DESCRIPTION= PERM_ATTR(ATTR_OPAQUE_PAIR, 2),       /* CS- */
	PERM_RESET_FROM_FILE     = PERM_ATTR(ATTR_NO_VALUE, 3),          /* -S- */
	PERM_RESET_FROM_DB       = PERM_ATTR(ATTR_NO_VALUE, 4),          /* -S- */
	PERM_DATABASE            = PERM_ATTR(ATTR_OPAQUE, 6),            /* CSG */
	PERM_FILE_NAME           = PERM_ATTR(ATTR_STRING, 7),            /* CSG */
	PERM_RESOURCE_PREFIX     = PERM_ATTR(ATTR_STRING, 8)             /* CSG */
} Permprop_attr;

typedef enum {
	DAP_int,
	DAP_bool,
	DAP_enum,
	DAP_string,
	DAP_stringlist
} Permprop_res_type_t;

typedef enum {
	PRC_U, /* user */
	PRC_H, /* host */
	PRC_D, /* display */
	PRC_B  /* both: display and host */
} Permprop_res_category_t;

#define PERM_NUM_CATS 4

typedef struct {
	char *name;
	int value;
} Permprop_res_enum_pair;

#if defined(sun) && defined(SVR4)
typedef long Ppmt;
#else
typedef Xv_opaque Ppmt;
#endif

typedef struct {
	char *res_name;
	Permprop_res_category_t category;
	Permprop_res_type_t type;
	int offset;
	Ppmt misc;/* Permprop_enum_pair *enums; when type == DAP_enum */
					/* Xv_opaque default_value; */
} Permprop_res_res_t;

_XVFUNCPROTOBEGIN
EXTERN_FUNCTION (Bool Permprop_res_exists, (Xv_opaque db, char *name));
EXTERN_FUNCTION (char *Permprop_res_get_string, (Xv_opaque db, char *instance, char *default_string));
EXTERN_FUNCTION (Bool Permprop_res_get_boolean, (Xv_opaque db, char *name, int default_bool));
EXTERN_FUNCTION (int Permprop_res_get_character, (Xv_opaque db, char *name, int default_char));
EXTERN_FUNCTION (int Permprop_res_get_enum, (Xv_opaque db, char *name, Permprop_res_enum_pair *pairs));
EXTERN_FUNCTION (int Permprop_res_get_integer, (Xv_opaque db, char *name, int default_integer));
EXTERN_FUNCTION (void Permprop_res_set_string, (Xv_opaque db, char *resource, char *value));
EXTERN_FUNCTION (void Permprop_res_set_character, (Xv_opaque db, char *resource, int value));
EXTERN_FUNCTION (void Permprop_res_set_boolean, (Xv_opaque db, char *resource, int value));
EXTERN_FUNCTION (void Permprop_res_set_integer, (Xv_opaque db, char *resource, int value));
EXTERN_FUNCTION (void Permprop_res_set_enum, (Xv_opaque db, char *resource, int value, Permprop_res_enum_pair *pair));
EXTERN_FUNCTION (void Permprop_res_destroy_db, (Xv_opaque db));
EXTERN_FUNCTION (Xv_opaque Permprop_res_create_file_db, (char *filename));
EXTERN_FUNCTION (Xv_opaque Permprop_res_create_string_db, (char *string));
EXTERN_FUNCTION (Xv_opaque Permprop_res_merge_dbs, (Xv_opaque prio_db, Xv_opaque survive_db));
EXTERN_FUNCTION (void Permprop_res_store_db, (Xv_opaque db, char *filename));
EXTERN_FUNCTION (Xv_opaque Permprop_res_copy_db, (Xv_opaque db));

EXTERN_FUNCTION (void Permprop_res_update_db, (Xv_opaque db, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt));
EXTERN_FUNCTION (void Permprop_res_update_dbs, (Xv_opaque *dbs, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt));

EXTERN_FUNCTION (void Permprop_res_read_db, (Xv_opaque db, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt));
EXTERN_FUNCTION (void Permprop_res_read_dbs, (Xv_opaque *dbs, char *prefix, char *base, Permprop_res_res_t *res, int res_cnt));
_XVFUNCPROTOEND

#define PERM_OFF(_ptr_type_,_field_) FP_OFF(_ptr_type_,_field_)
#define PERM_NUMBER(_array_) (sizeof(_array_)/sizeof(_array_[0]))

#endif
