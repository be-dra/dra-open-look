#include <string.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <time.h>
#include <xview/talk.h>
#include <xview/defaults.h>

char talk_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: talk.c,v 1.39 2025/04/13 14:33:49 dra Exp $";

typedef struct _pattern {
	struct _pattern *next;
	char *name;
	Atom client_selection;
} *pattern_t;

typedef enum {
	initialized,
	registering,
	registered
} Talk_state_t;

typedef struct {
	Xv_opaque       public_self;

	Selection_owner selown;
	Atom            pattern, trigg, regist, deregist, talksrv, targets;
	int             last_error;
	int             silent;
	int             notify_count;
	talk_notify_proc_t   notify_proc;
	Talk_state_t    state;
	pattern_t       not_yet_installed;
	pattern_t       installed;
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
static void register_to_server(Talk self, Talk_private *priv);

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

static Notify_value note_try_again(Talk self, int unused)
{
	Talk_private *priv = TALKPRIV(self);
	int savesil = priv->silent;

	priv->silent = TRUE;
	register_to_server(self, priv);
	priv->silent = savesil;

	if (priv->selown) {
		notify_set_itimer_func(self, NOTIFY_TIMER_FUNC_NULL, ITIMER_REAL,
									NULL, NULL);
	}
	return NOTIFY_DONE;
}

static void registration_failed(Talk_private *priv, int selerr)
{
	static char *selerrors[] = {
		"BAD_TIME",
		"BAD_WIN_ID",
		"BAD_PROPERTY",
		"BAD_CONVERSION",
		"TIMEDOUT",
		"PROPERTY_DELETED",
		"BAD_PROPERTY_EVENT"
	};
	Talk self = TALKPUB(priv);
	struct itimerval timer;
	char errbuf[200];

	sprintf(errbuf, "Talk registration failed with code %s, will try again",
									selerrors[selerr]);
	if (priv->silent) {
		/* I still want to know: */
		fprintf(stderr, "%s: %s\n", xv_app_name, errbuf);
	}
	else {
		xv_error(self,
			ERROR_PKG, TALK,
			ERROR_STRING, errbuf,
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);
	}

	/* The whole concept is based on the assumption that there is
	 * **one** process that plays the server role.
	 * In case a potential client is started earlier (or simply faster)
	 * than the server, we establish a "retry mechanism"
	 */

	timer.it_value.tv_sec = 60;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 60;
	timer.it_interval.tv_usec = 0;
	notify_set_itimer_func(self, note_try_again, ITIMER_REAL, &timer, NULL);
}

static void install_pattern(Talk_private *priv, char *patt)
{
	Talk self = TALKPUB(priv);
	char buf[100];
	unsigned long len;
	int format;

	priv->last_error = -1;
	if (! priv->selown) register_to_server(self, priv);
	if (priv->last_error >= 0) return;

	sprintf(buf, "%ld%c%s", xv_get(priv->selown, SEL_RANK), AGS, patt);
	xv_set(self,
			SEL_RANK, priv->talksrv,
			SEL_TYPE, priv->pattern,
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

	install_pattern(priv, p->name);
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

	if (priv->notify_proc) {
		priv->notify_proc(TALKPUB(priv), message, host, pid, params);
	}
}

static int client_convert_proc(Selection_owner sel_own, Atom *type,
					Xv_opaque *data, unsigned long *length, int *format)
{
	Talk_private *priv = (Talk_private *)xv_get(sel_own, XV_KEY_DATA, talk_key);

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
	Talk_private *priv = (Talk_private *)xv_get(sel_own, XV_KEY_DATA, talk_key);
	Talk self = TALKPUB(priv);

	xv_set(self,
			SEL_RANK, priv->talksrv,
			SEL_TYPE, priv->regist,
			NULL);
	sel_post_req(self);
}

static void registration_done(Talk_private *priv, Atom sel_atom)
{
	Talk self = TALKPUB(priv);

	/* can be a first registration or a re-registration from client_lose */
	if (priv->selown) {
		pattern_t p;

		/* this is a re-registration */
		xv_set(priv->selown, SEL_RANK, sel_atom, NULL);

		for (p = priv->installed; p; p = p->next) {
			install_pattern(priv, p->name);
		}
	}
	else {
		priv->state = registered;
		priv->selown = xv_create(xv_get(self, XV_OWNER), SELECTION_OWNER,
				XV_KEY_DATA, talk_key, priv,
				SEL_RANK, sel_atom,
				SEL_CONVERT_PROC, client_convert_proc,
				SEL_DONE_PROC, client_done,
				SEL_LOSE_PROC, client_lose,
				NULL);

		/* now deal with not yet installed patterns */
		priv->not_yet_installed = install_pending(priv, priv->not_yet_installed);
	}
	xv_set(priv->selown, SEL_OWN, TRUE, NULL);
}

static void note_client_reply(Talk self, Atom target, Atom type,
					Xv_opaque value, unsigned long length, int format)
{
	Talk_private *priv = TALKPRIV(self);

	if (target == priv->regist) {
		if (length == SEL_ERROR) {
			registration_failed(priv,  *((int *)value));
		}
		else {
			Atom *at = (Atom *)value;

			registration_done(priv, *at);
			xv_free(value);
		}
		return;
	}

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

static int talk_init(Xv_window owner, Xv_opaque slf, Attr_avlist avlist, int *u)
{
	Xv_talk *self = (Xv_talk *)slf;
	Talk_private *priv;
	Xv_server srv = XV_SERVER_FROM_WINDOW(owner);

	if (!(priv = (Talk_private *)xv_alloc(Talk_private))) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	if (! talk_key) talk_key = xv_unique_key();
	priv->silent = TRUE;

	if (xv_get(srv, XV_KEY_DATA, talk_key)) {
		xv_error(slf,
			ERROR_PKG, TALK,
			ERROR_STRING, "Already a TALK instance in this process",
			ERROR_SEVERITY, ERROR_RECOVERABLE,
			NULL);
		return XV_ERROR;
	}
	xv_set(srv, XV_KEY_DATA, talk_key, slf, NULL);

	priv->talksrv = xv_get(srv, SERVER_ATOM, "_DRA_TALK_SERVER");
	priv->trigg = xv_get(srv, SERVER_ATOM, "_DRA_TALK_TRIGGER");
	priv->regist = xv_get(srv, SERVER_ATOM, "_DRA_TALK_REGISTER");
	priv->deregist = xv_get(srv, SERVER_ATOM, "_DRA_TALK_DEREGISTER");
	priv->pattern = xv_get(srv, SERVER_ATOM, "_DRA_TALK_PATTERN");
	priv->targets = xv_get(srv, SERVER_ATOM, "TARGETS");

	xv_set(slf, SEL_REPLY_PROC, note_client_reply, NULL);

	return XV_OK;
}
static void deregister(Talk self, Talk_private *priv)
{
	Atom at = xv_get(priv->selown, SEL_RANK);
	xv_set(self,
			SEL_RANK, priv->talksrv,
			SEL_TYPE, priv->deregist,
			SEL_TYPE_INDEX, 0,
			SEL_PROP_TYPE, XA_ATOM,
			SEL_PROP_FORMAT, 32,
			SEL_PROP_LENGTH, 1,
			SEL_PROP_DATA, &at,
			NULL);
	sel_post_req(self);
}

static void register_to_server(Talk self, Talk_private *priv)
{
	if (priv->selown) return;

	priv->last_error = -1;
	xv_set(self,
			SEL_RANK, priv->talksrv,
			SEL_TYPE, priv->regist,
			NULL);
	sel_post_req(self);
}

static void add_pattern(Talk_private *priv, char *patt)
{
	pattern_t p = xv_alloc(struct _pattern);

	p->name = strdup(patt);
	p->next = priv->not_yet_installed;
	priv->not_yet_installed = p;
}

static void call_server(Talk t, int bufsize, const char *msg, char **params,
							int *countptr, char *host)
{
	Talk_private *priv = TALKPRIV(t);
	char *p, sep[2];

	/* this is a VLA (variable length array) */
	char buf[bufsize]; /* similar to alloca */

	/* compare REF (mbfdsvnbserf) */
	sprintf(buf, "%s%c%d%c%s%c%s", msg,       /* gsstrings[0] */
							AGS, getpid(),    /* gsstrings[1] */
							AGS, xv_app_name, /* gsstrings[2] */
							AGS, host);       /* gsstrings[3] */
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
			SEL_RANK, priv->talksrv,
			SEL_TYPE, priv->trigg,
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
	char *p;
	size_t bufsize;
	struct utsname u;

	uname(&u);
	bufsize = strlen(msg) + strlen(u.nodename) + 3;
	if (params) {
		int i = 0;

		while ((p = params[i++])) {
			bufsize += strlen(p) + 2;
		}
	}

	call_server(t, (int)bufsize, msg, params, countptr, u.nodename);
}

static void new_pattern(Talk_private *priv, char *patt)
{
	add_pattern(priv, patt);

	switch (priv->state) {
		case initialized:
			register_to_server(TALKPUB(priv), priv);
			priv->state = registering;
			break;

		case registering:
			/* do nothing, we still wait ... */
			break;

		case registered:
			priv->not_yet_installed = install_pending(priv,
											priv->not_yet_installed);
			break;
	}
}

static Xv_opaque talk_set(Talk self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Talk_private *priv = TALKPRIV(self);
	char *msg = NULL, **pars = NULL;
	int *count_ptr = NULL;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case TALK_NOTIFY_PROC:
			priv->notify_proc = (talk_notify_proc_t)A1;
			ADONE;

		case TALK_PATTERN:
			new_pattern(priv, (char *)A1);
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

		default: xv_check_bad_attr(TALK, A0);
			break;
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
	if ((self = xv_get(srv, XV_KEY_DATA, talk_key))) return self;
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
