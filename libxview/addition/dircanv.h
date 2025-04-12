#ifndef dircanv_included

#define dircanv_included

/* DIR_ASSIGN_FILETYPE sees a struct stat *, everybody using this
 * should hhave the same idea about struct stat (esp. st_size ...)
 * therefore we define (in a few moments) __USE_FILE_OFFSET64 1
 */

/* taugt erstmal (22.8.09) nur fuer linux: */
#ifdef linux
/* but this won't help if <sys/stat.h> has already been included before... -
 * - therefore:
 */
#ifndef __USE_FILE_OFFSET64
#  ifdef	_SYS_STAT_H
#    error sys/stat.h has already been included
#  endif
#  define __USE_FILE_OFFSET64 1
#endif
#endif /* linux */

#include <sys/stat.h>
#include <xview/xview.h>
#include <xview/scrollw.h>
#include <xview/attrol.h>


/* "@(#) %M% V%I% %E% %U% $Id: dircanv.h,v 1.24 2025/04/11 15:05:41 dra Exp $" */

extern const Xv_pkg xv_dircanvas_pkg;
#define DIRCANVAS &xv_dircanvas_pkg
typedef Xv_opaque Dircanvas;

typedef struct {
	Xv_scrollwin  parent_data;
    Xv_opaque     private_data;
} Xv_dir_public;

#define DIR_ICON_WIDTH 32
#define DIR_ICON_HEIGHT 32

#define	DIR_ATTR(type, ordinal)	ATTR(ATTR_PKG_DIR, type, ordinal)
#define ATTR_OPAQUE_QUAD ATTR_TYPE(ATTR_BASE_OPAQUE, 4)

typedef enum {
	DIR_LAYOUT             = DIR_ATTR(ATTR_ENUM, 1),           /* C-G */
	DIR_COL_DISTANCE       = DIR_ATTR(ATTR_INT, 2),            /* CSG */
	DIR_ROW_DISTANCE       = DIR_ATTR(ATTR_INT, 3),            /* CSG */
	DIR_DIRECTORY          = DIR_ATTR(ATTR_STRING, 4),         /* CSG */
	DIR_MULTI_SELECT_PROC  = DIR_ATTR(ATTR_FUNCTION_PTR, 5),   /* CSG */
	DIR_SELECT_PROC        = DIR_ATTR(ATTR_FUNCTION_PTR, 6),   /* CSG */
	DIR_DROP_PROC          = DIR_ATTR(ATTR_FUNCTION_PTR, 7),   /* CSG */
	DIR_SYSERR_PROC        = DIR_ATTR(ATTR_FUNCTION_PTR, 8),   /* CSG */
	DIR_SHOW_DIRS          = DIR_ATTR(ATTR_BOOLEAN, 9),        /* CSG */
	DIR_SHOW_DOT_FILES     = DIR_ATTR(ATTR_BOOLEAN, 10),       /* CSG */
	DIR_ARGS_OFFSET        = DIR_ATTR(ATTR_INT, 11),           /* CSG */
	DIR_CLIENT_DATA        = DIR_ATTR(ATTR_OPAQUE, 12),        /* CSG */
	DIR_ADD_TO_MENU        = DIR_ATTR(ATTR_OPAQUE_QUAD, 13),   /* C-- */
	DIR_FILES              =                                   /* CS- */
			DIR_ATTR(ATTR_LIST_INLINE(ATTR_NULL, ATTR_STRING), 14),
	DIR_FILE_VECTOR        = DIR_ATTR(ATTR_OPAQUE, 15),        /* CS- */
	DIR_INHIBIT            = DIR_ATTR(ATTR_LONG, 16),          /* CSG */
	DIR_FILTER_PATTERN     = DIR_ATTR(ATTR_STRING, 17),        /* CSG */
	DIR_TELL_MATCH_PROC    = DIR_ATTR(ATTR_FUNCTION_PTR, 19),  /* CSG */
	DIR_PROGRESS_PROC      = DIR_ATTR(ATTR_FUNCTION_PTR, 20),  /* CSG */
	DIR_CHOOSE_ONE         = DIR_ATTR(ATTR_BOOLEAN, 21),       /* CSG */
	DIR_UPDATE_INTERVAL    = DIR_ATTR(ATTR_INT, 22),           /* CSG */
	DIR_ROOTDROP_PROC      = DIR_ATTR(ATTR_FUNCTION_PTR, 23),  /* CSG */
	DIR_USE_ICONS          = DIR_ATTR(ATTR_BOOLEAN, 24),       /* --G */
	DIR_ICON_DIST          = DIR_ATTR(ATTR_INT, 25),           /* C-G */
	DIR_OWN_MENU_ITEM      = DIR_ATTR(ATTR_ENUM, 26),          /* --G */
	DIR_LINE_ORIENTED      = DIR_ATTR(ATTR_BOOLEAN, 27),       /* CSG */
	DIR_SELECTED_FILES     = DIR_ATTR(ATTR_INT, 28),           /* --G */
	DIR_MAX_NAME_LENGTH    = DIR_ATTR(ATTR_INT, 30),           /* CSG */
	DIR_RESCAN             = DIR_ATTR(ATTR_BOOLEAN, 31),       /* CSG */
	DIR_USE_ED_FILTERING   = DIR_ATTR(ATTR_BOOLEAN, 32),       /* CSG */
	DIR_SELECT_FULL_PATH   = DIR_ATTR(ATTR_BOOLEAN, 33),       /* CSG */
	DIR_FIND_PROC          = DIR_ATTR(ATTR_FUNCTION_PTR, 34),  /* CSG */
	DIR_DIRECTORY_FONT     = DIR_ATTR(ATTR_OPAQUE, 35),        /* C-G */
	DIR_NUM_VISIBLE_FILES  = DIR_ATTR(ATTR_INT, 36),           /* --G */
	DIR_PROPS_PROC   = DIR_ATTR(ATTR_FUNCTION_PTR, 37),  /* CSG */
	DIR_FILL_SORT_MENU     = DIR_ATTR(ATTR_OPAQUE, 38),        /* -S- */
	DIR_FUNCTION_KEYS_REG  = DIR_ATTR(ATTR_OPAQUE, 41),        /* -S- */

	/* attributes intended for subclasses */
	DIR_CHANGE_SELECTED    = DIR_ATTR(ATTR_INT, 50),           /* -S- */
	DIR_NEW_DIRECTORY      = DIR_ATTR(ATTR_OPAQUE_PAIR, 51),   /* -S- */
	DIR_ASSIGN_FILETYPE    = DIR_ATTR(ATTR_OPAQUE_PAIR, 52),   /* -S- */
	DIR_RECEIVE_DROP       = DIR_ATTR(ATTR_OPAQUE_PAIR, 53),   /* -S- */
	DIR_DROP_ON_ROOT       = DIR_ATTR(ATTR_OPAQUE_PAIR, 54),   /* -S- */
	DIR_DO_DRAG_MOVE       = DIR_ATTR(ATTR_BOOLEAN, 55),       /* --G */
	DIR_GET_ENTRY          = DIR_ATTR(ATTR_STRING, 56),        /* --G */
	DIR_START_TEXT         = DIR_ATTR(ATTR_OPAQUE_PAIR, 57),   /* -S- */
	DIR_AUTO_RENAME        = DIR_ATTR(ATTR_BOOLEAN, 58),       /* --G */
	DIR_SET_FILEPROPS      = DIR_ATTR(ATTR_OPAQUE_QUAD, 59),   /* -S- */
	DIR_CATCH              = DIR_ATTR(ATTR_OPAQUE_TRIPLE, 60), /* --G */
	DIR_CONVERT_SEL        = DIR_ATTR(ATTR_OPAQUE, 61),        /* --G */
	DIR_HANDLE_HELP        = DIR_ATTR(ATTR_OPAQUE_TRIPLE, 62), /* -S- */
	DIR_START_SCAN         = DIR_ATTR(ATTR_STRING, 63),        /* -S- */
	DIR_END_SCAN           = DIR_ATTR(ATTR_NO_VALUE, 64),      /* -S- */
	DIR_CONFIG_DRAGDROP    = DIR_ATTR(ATTR_OPAQUE, 93)         /* -S- */
} Dir_attr;

/* for DIR_INHIBIT: */
#define DIR_INHIBIT_NOTHING          0
#define DIR_INHIBIT_DRAG          (1<<0)
#define DIR_INHIBIT_DRAG_MOVE          (1<<1)
#define DIR_INHIBIT_DROP          (1<<2)
#define DIR_INHIBIT_RENAME          (1<<3)

typedef enum {
	DIR_LAYOUT_SINGLE_COLUMN,
	DIR_LAYOUT_TAKE_COLUMNS_FROM_WIDTH,
	DIR_LAYOUT_TRY_SQUARE
} Dir_layout;

typedef enum {
	DIR_UPDATE_VIEW,
	DIR_SELECT,
	DIR_DESELECT,
	DIR_HIDE_SUB,
	DIR_SORT_SUB,
	DIR_SHOW_PROPS
} Dir_menu_enum;

typedef enum {
	DIR_DIR, DIR_DIR_LINK, DIR_FILE, DIR_FILE_LINK, DIR_BROKEN_LINK
} Dir_filetype;

typedef struct {
	Pixmap icon, sel_icon;
	int ordering;
} Dir_subclass_attrs;

typedef struct {
	char *path, *name;
	int extent;
	ino_t inode;
	dev_t dev;
	int selected;
	Dir_filetype type;
	long mtime;
	long long size;
	unsigned short access_mode;
	Dir_subclass_attrs *dscattrs;
} Dir_entry_t;

typedef struct {
	Atom *type;
	Xv_opaque *value;
	unsigned long *length;
	int *format;
	Xv_opaque dnd;
	char *cur_file;
	Dir_entry_t *entry;
} Dir_sel_req_t;

typedef void (*dir_rootact_t)(Dircanvas, char **, int);
typedef void (*dir_progress_t)(Dircanvas, int cur, int max);
typedef void (*dir_find_proc_t)(Dircanvas self);
typedef void (*dir_drop_proc_t)(Dircanvas self, char **, int, char *);
typedef void (*dir_multi_proc_t)(Dircanvas self, char **, int, int);
typedef void (*dir_select_proc_t)(Dircanvas self, char *, int, int, int, Dir_entry_t *);
typedef int (*dir_props_proc_t)(Dircanvas self, char **);
typedef void (*dir_syserr_proc_t)(Dircanvas self, char **);

#endif
