/*	@(#)svr_impl.h 20.62 93/06/28 SMI   DRA: $Id: svr_impl.h,v 4.12 2026/01/13 11:22:57 dra Exp $	*/

/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license. 
 */

#ifndef _server_impl_h_already_included
#define _server_impl_h_already_included

#include <sys/types.h>
#include <xview_private/xv_list.h>
#include <xview_private/i18n_impl.h>
#include <xview/server.h>
#include <xview/screen.h>
#include <xview/frame.h>

#include <X11/Xutil.h>
#include <X11/Xresource.h>

#ifdef OW_I18N
#include <xview_private/i18n_impl.h> 
#endif /* OW_I18N */
#include <xview_private/svr_atom.h>

/* maximum # of screens per server (arbitrary) */
#define	MAX_SCREENS		10
#define BITS_PER_BYTE		8

	/* For atom mgr */
#define ATOM			0
#define NAME			1
#define TYPE			2
#define DATA			3
#define SERVER_LIST_SIZE	25

#define	OLLC_BASICLOCALE	0
#define	OLLC_DISPLAYLANG	1
#define	OLLC_INPUTLANG		2
#define	OLLC_NUMERIC		3
#define	OLLC_TIMEFORMAT		4
#define	OLLC_MAX		5

typedef struct server_proc_list {
	Xv_sl_link	next;
	Xv_opaque	id;  	/* unique id, typically xview handle */
	server_extension_proc_t extXeventProc;
	server_extension_proc_t pvtXeventProc;
} Server_proc_list;

/*	@(#)svr_impl.h 20.62 93/06/28 SMI   DRA: $Id: svr_impl.h,v 4.12 2026/01/13 11:22:57 dra Exp $	*/

/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license. 
 */

#ifndef _server_impl_h_already_included
#define _server_impl_h_already_included

#include <sys/types.h>
#include <xview_private/xv_list.h>
#include <xview_private/i18n_impl.h>
#include <xview/server.h>
#include <xview/screen.h>
#include <xview/frame.h>

#include <X11/Xutil.h>
#include <X11/Xresource.h>

#ifdef OW_I18N
#include <xview_private/i18n_impl.h> 
#endif /* OW_I18N */
#include <xview_private/svr_atom.h>

/* maximum # of screens per server (arbitrary) */
#define	MAX_SCREENS		10
#define BITS_PER_BYTE		8

	/* For atom mgr */
#define ATOM			0
#define NAME			1
#define TYPE			2
#define DATA			3
#define SERVER_LIST_SIZE	25

#define	OLLC_BASICLOCALE	0
#define	OLLC_DISPLAYLANG	1
#define	OLLC_INPUTLANG		2
#define	OLLC_NUMERIC		3
#define	OLLC_TIMEFORMAT		4
#define	OLLC_MAX		5

typedef struct server_proc_list {
	Xv_sl_link	next;
	Xv_opaque	id;  	/* unique id, typically xview handle */
	server_extension_proc_t extXeventProc;
	server_extension_proc_t pvtXeventProc;
} Server_proc_list;

typedef struct server_mask_list {
	Xv_sl_link	next;
	Xv_opaque	id; 	/* unique id, typically xview handle */
	Xv_opaque	extmask;   /* mask of X events req. by app */
	Xv_opaque	pvtmask;   /* mask of X events req. by xview pkgs*/
	Server_proc_list *proc;
} Server_mask_list;

typedef struct server_xid_list {
	Xv_sl_link	next;
	Xv_opaque	xid;    /* XID of the window */
	Server_mask_list *masklist;
} Server_xid_list;

typedef enum ollc_from {
	OLLC_FROM_ATTR		= 1,
	OLLC_FROM_CMDLINE	= 2,
	OLLC_FROM_RESOURCE	= 3,
	OLLC_FROM_POSIX		= 4,
	OLLC_FROM_C		= 5	/* Hard coded defaults/upon error */
} Ollc_from;

typedef struct ollc_item {
	char		*locale;
	Ollc_from	from;
} Ollc_item;

struct _xvwp;

typedef enum {
	SEM_INDEX_BASIC,
/* 	SEM_INDEX_HP, */
/* 	SEM_INDEX_SUN, */
	SEM_INDEX_XF86,
	__SEM_INDEX_LAST
} semantic_index_t;

typedef struct {
    Xv_sl_link		 next;
    Xv_Server		 public_self;	 /* Back pointer */
    Xv_Screen		 screens[MAX_SCREENS];
    Display		*xdisplay;
    unsigned int	*xv_map;
    unsigned char	*sem_maps[__SEM_INDEX_LAST];
    unsigned char	*ascii_map;
    /* ACC_XVIEW */
    unsigned char	*acc_map;
    unsigned		 acceleration:1;
    /* ACC_XVIEW */
    KeySym		 cut_keysym;
    KeySym		 paste_keysym;
    KeySym		 help_keysym;
    Xv_opaque		 xtime;           /* Last time stamp */
    int			 sel_function_pending;
    unsigned		 journalling;
    Atom		 journalling_atom;
    short		 in_fullscreen;
    Xv_opaque		 top_level_win;
    XID			 atom_mgr[4];
    short		 nbuttons;       /* Number of physical mouse buttons */
    unsigned int	 but_two_mod;    /* But 1 + this modifier == but 2 */
    unsigned int	 but_three_mod;  /* But 1 + this modifier == but 3 */
					 /* Above only valid if nbuttons < 3 */
    server_extension_proc_t extensionProc;
    char		*display_name;
    int			 alt_modmask;    /* Represents the modifier slot the
					  * ALT key is in, between
					  * Mod1MapIndex -> Mod5MapIndex.
					  */
    int			 meta_modmask;   /* Represents the modifier slot the
					  * META key is in, between
					  * Mod1MapIndex -> Mod5MapIndex.
					  */
    int			 num_lock_modmask;/* Represents the modifier slot the
					   * Num Lock key is in, between
					   * Mod1MapIndex -> Mod5MapIndex.
					   */
    int			 quick_modmask;     /* Represents the modifier slot the
					   * selection function keys (CUT, PASTE) are in,
					   * between Mod1MapIndex -> Mod5MapIndex.
					   */
	int shiftmask_duplicate;
	int shiftmask_constrain;
	int shiftmask_pan;
	int shiftmask_set_default;
	int shiftmask_primary_paste;
    int                  chording_timeout;
    unsigned int         chord_menu;
    unsigned int	 dnd_ack_key;     /* For Dnd acks under local drops */
    unsigned int	 atom_list_head_key;/* For the list of allocated atoms*/
    unsigned int	 atom_list_tail_key;/* For the list of allocated atoms*/
    unsigned int         atom_list_number;/* The size of the atom list */
    XrmDatabase		 db;
	int shape_available;
    Ollc_item		 ollc[OLLC_MAX];
    char		*localedir;
    unsigned long	 focus_timestamp;/* storing the FocusIn/Out timestamps
                                          * recieved during WM_TAKE_FOCUS/
					  * ButtonPress used in soft function
					  * keys
					  */
    XComposeStatus	*composestatus;
    int			 pass_thru_modifiers;/* Modifiers the user does not want
					      * us to use for mouseless in the
					      * ttysw.  Pass them through to
					      * the pty.
					      */
#ifdef OW_I18N
    XIM                  xim;		     /* handle to IM server */
#ifdef FULL_R5
    XIMStyles		*supported_im_styles;/* IM styles supported by both
					      * im-server and toolkit 
					      */
    char		*preedit_style;	     /* preedit style requested */
    char		*status_style;       /* status style requested */
    					     /* Store as string so that we can
					      * support a preference list 
					      */
    XIMStyle		 determined_im_style;/* Negotiated IM style based on
					      * what is supported and requested.
					      */
#endif /* FULL_R5 */
#endif /* OW_I18N */
    Server_proc_list	*idproclist;
    Server_xid_list	*xidlist;
    XContext		 svr_dpy_context;    /* Context used in XSaveContext to
					      * store svr obj from dpy struct.
					      */
#ifdef OW_I18N
        _xv_string_attr_dup_t		app_name_string;
#else
    char		*app_name_string;
#endif
	server_ui_registration_proc_t ui_reg_proc;
	server_trace_proc_t trace_proc;
	char *app_help_file;
	int want_rows_and_columns;
	struct _xvwp *xvwp;
} Server_info;

typedef struct _Server_atom_list {
    Xv_sl_link		next;
    Atom		list[SERVER_LIST_SIZE];
} Server_atom_list;

#define	SERVER_PRIVATE(server)	XV_PRIVATE(Server_info, Xv_server_struct, server)
#define	SERVER_PUBLIC(server)	XV_PUBLIC(server)

#define         SELN_FN_NUM     3

Pkg_private Xv_opaque	server_init_x(char *);

/* server_get.c */
Pkg_private Xv_opaque server_get_attr(Xv_Server server_public, int *status, Attr_attribute attr, va_list valist);

/* server_set.c */
Pkg_private     Xv_opaque server_set_avlist(Xv_Server server_public, Attr_attribute *avlist);
Pkg_private Server_xid_list *server_xidnode_from_xid(Server_info *server, Xv_opaque xid);
Pkg_private Server_mask_list *server_masknode_from_xidid(Server_info *server, Xv_opaque xid, Xv_opaque pkg_id);
Pkg_private  Server_proc_list *server_procnode_from_id(Server_info *server, Xv_opaque pkg_id);
Xv_private void server_refresh_modifiers(Xv_opaque server_public, Bool update_map);
Xv_private int server_parse_keystr(Xv_server server_public, CHAR *keystr, KeySym *keysym, short *code, unsigned int *modifiers, unsigned int diamond_mask, char *qual_str);

Xv_private void xv_string_to_rgb(char *buffer, unsigned char *red, unsigned char *green, unsigned char *blue);
Xv_private void server_journal_sync_event(Xv_Server server_public, int type);
Xv_private void server_register_ui(Xv_server srv, Xv_opaque uiElem, const char *name);

Xv_private void server_trace_set_file_line(const char *file, int line);
Xv_private void server_trace(int level, const char *format, ...);
#define SERVERTRACE(_a_) server_trace_set_file_line(__FILE__,__LINE__),server_trace _a_

Xv_private Atom server_intern_atom(Server_info *server, char *atomName,Atom at);
Xv_private char *server_get_atom_name(Server_info *server, Atom atom);
Xv_private int server_set_atom_data(Server_info *server, Atom atom, Xv_opaque data);
Xv_private Xv_opaque server_get_atom_data(Server_info *server, Atom atom, int *status);
Xv_private Server_atom_type server_get_atom_type(Xv_Server server_public,
													Atom atom);

Xv_private Xv_opaque server_get_timestamp(Xv_Server server_public);
Xv_private Xv_opaque server_get_fullscreen(Xv_Server server_public);
Xv_private void server_set_timestamp(Xv_Server server_public, struct timeval *ev_time, unsigned long xtime);
Xv_private void server_set_fullscreen(Xv_Server server_public, int in_fullscreen);
Xv_private 	void server_do_xevent_callback(Server_info *server, Display *display, XEvent	*xevent);

Xv_private int server_get_seln_function_pending(Xv_Server server_public);
Xv_private void server_set_seln_function_pending(Xv_Server server_public, int);
Xv_private int server_sem_map_index(KeySym ks);

Pkg_private void server_xvwp_init(Server_info *srv, char **argv);
Pkg_private void server_xvwp_connect(Xv_server srv, char *base_inst_name);
Pkg_private Xv_opaque *server_xvwp_get_db(Server_info *srvpriv);
Pkg_private void server_xvwp_write_file(Server_info *srvpriv);
Pkg_private void server_xvwp_install(Frame base);
Pkg_private int server_xvwp_is_own_help(Xv_server srv, Frame fr, Event *ev);
Pkg_private void server_note_register_ui(Xv_server srv,Xv_opaque obj,const char *name);
Xv_private void server_set_popup(Frame, Attr_attribute *);
Xv_private void server_set_menu(Xv_opaque menu, Xv_opaque win);
Pkg_private void server_show_propwin(Server_info *srvpriv);
Pkg_private void server_appl_set_busy(Server_info *srvpriv, int busy, Frame except);
Pkg_private void server_register_secondary_base(Xv_Server srv, Frame secondary,
													Frame baseframe);
Pkg_private void server_initialize_atoms(Server_info *server);
#endif
typedef struct server_mask_list {
	Xv_sl_link	next;
	Xv_opaque	id; 	/* unique id, typically xview handle */
	Xv_opaque	extmask;   /* mask of X events req. by app */
	Xv_opaque	pvtmask;   /* mask of X events req. by xview pkgs*/
	Server_proc_list *proc;
} Server_mask_list;

typedef struct server_xid_list {
	Xv_sl_link	next;
	Xv_opaque	xid;    /* XID of the window */
	Server_mask_list *masklist;
} Server_xid_list;

typedef enum ollc_from {
	OLLC_FROM_ATTR		= 1,
	OLLC_FROM_CMDLINE	= 2,
	OLLC_FROM_RESOURCE	= 3,
	OLLC_FROM_POSIX		= 4,
	OLLC_FROM_C		= 5	/* Hard coded defaults/upon error */
} Ollc_from;

typedef struct ollc_item {
	char		*locale;
	Ollc_from	from;
} Ollc_item;

struct _xvwp;

typedef enum {
	SEM_INDEX_BASIC,
/* 	SEM_INDEX_HP, */
/* 	SEM_INDEX_SUN, */
	SEM_INDEX_XF86,
	__SEM_INDEX_LAST
} semantic_index_t;

typedef struct {
    Xv_sl_link		 next;
    Xv_Server		 public_self;	 /* Back pointer */
    Xv_Screen		 screens[MAX_SCREENS];
    Display		*xdisplay;
    unsigned int	*xv_map;
    unsigned char	*sem_maps[__SEM_INDEX_LAST];
    unsigned char	*ascii_map;
    /* ACC_XVIEW */
    unsigned char	*acc_map;
    unsigned		 acceleration:1;
    /* ACC_XVIEW */
    KeySym		 cut_keysym;
    KeySym		 paste_keysym;
    KeySym		 help_keysym;
    Xv_opaque		 xtime;           /* Last time stamp */
    int			 sel_function_pending;
    unsigned		 journalling;
    Atom		 journalling_atom;
    short		 in_fullscreen;
    Xv_opaque		 top_level_win;
    XID			 atom_mgr[4];
    short		 nbuttons;       /* Number of physical mouse buttons */
    unsigned int	 but_two_mod;    /* But 1 + this modifier == but 2 */
    unsigned int	 but_three_mod;  /* But 1 + this modifier == but 3 */
					 /* Above only valid if nbuttons < 3 */
    void	       (*extensionProc)(Display *,XEvent *, Xv_opaque); 
    char		*display_name;
    int			 alt_modmask;    /* Represents the modifier slot the
					  * ALT key is in, between
					  * Mod1MapIndex -> Mod5MapIndex.
					  */
    int			 meta_modmask;   /* Represents the modifier slot the
					  * META key is in, between
					  * Mod1MapIndex -> Mod5MapIndex.
					  */
    int			 num_lock_modmask;/* Represents the modifier slot the
					   * Num Lock key is in, between
					   * Mod1MapIndex -> Mod5MapIndex.
					   */
    int			 quick_modmask;     /* Represents the modifier slot the
					   * selection function keys (CUT, PASTE) are in,
					   * between Mod1MapIndex -> Mod5MapIndex.
					   */
	int shiftmask_duplicate;
	int shiftmask_constrain;
	int shiftmask_pan;
	int shiftmask_set_default;
	int shiftmask_primary_paste;
    int                  chording_timeout;
    unsigned int         chord_menu;
    unsigned int	 dnd_ack_key;     /* For Dnd acks under local drops */
    unsigned int	 atom_list_head_key;/* For the list of allocated atoms*/
    unsigned int	 atom_list_tail_key;/* For the list of allocated atoms*/
    unsigned int         atom_list_number;/* The size of the atom list */
    XrmDatabase		 db;
	int shape_available;
    Ollc_item		 ollc[OLLC_MAX];
    char		*localedir;
    unsigned long	 focus_timestamp;/* storing the FocusIn/Out timestamps
                                          * recieved during WM_TAKE_FOCUS/
					  * ButtonPress used in soft function
					  * keys
					  */
    XComposeStatus	*composestatus;
    int			 pass_thru_modifiers;/* Modifiers the user does not want
					      * us to use for mouseless in the
					      * ttysw.  Pass them through to
					      * the pty.
					      */
#ifdef OW_I18N
    XIM                  xim;		     /* handle to IM server */
#ifdef FULL_R5
    XIMStyles		*supported_im_styles;/* IM styles supported by both
					      * im-server and toolkit 
					      */
    char		*preedit_style;	     /* preedit style requested */
    char		*status_style;       /* status style requested */
    					     /* Store as string so that we can
					      * support a preference list 
					      */
    XIMStyle		 determined_im_style;/* Negotiated IM style based on
					      * what is supported and requested.
					      */
#endif /* FULL_R5 */
#endif /* OW_I18N */
    Server_proc_list	*idproclist;
    Server_xid_list	*xidlist;
    XContext		 svr_dpy_context;    /* Context used in XSaveContext to
					      * store svr obj from dpy struct.
					      */
#ifdef OW_I18N
        _xv_string_attr_dup_t		app_name_string;
#else
    char		*app_name_string;
#endif
	server_ui_registration_proc_t ui_reg_proc;
	server_trace_proc_t trace_proc;
	char *app_help_file;
	int want_rows_and_columns;
	struct _xvwp *xvwp;
} Server_info;

typedef struct _Server_atom_list {
    Xv_sl_link		next;
    Atom		list[SERVER_LIST_SIZE];
} Server_atom_list;

#define	SERVER_PRIVATE(server)	XV_PRIVATE(Server_info, Xv_server_struct, server)
#define	SERVER_PUBLIC(server)	XV_PUBLIC(server)

#define         SELN_FN_NUM     3

Pkg_private Xv_opaque	server_init_x(char *);

/* server_get.c */
Pkg_private Xv_opaque server_get_attr(Xv_Server server_public, int *status, Attr_attribute attr, va_list valist);

/* server_set.c */
Pkg_private     Xv_opaque server_set_avlist(Xv_Server server_public, Attr_attribute *avlist);
Pkg_private Server_xid_list *server_xidnode_from_xid(Server_info *server, Xv_opaque xid);
Pkg_private Server_mask_list *server_masknode_from_xidid(Server_info *server, Xv_opaque xid, Xv_opaque pkg_id);
Pkg_private  Server_proc_list *server_procnode_from_id(Server_info *server, Xv_opaque pkg_id);
Xv_private void server_refresh_modifiers(Xv_opaque server_public, Bool update_map);
Xv_private int server_parse_keystr(Xv_server server_public, CHAR *keystr, KeySym *keysym, short *code, unsigned int *modifiers, unsigned int diamond_mask, char *qual_str);

Xv_private void xv_string_to_rgb(char *buffer, unsigned char *red, unsigned char *green, unsigned char *blue);
Xv_private void server_journal_sync_event(Xv_Server server_public, int type);
Xv_private void server_register_ui(Xv_server srv, Xv_opaque uiElem, const char *name);

Xv_private void server_trace_set_file_line(const char *file, int line);
Xv_private void server_trace(int level, const char *format, ...);
#define SERVERTRACE(_a_) server_trace_set_file_line(__FILE__,__LINE__),server_trace _a_

Xv_private Atom server_intern_atom(Server_info *server, char *atomName,Atom at);
Xv_private char *server_get_atom_name(Server_info *server, Atom atom);
Xv_private int server_set_atom_data(Server_info *server, Atom atom, Xv_opaque data);
Xv_private Xv_opaque server_get_atom_data(Server_info *server, Atom atom, int *status);
Xv_private Server_atom_type server_get_atom_type(Xv_Server server_public,
													Atom atom);

Xv_private Xv_opaque server_get_timestamp(Xv_Server server_public);
Xv_private Xv_opaque server_get_fullscreen(Xv_Server server_public);
Xv_private void server_set_timestamp(Xv_Server server_public, struct timeval *ev_time, unsigned long xtime);
Xv_private void server_set_fullscreen(Xv_Server server_public, int in_fullscreen);
Xv_private 	void server_do_xevent_callback(Server_info *server, Display *display, XEvent	*xevent);

Xv_private int server_get_seln_function_pending(Xv_Server server_public);
Xv_private void server_set_seln_function_pending(Xv_Server server_public, int);
Xv_private int server_sem_map_index(KeySym ks);

Pkg_private void server_xvwp_init(Server_info *srv, char **argv);
Pkg_private void server_xvwp_connect(Xv_server srv, char *base_inst_name);
Pkg_private Xv_opaque *server_xvwp_get_db(Server_info *srvpriv);
Pkg_private void server_xvwp_write_file(Server_info *srvpriv);
Pkg_private void server_xvwp_install(Frame base);
Pkg_private int server_xvwp_is_own_help(Xv_server srv, Frame fr, Event *ev);
Pkg_private void server_note_register_ui(Xv_server srv,Xv_opaque obj,const char *name);
Xv_private void server_set_popup(Frame, Attr_attribute *);
Xv_private void server_set_menu(Xv_opaque menu, Xv_opaque win);
Pkg_private void server_show_propwin(Server_info *srvpriv);
Pkg_private void server_appl_set_busy(Server_info *srvpriv, int busy, Frame except);
Pkg_private void server_register_secondary_base(Xv_Server srv, Frame secondary,
													Frame baseframe);
Pkg_private void server_initialize_atoms(Server_info *server);
#endif
