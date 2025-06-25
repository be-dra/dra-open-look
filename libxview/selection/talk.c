#include <string.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <time.h>
#include <xview/talk.h>
#include <xview/defaults.h>
#include <xview_private/svr_impl.h>

char talk_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: talk.c,v 1.49 2025/06/24 19:34:28 dra Exp $";

typedef struct _pattern {
	struct _pattern *next;
	char *name;
	char **startpars;
	Atom client_selection;
} *pattern_t;

typedef enum {
	initialized,
	server_based,
	decentral
} Talk_state_t;

typedef struct {
	Xv_opaque          public_self;

	Selection_owner    selown;
	Atom               talksrv, trigg, regist, targets;
	int                last_error, silent, notify_count;
	Xv_opaque          client_data;
	talk_notify_proc_t notify_proc;
	Talk_state_t       state;
	pattern_t          not_yet_installed, installed;
} Talk_private;

#define TALKPRIV(_x_) XV_PRIVATE(Talk_private, Xv_talk, _x_)
#define TALKPUB(_x_) XV_PUBLIC(_x_)

#define NUM(_a_)  (sizeof(_a_)/sizeof(_a_[0]))
#define AGS '\035'
#define ARS '\036'

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]

#define ADONE ATTR_CONSUME(*attrs);break

extern char *xv_app_name;
static int talk_key = 0;
#define TALK_KEY XV_KEY_DATA,talk_key

static int analyze_string(char *str, int sep, char **strings, long max_strings)
{
	long i = 0;
	char *p = str;

	do {
		strings[i++] = p;
		if ((p = strchr(p, sep))) {
			*p++ = '\0';
		}
		if (i >= max_strings) return i;
	} while (p);

	strings[i] = NULL;
	return i;
}

static void install_pattern(Talk_private *priv, char *patt, char **startpars)
{
	Talk self = TALKPUB(priv);
	char *p, sep[2], buf[2000];
	unsigned long len;
	int format, i = 0;

	if (priv->state != server_based) return;

	sprintf(buf, "%ld%c%s", xv_get(priv->selown, SEL_RANK), AGS, patt);

	sep[0] = AGS;
	sep[1] = '\0';
	while ((p = startpars[i++])) {
		strcat(buf, sep);
		strcat(buf, p);
		sep[0] = ARS;
	}

	xv_set(self,
			SEL_RANK, priv->talksrv,
			SEL_TYPE_NAME, "_DRA_TALK_PATTERN",
			SEL_TYPE_INDEX, 0,
			SEL_PROP_TYPE, XA_STRING,
			SEL_PROP_FORMAT, 8,
			SEL_PROP_LENGTH, strlen(buf),
			SEL_PROP_DATA, buf,
			NULL);

	xv_get(self, SEL_DATA, &len, &format);
	if (len == SEL_ERROR) {
		if (! priv->silent) {
			char errbuf[200];

			sprintf(errbuf, "Talk pattern installation failed with code %d",
										priv->last_error);
			xv_error(self,
				ERROR_PKG, TALK,
				ERROR_STRING, errbuf,
				ERROR_SEVERITY, ERROR_RECOVERABLE,
				NULL);
		}
		return;
	}
}

static pattern_t install_pending(Talk_private *priv, pattern_t p)
{
	if (! p) return p;
	p->next = install_pending(priv, p->next);

	install_pattern(priv, p->name, p->startpars);
	/* now link p to the "installed" list */
	p->next = priv->installed;
	priv->installed = p;
	return NULL;
}

static void client_done(Selection_owner sel_own, Xv_opaque buf, Atom target)
{
	if (target == xv_get(sel_own, SEL_RANK)) {
		if (buf) xv_free(buf);
	}
}

static void analyze_trigger_data(Talk_private *priv, char *val)
{
	char *message;
	int pid;
/* 	char *appname; */
	char *parstr;
	char *gsstrings[10];
	char *params[20];
	char *host;

	analyze_string(val, AGS, gsstrings, NUM(gsstrings));
	/* compare REF (mbfdsvnbserf) */
	message = gsstrings[0];
	pid = atoi(gsstrings[1]);
	/* appname = gsstrings[2]; unused at the moment */
	host = gsstrings[3];
	parstr = gsstrings[4];

	if (parstr) {
		analyze_string(parstr, ARS, params, NUM(params));
	}
	else {
		params[0] = NULL;
	}

	if (priv->state == decentral) {
		pattern_t p;
		/* we have to check our own patterns: */

		for (p = priv->not_yet_installed; p; p = p->next) {
			if (0 == strcmp(p->name, message)) {
				if (priv->notify_proc) {
					priv->notify_proc(TALKPUB(priv), message,host, pid, params);
				}
				break;
			}
		}
	}
	else {
		if (priv->notify_proc) {
			priv->notify_proc(TALKPUB(priv), message, host, pid, params);
		}
	}
}

static int client_convert_proc(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	Talk_private *priv = (Talk_private *)xv_get(sel_own, TALK_KEY);

	if (*type == xv_get(sel_own, SEL_RANK)) {
		Sel_prop_info *info;

		if ((info = (Sel_prop_info *)xv_get(sel_own, SEL_PROP_INFO)) &&
				info->length > 0 &&
				info->format == 8 &&
				info->type == XA_STRING)
		{
			analyze_trigger_data(priv, (char *)info->data);
		}

		*data = XV_NULL;
		*length = 0;
		*format = 8;
		return TRUE;
	}
	if (*type == priv->targets) {
		static Atom tgts[3];
		int i = 0;

		tgts[i++] = xv_get(sel_own, SEL_RANK);
		tgts[i++] = priv->targets;
		*data = (Xv_opaque)tgts;
		*type = XA_ATOM;
		*length = i;
		*format = 32;
		return TRUE;
	}
	if (*type == priv->regist) {
		/* a new server has been started - we should "reconnect" */
		xv_set(sel_own, SEL_OWN, FALSE, NULL);

		*data = XV_NULL;
		*length = 0;
		*format = 8;
		return TRUE;
	}

	return FALSE;
}

static void client_lose(Selection_owner sel_own)
{
	Talk_private *priv = (Talk_private *)xv_get(sel_own, TALK_KEY);
	Talk self = TALKPUB(priv);

	SERVERTRACE((200, "%s\n", __FUNCTION__));
	xv_set(self,
			SEL_RANK, priv->talksrv,
			SEL_TYPE, priv->regist,
			NULL);
	sel_post_req(self);
}

static void client_reply(Talk self, Atom target, Atom type,
					Xv_opaque value, unsigned long length, int format)
{
	Talk_private *priv = TALKPRIV(self);

	if (length == SEL_ERROR) {
		priv->last_error = *((int *)value);
		return;
	}

	if (target == priv->trigg) {
		if (length == 1) {
			long *cntp = (long *)value;

			priv->notify_count = (int)*cntp;
		}
	}

	if (value) xv_free(value);
}

static void cleanup_root_prop(Talk self, Atom bad_atom)
{
	Talk_private *priv = TALKPRIV(self);
	Xv_window win = xv_get(self, XV_OWNER);
	Window root = xv_get(xv_get(win, XV_ROOT), XV_XID);
	Display *dpy = (Display *)xv_get(win, XV_DISPLAY);
	int format, succ;
	unsigned long len, rest;
	Atom acttype, *data = NULL;

	/* there is a race condition between this XGetWindowProperty and
	 * the following XChangeProperty... - therefore, we use the 
	 * 'delete' flag of XGetWindowProperty, so, a second reader will get
	 * nothing and therefore will not get in our way.
	 */
	succ = XGetWindowProperty(dpy, root, priv->talksrv, 0L, 100L, TRUE,
				XA_ATOM, &acttype, &format, &len, &rest,
				(unsigned char **)&data);
	if (succ == Success && acttype == XA_ATOM && format == 32) {
		unsigned long i;

		for (i = 0; i < len; i++) {
			if (data[i] == bad_atom) {
				++i;
				for (; i < len; i++) data[i-1] = data[i];
				XChangeProperty(dpy, root, priv->talksrv, XA_ATOM, 32,
						PropModeReplace, (unsigned char *)data, 
						(int)len - 1);
				XFree(data);
				return;
			}
		}
	}
	if (data) XFree(data);
}

static void decentral_reply(Talk self, Atom target, Atom type,
					Xv_opaque value, unsigned long length, int format)
{
	/* we are in decentral mode where all participants of the TALK
	 * mechanism store their selection atom in a root property
	 * _DRA_TALK_SERVER. If such a participant terminates, its atom is
	 * still in that property and causes selection errors.
	 */
	if (length == SEL_ERROR) {
		int have_owner = format;

		if (! have_owner) cleanup_root_prop(self, target);
		return;
	}

	if (value) xv_free(value);
}

static int decentral_convert_proc(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	Talk_private *priv = (Talk_private *)xv_get(sel_own, TALK_KEY);

	if (*type == xv_get(sel_own, SEL_RANK)) {
		Sel_prop_info *info;

		if ((info = (Sel_prop_info *)xv_get(sel_own, SEL_PROP_INFO)) &&
				info->length > 0 &&
				info->format == 8 &&
				info->type == XA_STRING)
		{
			analyze_trigger_data(priv, (char *)info->data);
		}

		*data = XV_NULL;
		*length = 0;
		*format = 8;
		return TRUE;
	}
	if (*type == priv->targets) {
		static Atom tgts[3];
		int i = 0;

		tgts[i++] = xv_get(sel_own, SEL_RANK);
		tgts[i++] = priv->targets;
		*data = (Xv_opaque)tgts;
		*type = XA_ATOM;
		*length = i;
		*format = 32;
		return TRUE;
	}

	return FALSE;
}

static Notify_value note_try_again(Talk self, int unused);

static void try_again(Talk self)
{
	struct itimerval timer;

	timer.it_value.tv_sec = 2;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 2;
	timer.it_interval.tv_usec = 0;
	notify_set_itimer_func(self, note_try_again, ITIMER_REAL, &timer, NULL);
}

/* we return
 *         initialized = "don't know yet"
 *         server_based  = "server mode"
 *         decentral   = "decentral mode"
 */
static Talk_state_t check_mode(Xv_window owner, Atom tsrv)
{
	Xv_screen screen = XV_SCREEN_FROM_WINDOW(owner);
	Xv_server srv;
	Window root;
	Display *dpy;
	int format, succ;
	unsigned long len, rest;
	Atom acttype, *data;
	Talk_state_t scr_state = (Talk_state_t)xv_get(screen, TALK_KEY);

	if (scr_state != initialized) return scr_state;

	srv = XV_SERVER_FROM_WINDOW(owner);
	root = xv_get(xv_get(owner, XV_ROOT), XV_XID);
	dpy = (Display *)xv_get(srv, XV_DISPLAY);

	succ = XGetWindowProperty(dpy, root, tsrv, 0L, 100L, FALSE,
				XA_ATOM, &acttype, &format, &len, &rest,
				(unsigned char **)&data);
	if (succ == Success && acttype == XA_ATOM && format == 32) {
		XFree(data);
		xv_set(screen, TALK_KEY, decentral, NULL);
		return decentral;
	}

	if (XGetSelectionOwner(dpy, tsrv) != None) {
		xv_set(screen, TALK_KEY, server_based, NULL);
		return server_based;
	}

	return initialized;
}

static void initialize_by_mode(Talk_private *priv)
{
	Talk self = TALKPUB(priv);
	struct utsname u;
	char buf[100];
	Atom sel_atom, *data;
	Xv_window win;
	Xv_server srv;
	Window root;
	Display *dpy;
	unsigned long length;
	int format;

	if (priv->state != initialized) {
		/* we already know! */
		return;
	}

	/* No need to register anything????
	 * I can still be an instance of TALK that just calls TALK_MESSAGE...
	 */
	if (! priv->notify_proc) return;
	if (! priv->not_yet_installed) return;

	switch (check_mode(xv_get(self, XV_OWNER), priv->talksrv)) {
		case server_based:
			SERVERTRACE((200, "%s: server mode\n", __FUNCTION__));
			xv_set(self,
					SEL_RANK, priv->talksrv,
					SEL_TYPE, priv->regist,
					NULL);
			data = (Atom *)xv_get(self, SEL_DATA, &length, &format);
			if (length == SEL_ERROR) {
				try_again(self);
				return;
			}

			priv->state = server_based;
			priv->selown = xv_create(xv_get(self, XV_OWNER), SELECTION_OWNER,
					TALK_KEY, priv,
					SEL_RANK, *data,
					SEL_CONVERT_PROC, client_convert_proc,
					SEL_DONE_PROC, client_done,
					SEL_LOSE_PROC, client_lose,
					NULL);
			XFree(data);

			xv_set(self, SEL_REPLY_PROC, client_reply, NULL);
			/* now deal with not yet installed patterns */
			priv->not_yet_installed = install_pending(priv,
									priv->not_yet_installed);
			break;

		case decentral:
			priv->state = decentral;
			xv_set(self, SEL_REPLY_PROC, decentral_reply, NULL);
			win = xv_get(self, XV_OWNER);
			srv = XV_SERVER_FROM_WINDOW(win);
			root = xv_get(xv_get(win, XV_ROOT), XV_XID);
			dpy = (Display *)xv_get(srv, XV_DISPLAY);
			uname(&u);
			sprintf(buf, "_DRATALK%s%d", u.nodename, getpid());
			sel_atom = xv_get(srv, SERVER_ATOM, buf);
			priv->regist = sel_atom; /* to recognize myself (hsrdtfgklherst) */
			priv->selown = xv_create(win, SELECTION_OWNER,
						TALK_KEY, priv,
						SEL_RANK, sel_atom,
						SEL_CONVERT_PROC, decentral_convert_proc,
						SEL_OWN, TRUE,
						NULL);

			XChangeProperty(dpy, root, priv->talksrv, XA_ATOM, 32,
							PropModeAppend, (unsigned char *)&sel_atom, 1);

			break;

		default:
			/* we don't know yet... */
			try_again(self);
	}
}

static Notify_value note_try_again(Talk self, int unused)
{
	Talk_private *priv = TALKPRIV(self);

	SERVERTRACE((200, "%s\n", __FUNCTION__));

	notify_set_itimer_func(self, NOTIFY_TIMER_FUNC_NULL, ITIMER_REAL,
									NULL, NULL);
	initialize_by_mode(priv);
	return NOTIFY_DONE;
}

static int talk_init(Xv_window owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	Xv_talk *self = (Xv_talk *)slf;
	Xv_server srv = XV_SERVER_FROM_WINDOW(owner);
	Talk_private *priv;
	Attr_attribute *attrs;

	if (!(priv = (Talk_private *)xv_alloc(Talk_private))) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	if (! talk_key) talk_key = xv_unique_key();
	priv->silent = TRUE;

	if (xv_get(srv, TALK_KEY)) {
		xv_error(slf,
			ERROR_PKG, TALK,
			ERROR_STRING, "Already a TALK instance in this process",
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);
		return XV_ERROR;
	}
	xv_set(srv, TALK_KEY, slf, NULL);

	priv->talksrv = xv_get(srv, SERVER_ATOM, "_DRA_TALK_SERVER");
	priv->trigg = xv_get(srv, SERVER_ATOM, "_DRA_TALK_TRIGGER");
	priv->regist = xv_get(srv, SERVER_ATOM, "_DRA_TALK_REGISTER");
	priv->targets = xv_get(srv, SERVER_ATOM, "TARGETS");

	priv->state = (Talk_state_t)xv_get(XV_SCREEN_FROM_WINDOW(owner), TALK_KEY);

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case TALK_NOTIFY_PROC:
			priv->notify_proc = (talk_notify_proc_t)A1;
			ADONE;
		default: break;
	}
	return XV_OK;
}

static void deregister(Talk self, Talk_private *priv)
{
	Atom at = xv_get(priv->selown, SEL_RANK);

	if (priv->state == decentral) {
		cleanup_root_prop(self, at);
	}
	else {
		xv_set(self,
				SEL_RANK, priv->talksrv,
				SEL_TYPE_NAME, "_DRA_TALK_DEREGISTER",
				SEL_TYPE_INDEX, 0,
				SEL_PROP_TYPE, XA_ATOM,
				SEL_PROP_FORMAT, 32,
				SEL_PROP_LENGTH, 1,
				SEL_PROP_DATA, &at,
				NULL);
		sel_post_req(self);
	}
}

static void add_pattern(Talk_private *priv, char *patt, char **startpars)
{
	pattern_t p = xv_alloc(struct _pattern);

	if (startpars) {
		unsigned long i = 0, n = 0;
		char *q;

		while (startpars[n++]);

		p->startpars = xv_alloc_n(char *, n);
		while ((q = startpars[i++])) {
			p->startpars[i] = strdup(q);
		}
	}

	p->name = strdup(patt);
	p->next = priv->not_yet_installed;
	priv->not_yet_installed = p;
}

static void call_server(Talk t, Atom rank, Atom target, int bufsize,
				const char *msg, char **params, int *countptr, char *host)
{
	Talk_private *priv = TALKPRIV(t);
	char *p, sep[2];

	/* this is a VLA (variable length array) */
	char buf[bufsize]; /* similar to alloca */

	SERVERTRACE((200, "call_server %ld '%s' params=%p\n", target, msg, params));
	/* compare REF (mbfdsvnbserf) */
	sprintf(buf, "%s%c%d%c%s%c%s", msg,       /* gsstrings[0] */
							AGS, getpid(),    /* gsstrings[1] */
							AGS, xv_app_name, /* gsstrings[2] */
							AGS, host);       /* gsstrings[3] */
	SERVERTRACE((200, "'%s' params=%p len = %d\n", msg, params, strlen(buf)));
	sep[0] = AGS;
	sep[1] = '\0';
	if (params) {
		int i = 0;

		while ((p = params[i++])) {
			strcat(buf, sep);
			strcat(buf, p);
			sep[0] = ARS;
		}
	}

	xv_set(t,
			SEL_RANK, rank,
			SEL_TYPE, target,
			SEL_TYPE_INDEX, 0,
			SEL_PROP_TYPE, XA_STRING,
			SEL_PROP_FORMAT, 8,
			SEL_PROP_LENGTH, strlen(buf),
			SEL_PROP_DATA, buf,
			NULL);

	if (countptr) {
		unsigned long length;
		int format;

		/* the return value is freed in my reply_proc */
		xv_get(t, SEL_DATA, &length, &format);

		if (length == SEL_ERROR) {
			*countptr = -1;
		}
		else {
			*countptr = priv->notify_count;
		}
	}
	else {
		sel_post_req(t);
	}
}

static void trigger(Talk t, const char *msg, char **params, int *countptr)
{
	Talk_private *priv = TALKPRIV(t);
	char *p;
	size_t bufsize;
	struct utsname u;

	uname(&u);
	bufsize = strlen(msg) + strlen(u.nodename) + strlen(xv_app_name) + 12;
	if (params) {
		int i = 0;

		while ((p = params[i++])) {
			bufsize += strlen(p) + 2;
		}
	}

	if (priv->state == initialized) {
		priv->state = check_mode(xv_get(t, XV_OWNER), priv->talksrv);
	}

	if (priv->state == decentral) {
		Xv_window win = xv_get(t, XV_OWNER);
		Xv_server srv = XV_SERVER_FROM_WINDOW(win);
		Window root = xv_get(xv_get(win, XV_ROOT), XV_XID);
		Display *dpy = (Display *)xv_get(srv, XV_DISPLAY);
		int format, succ;
		unsigned long i, len, rest;
		Atom acttype, *data;

		succ = XGetWindowProperty(dpy, root, priv->talksrv, 0L, 100L, FALSE,
					XA_ATOM, &acttype, &format, &len, &rest,
					(unsigned char **)&data);
		if (succ == Success && acttype == XA_ATOM && format == 32) {
			for (i = 0; i < len; i++) {
				if (data[i] == XA_ATOM) continue;
				if (data[i] ==priv->regist) continue; /* Ref (hsrdtfgklherst) */
				call_server(t, data[i], data[i], (int)bufsize, msg,
									params, NULL, u.nodename);
			}
		}

		if (data) XFree(data);
		/* No counting here... however, callers (usually) want to know
		 * "how many others" were interested - this '-1' is the indication
		 * for 'we do not know' meaning 'we cannot count'.
		 */
		if (countptr) *countptr = -1;
	}
	else {
		call_server(t, priv->talksrv, priv->trigg, (int)bufsize, msg, params,
								countptr, u.nodename);
	}
}

static void new_pattern(Talk_private *priv, char *patt, char **startpars)
{
	SERVERTRACE((200, "%s(%s)\n", __FUNCTION__, patt));
	add_pattern(priv, patt, startpars);

	if (priv->state == server_based) {
		priv->not_yet_installed = install_pending(priv,priv->not_yet_installed);
	}
}

static Xv_opaque talk_set(Talk self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Talk_private *priv = TALKPRIV(self);
	char *msg = NULL, **pars = NULL;
	int *count_ptr = NULL;
	char *patt = NULL, **startpars = NULL;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case TALK_PATTERN:
			patt = (char *)A1;
			ADONE;

		case TALK_START:
			startpars = (char **)(attrs + 1);
			ADONE;

		case TALK_DEREGISTER:
			deregister(self, priv);
			ADONE;

		case TALK_MESSAGE:
			msg = (char *)A1;
			ADONE;

		case TALK_MSG_PARAMS:
			pars = (char **)(attrs + 1);
			ADONE;

		case TALK_NOTIFY_COUNT:
			count_ptr = (int *)A1;
			ADONE;

		case TALK_SILENT:
			priv->silent = (int)A1;
			ADONE;

		case TALK_CLIENT_DATA:
			priv->client_data = (Xv_opaque)A1;
			ADONE;

		case XV_END_CREATE:
			initialize_by_mode(priv);
			break;

		default: xv_check_bad_attr(TALK, A0);
			break;
	}

	if (patt) {
		new_pattern(priv, patt, startpars);
	}

	if (msg) {
		trigger(self, msg, pars, count_ptr);
	}

	return XV_OK;
}

static Xv_opaque talk_get(Talk self, int *status, Attr_attribute attr, va_list vali)
{
	Talk_private *priv = TALKPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case TALK_NOTIFY_PROC: return (Xv_opaque)priv->notify_proc;
		case TALK_SILENT: return (Xv_opaque)priv->silent;
		case TALK_CLIENT_DATA: return priv->client_data;
		case TALK_LAST_ERROR: return (Xv_opaque)priv->last_error;
		default:
			*status = xv_check_bad_attr(TALK, attr);
	}
	return (Xv_opaque)XV_OK;
}

static int talk_destroy(Talk self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Talk_private *priv = TALKPRIV(self);

		if (priv->selown) xv_destroy(priv->selown);
		xv_free((char *)priv);
	}

	return XV_OK;
}

static Xv_object talk_find(Xv_window owner, const Xv_pkg *pkg, Attr_avlist avlist)
{
	Xv_server srv = XV_SERVER_FROM_WINDOW(owner);
	Talk self;

	if (! talk_key) talk_key = xv_unique_key();

	/* only one instance per server */
	if ((self = xv_get(srv, TALK_KEY))) return self;
	return XV_NULL;
}

const Xv_pkg xv_talk_pkg = {
	"Talk",
	ATTR_PKG_TALK,
	sizeof(Xv_talk),
	SELECTION_REQUESTOR,
	talk_init,
	talk_set,
	talk_get,
	talk_destroy,
	talk_find
};
