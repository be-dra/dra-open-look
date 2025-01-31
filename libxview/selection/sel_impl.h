#ifndef lint
#ifdef SCCS
static char     sccsid[] = "@(#)sel_impl.h 1.10 91/03/01 DRA: $Id: sel_impl.h,v 4.18 2025/01/27 19:40:07 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#ifndef sel_impl_defined
#define sel_impl_defined

#include <xview/sel_pkg.h>
#include <xview/window.h>
#include <xview_private/portable.h>


#define SEL_PUBLIC(object)	XV_PUBLIC(object)
#define SEL_PRIVATE(object)	XV_PRIVATE(Sel_info, Xv_sel, object)

#define SEL_OWNER_PUBLIC(object) XV_PUBLIC(object)
#define SEL_OWNER_PRIVATE(object) \
    XV_PRIVATE(Sel_owner_info, Xv_sel_owner, object)

#define SEL_REQUESTOR_PUBLIC(object) XV_PUBLIC(object)
#define SEL_REQUESTOR_PRIVATE(object) \
    XV_PRIVATE(Sel_req_info, Xv_sel_requestor, object)

#define SEL_ITEM_PUBLIC(object)	XV_PUBLIC(object)
#define SEL_ITEM_PRIVATE(object) \
    XV_PRIVATE(Sel_item_info, Xv_sel_item, object)


#define SEL_ADD_CLIENT     0
#define SEL_DELETE_CLIENT  1



typedef struct sel_type_tbl {
    Atom	       type;
    char	       *type_name;
    int            status;
    Sel_prop_info  *propInfo;
} Sel_type_tbl;


/*
 * Selection object private data
 */
typedef struct sel_info {
    Selection	    public_self;  /* back pointer to public object */
    Atom	    rank;
    char	    *rank_name;
    struct timeval  time;
    u_int	    timeout;
    Display         *dpy;
} Sel_info;


typedef void (*reply_proc_t)(Selection_requestor,Atom,Atom,Xv_opaque,unsigned long,int);

/*
 * Selection_requestor object private data
 */
typedef struct sel_req_info {
    Selection_requestor	    public_self;  /* back pointer to public object */
    int		    nbr_types;	/* number of types and type names */
    reply_proc_t	reply_proc, saved_reply_proc;
	int             auto_collect_incr;
	int                is_incremental;
	unsigned long      incr_size;
	char               *incr_collector;
    Sel_type_tbl    *typeTbl;
    int             typeIndex;
} Sel_req_info;



/*
 * Selection_owner object private data
 */
typedef struct requestor {
    XID        requestor;
    Atom       property;
    Atom       target;
    Atom       type;
    int        format;
    char       *data;
    int        bytelength;
    int        offset;
    int        timeout;
    Time       time;
    int        incr;              /* reply in increments */
    int        numIncr;           /* number of incrs in a request */
    int        multiple;
    reply_proc_t reply_proc;
    Atom       *incrPropList;
    struct sel_owner_info  *owner;
} Requestor;


typedef struct  sel_prop_list {
    Atom     prop;
    int      avail;
    struct  sel_prop_list *next;
} Sel_prop_list;


typedef struct  sel_atom_list {
    Atom         multiple;
    Atom         targets;
    Atom         timestamp;
    Atom         file_name;
    Atom         string;
    Atom         incr;
    Atom         integer;
    Atom         atom_pair;
#ifdef OW_I18N
    Atom	 ctext;
#endif  /* OW_I18N */
} Sel_atom_list;


typedef struct {
    Atom  target;
    Atom  property;
} atom_pair;

typedef Bool (*convert_proc_t)(Selection_owner,Atom *,Xv_opaque *,unsigned long *,int *);

typedef struct sel_owner_info {
    Selection_owner      public_self;  /* back pointer to public object */
    convert_proc_t convert_proc; /* called only via sel_wrap_convert_proc */
    void	        (*done_proc)(Selection_owner, Xv_opaque, Atom);
    void	        (*lose_proc)(Selection_owner);
    Bool	        own;	/* True: acquired, False: lost */
    struct sel_item_info *first_item;
    struct sel_item_info *last_item;
    Display         *dpy;
    Time	    time;
    XID             xid;
    Atom            property;
    Atom            selection;
    int             status;
    Sel_atom_list   *atomList;
    Sel_prop_info   *propInfo;
	int propInfoDataAlloced;
    Sel_req_info    *req_info;
    Requestor       *req;
	void            *to_be_freed;
} Sel_owner_info;


/*
 * Selection_item object private data
 */
typedef struct sel_item_info {
    Selection_item  public_self;  /* back pointer to public object */
    Selection_copy_mode	copy;	/* True: malloc & copy data */
    Xv_opaque	    data;
    int		    format;	/* data element size: 8, 16 or 32 bits */
    unsigned long   length;	/* nbr of elements in data */
    struct sel_item_info *next;
    struct sel_owner_info *owner;
    struct sel_item_info *previous;
    Atom	    type;
    char	   *type_name;
    Atom	    reply_type;
	unsigned blocksize;	/* used only when copy == SEL_COPY_BLOCKED */
	unsigned buflen;	/* used only when copy == SEL_COPY_BLOCKED */
	char *buffer;	/* used only when copy == SEL_COPY_BLOCKED */
} Sel_item_info;


/*
 *  Reply data
 */
typedef struct {
	Window         sri_requestor;
	Atom           *sri_target;
	Atom           sri_property;
	int            sri_format;
	Xv_opaque      sri_propdata;
	unsigned long  sri_length;
	int            sri_timeout;
	int            sri_multiple_count;
	atom_pair      *sri_atomPair;
	Time           sri_time;
	int            sri_status;
	int            sri_during_incr;
	Atom           sri_selection;
	Display        *sri_dpy;
	Atom           sri_incr;
	Atom           sri_multiple;
	Sel_owner_info *sri_local_owner;
	Sel_req_info   *sri_req_info;
} Sel_reply_info;


typedef struct sel_client_info {
    Sel_owner_info          *client;
    struct sel_client_info  *next;
} Sel_client_info;



Pkg_private int xv_sel_add_prop_notify_mask(Display *dpy, Window win,XWindowAttributes *winAttr);
Pkg_private Atom xv_sel_get_property(Display *);
Pkg_private void xv_sel_free_property(Display *, Atom);
Pkg_private int xv_sel_predicate(Display *display, XEvent *xevent, char *args);
Pkg_private int xv_sel_check_property_event(Display *display, XEvent *xevent, char *args);
Xv_private int xv_sel_handle_incr(Sel_owner_info *selection);
Pkg_private void xv_sel_cvt_xtime_to_timeval(Time, struct timeval *);
Pkg_private Time xv_sel_cvt_timeval_to_xtime(struct timeval *);
Pkg_private Sel_atom_list *xv_sel_find_atom_list(Display *dpy, Window xid);
/* Pkg_private Sel_prop_list *xv_sel_get_prop_list(); */
Pkg_private void xv_sel_set_reply(Sel_reply_info  *reply);
Pkg_private Sel_reply_info *xv_sel_get_reply(XEvent *event);
Pkg_private Sel_cmpat_info  * xv_sel_get_compat_data(Display *);

Pkg_private void xv_sel_send_old_pkg_sel_clear(Display *dpy, Atom selection, Window xid, Time time);
Pkg_private void xv_sel_free_compat_data(Display *dpy, Atom selection);
Xv_private int xv_sel_handle_property_notify(XPropertyEvent *ev);

Xv_private int xv_seln_handle_req(Sel_cmpat_info *cmpatInfo, Display *dpy, Atom sel, Atom target, Atom prop, Window req, Time time);
Xv_private void xv_sel_send_old_owner_sel_clear(Display *dpy, Atom selection, Window xid, Time time);
Xv_private void xv_sel_set_compat_data(Display *dpy, Atom selection, Window xid, int clientType);
Pkg_private Sel_owner_info  *xv_sel_find_selection_data(Display *,Atom,Window);
Pkg_private Sel_owner_info * xv_sel_set_selection_data(Display *dpy, Atom selection, Sel_owner_info *sel_owner);
Xv_private Time xv_sel_get_last_event_time(Display *, Window);
Pkg_private int xv_sel_block_for_event(Display *display, XEvent *xevent, int seconds, int (*predicate)(Display *, XEvent *, char *), char *arg);
Xv_private int xv_sel_handle_selection_request(XSelectionRequestEvent *reqEvent);
Pkg_private int xv_sel_check_selnotify(Display *display, XEvent *xevent, char *args);
Pkg_private char * xv_sel_atom_to_str(Display *dpy, Atom atom, XID xid);
Pkg_private Atom xv_sel_str_to_atom(Display *dpy, char *str, XID xid);
Pkg_private void xv_sel_handle_error(int errCode, Sel_req_info *sel, Sel_reply_info *replyInfo, Atom target);
Pkg_private Notify_value xv_sel_handle_sel_timeout(Notify_client client, int which);
Pkg_private int xv_sel_end_request(Sel_reply_info *reply);
Xv_private void xv_sel_handle_selection_clear(XSelectionClearEvent *clrEv);
Xv_private int xv_sel_handle_selection_notify(XSelectionEvent *ev);
Pkg_private int sel_wrap_convert_proc(Selection_owner owner, Atom *type,
					Xv_opaque *value, unsigned long *length, int *format);

extern XContext  selCtx;
extern XContext  reqCtx;
extern XContext  targetCtx;
extern XContext  propCtx;

#endif /* sel_impl_defined */


