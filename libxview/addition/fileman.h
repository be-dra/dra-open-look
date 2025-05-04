#ifndef fileman_included

#define fileman_included

#include <xview/dircanv.h>

/* "@(#) %M% V%I% %E% %U% $Id: fileman.h,v 1.27 2025/05/04 06:35:13 dra Exp $" */

extern const Xv_pkg xv_filemanager_pkg;
#define FILE_MANAGER &xv_filemanager_pkg
typedef Xv_opaque FileManager;

typedef struct {
	Xv_dir_public parent_data;
    Xv_opaque     private_data;
} Xv_fm_public;

#define FM_OP_COPY 0
#define FM_OP_MOVE 1
#define FM_OP_LINK 2
#define FM_OP_COPY_FROM_CUT 3
#define FM_OP_COPY_LOCAL 4
#define FM_OP_PASTE_TEXT 5
#define FM_OP_URL 6

#define FM_STATUS_OK 0
#define FM_STATUS_NEED_DELETE 1
#define FM_STATUS_ABORT 2
#define FM_STATUS_ERROR 3

#define FM_INHIBIT_COPY          (1<<10)
#define FM_INHIBIT_CUT           (1<<11)
#define FM_INHIBIT_PASTE         (1<<12)
#define FM_DELETE_ON_MOVE_DROP   (1<<13)

#define	FM_ATTR(type, ordinal)	ATTR(ATTR_PKG_FILEMGR, type, ordinal)

typedef enum {
	FM_DEFAULT_EDITOR       = FM_ATTR(ATTR_STRING, 1),         /* CSG */
	FM_NEW_DIRECTORY_PROC   = FM_ATTR(ATTR_FUNCTION_PTR, 2),   /* CSG */
	FM_OWN_MENU_ITEM        = FM_ATTR(ATTR_ENUM, 3),           /* --G */
	FM_ADD_MENU_ACTIVITY    = FM_ATTR(ATTR_OPAQUE, 4),         /* -S- */
	FM_ERRLOG_PROC          = FM_ATTR(ATTR_FUNCTION_PTR, 6),   /* CSG */
	FM_OPEN_DIR_PROC        = FM_ATTR(ATTR_FUNCTION_PTR, 7),   /* CSG */
	FM_START_SHELL_PROC     = FM_ATTR(ATTR_FUNCTION_PTR, 8),   /* CSG */
	FM_CONFIRM_DELETE       = FM_ATTR(ATTR_BOOLEAN, 9),        /* CSG */
	FM_FILE_TRANSFER_PROC   = FM_ATTR(ATTR_FUNCTION_PTR, 10),  /* CSG */
	FM_USE_FILE_CONTENTS    = FM_ATTR(ATTR_ENUM, 11),          /* CSG */
	FM_REMOVE_MENU_ACTIVITY = FM_ATTR(ATTR_OPAQUE, 12),        /* -S- */
	FM_CLIENT_SERVICE       = FM_ATTR(ATTR_BOOLEAN, 13),       /* CSG */
	FM_OPEN_WITH_ARGS       = FM_ATTR(ATTR_OPAQUE_PAIR, 14),   /* -S- */
	FM_DEFAULT_PRINT        = FM_ATTR(ATTR_STRING, 15),        /* CSG */
	FM_RELATIVE_LINK_LEVEL  = FM_ATTR(ATTR_INT, 18),           /* CSG */
	FM_AUTO_CREATE_MENU     = FM_ATTR(ATTR_BOOLEAN, 19),       /* CSG */
	FM_WASTEBASKET_PROC     = FM_ATTR(ATTR_FUNCTION_PTR, 21),  /* C-G */
	FM_DROP_PROC            = FM_ATTR(ATTR_FUNCTION_PTR, 22),  /* CSG */
	FM_TYPE_PROC            = FM_ATTR(ATTR_FUNCTION_PTR, 23),  /* C-- */
	FM_HANDLE_DROP          = FM_ATTR(ATTR_OPAQUE_PAIR, 99),   /* -S- */

	/* private: */
	FM_INVALIDATE_CACHE     = FM_ATTR(ATTR_NO_VALUE, 109),     /* -S- */
	FM_PERFORM_ACTION       = FM_ATTR(ATTR_OPAQUE_TRIPLE,111)  /* -S- */
} Fileman_attr;

typedef enum {
	/* really from dircanvas, must be first */
	FM_MI_UPDATE_VIEW,
	FM_MI_SELECT,
	FM_MI_DESELECT,
	FM_MI_HIDE_SUB,
	FM_MI_SORT_SUB,
	FM_MI_SHOW_PROPS,

	/* really own */
	FM_MI_OPEN,
	FM_MI_CUT,
	FM_MI_COPY,
	FM_MI_PASTE,
	FM_MI_LINK,
	FM_MI_PRINT,
	FM_MI_CREATE_FILE,
	FM_MI_CREATE_DIR,
	FM_MI_UP,
	FM_MI_COPY_MENU,
	FM_MI_OPEN_VIEW,
	FM_MI_UPDATE_MENU,
	FM_MI_UPDATE_VIEW_UNCACHED,
	FM_MI_DELETE /* must remain last */
} Fm_menu_enum;

typedef enum {
	FM_FC_NEVER,
	FM_FC_ONLY_IF_NO_NAME_MATCH,
	FM_FC_ALWAYS
} Fm_filecontents_setting;

typedef struct {
	Menu_item item;
	char only_when_dir_writable;
	char active[3];
} Fm_menu_activity;

typedef struct _fileman_types {
	Dir_subclass_attrs dscattrs;
	char *name, *action, *print;
	Server_image glyph, mask, svrim;
	void *entry; /* maybe something like CE_ENTRY */
	struct _fileman_types *next;
	unsigned long refcount;
	int is_user;
} *fileman_types;

typedef void (*fileman_assign_filetype_t)(FileManager self, struct stat *sb,
													Dir_entry_t *ent);

typedef void (*fileman_errlog_proc_t)(FileManager self, char *msg, int beep);

#endif
