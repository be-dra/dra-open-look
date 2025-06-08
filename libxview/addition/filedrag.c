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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
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
#include <sys/utsname.h>
#include <fcntl.h>
#include <xview/filedrag.h>
#include <xview/filereq.h>
#include <xview/font.h>
#include <xview/cursor.h>
#include <xview/frame.h>
#include <xview_private/svr_impl.h>

#ifndef lint
char filedrag_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: filedrag.c,v 4.9 2025/06/06 18:26:13 dra Exp $";
#endif

typedef struct {
	Xv_opaque       public_self;
	char            **files;
	int             num_files, current;
	filedrag_dnd_done_proc_t dnd_done_proc;
	Xv_cursor copy_1, copy_accept_1, copy_n, copy_accept_n;
	Xv_cursor move_1, move_accept_1, move_n, move_accept_n;
	Xv_cursor copy_reject_1, copy_reject_n, move_reject_1, move_reject_n;
	Filedrag_string_setting convert_string;
	char            allow_move, do_delete, is_move, send_filenames_on_STRING;
} FileDrag_private;

#define FILEDRAGPRIV(_x_) XV_PRIVATE(FileDrag_private, Xv_filedrag_public, _x_)
#define FILEDRAGPUB(_x_) XV_PUBLIC(_x_)

#define A0 *attrs
#define A1 attrs[1]
#define A2 attrs[2]
#define A3 attrs[3]

#define ADONE ATTR_CONSUME(*attrs);break

/* ARGSUSED */
static void filedrag_done_proc(FileDrag self, Xv_opaque buf, Atom target)
{
/* 	Xv_server server; */

/* 	server = XV_SERVER_FROM_WINDOW(xv_get(self, XV_OWNER)); */
/* 	DTRACE(DTL_SEL+70, "filedrag_done_proc called for target '%s'\n", */
/* 			(char *)xv_get(server, SERVER_ATOM_NAME, target)); */

	if (target == XA_STRING) {
		xv_free(buf);
	}
}

static void note_dnd_done(Xv_opaque clnt)
{
	FileDrag_private *priv = (FileDrag_private *)clnt;

	if (priv->dnd_done_proc) (*(priv->dnd_done_proc))(FILEDRAGPUB(priv),
													(int)priv->is_move);
}

static void internal_convert_namestring(FileDrag_private *priv, Xv_opaque *data,
								unsigned long *length, int sep)
{
	int i, len;
	char *buffer;
	char sepbuf[2];

	sepbuf[0] = (char)sep;
	sepbuf[1] = '\0';

	len = 4;
	for (i = 0; i < priv->num_files; i++)
		len += 1 + strlen(priv->files[i]);

	buffer = xv_malloc((unsigned long)len);
	*buffer = '\0';

	for (i = 0; i < priv->num_files; i++) {
		strcat(buffer, priv->files[i]);
		if (i + 1 < priv->num_files) strcat(buffer, sepbuf);
	}

	*length = strlen(buffer);
	*data = (Xv_opaque)buffer;
}

static int convert_string(FileDrag_private *priv, Xv_server server, Atom *type,
				Xv_opaque *data, unsigned long *length, int *format)
{
	int fd;
	struct stat sb;
	char *buffer;

	*type = XA_STRING;
	*format = 8;

	if (priv->send_filenames_on_STRING) {
		internal_convert_namestring(priv, data, length, '\n');
		return TRUE;
	}

	switch (priv->convert_string) {
		case FILEDRAG_NO_STRING: return FALSE;
		case FILEDRAG_NAME_STRING:
			internal_convert_namestring(priv, data, length, ' ');
			return TRUE;

		default: break;
	}

	if (stat(priv->files[priv->current], &sb)) return FALSE;

	if (! S_ISREG(sb.st_mode)) {
		internal_convert_namestring(priv, data, length, ' ');
		return TRUE;
	}

	*length = sb.st_size;

	if (sb.st_size == 0) {
		*data = (Xv_opaque)0;
		return TRUE;
	}

	if (!(buffer = xv_malloc((unsigned long)(sb.st_size + 1)))) return FALSE;

	if ((fd = open(priv->files[priv->current], O_RDONLY)) < 0) {
		free(buffer);
		return FALSE;
	}

	read(fd, buffer, (size_t)sb.st_size);
	close(fd);
	*data = (Xv_opaque)buffer;

	return TRUE;
}

static int convert_file_name(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	*type = XA_STRING;
	*format = 8;
	*length = strlen(priv->files[priv->current]);
	*data = (Xv_opaque)priv->files[priv->current];
	return TRUE;
}

static int convert_netscape_url(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	static char bigbuf[1000];

	/* they say this would be correct.
	 * However, nearly nobody seems to understand - 
	 * there are a lot of idiots out there...
	 */
#ifdef MANY_IDIOTS_DO_NOT_UNDERSTAND
	{
		struct utsname name;
		uname(&name);
		sprintf(bigbuf, "file://%s%s\r\n", name.nodename,
										priv->files[priv->current]);
	}
#else /* MANY_IDIOTS_DO_NOT_UNDERSTAND */
	/* so macht das der nautilus (der GNOME-Filemanager) -
	 * auch so ein lustiges Teilchen....
	 */
	sprintf(bigbuf, "file://%s", priv->files[priv->current]);
#endif /* MANY_IDIOTS_DO_NOT_UNDERSTAND */

	*format = 8;
	*length = strlen(bigbuf);
	*data = (Xv_opaque)bigbuf;
	return TRUE;
}

static int convert_base_name(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	char *p;

	if ((p = strrchr(priv->files[priv->current], '/'))) ++p;
	else p = priv->files[priv->current];

	*type = XA_STRING;
	*format = 8;
	*length = strlen(p);
	*data = (Xv_opaque)p;
	return TRUE;
}

static int convert_length(FileDrag_private *priv, Xv_server server, Atom *type,
				Xv_opaque *data, unsigned long *length, int *format)
{
	static int len;
	int i;

	if (priv->convert_string == FILEDRAG_NO_STRING) return FALSE;
	else if (priv->convert_string == FILEDRAG_NAME_STRING) {
		len = -1;
		for (i = 0; i < priv->num_files; i++) len += 1 + strlen(priv->files[i]);
	}
	else {
		struct stat sb;

		if (stat(priv->files[priv->current], &sb)) return FALSE;

		len = sb.st_size;
	}

	*type = XA_INTEGER;
	*format = 32;
	*length = 1;
	*data = (Xv_opaque)&len;
	return TRUE;
}

static int convert_length_type(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	static int len;
	struct stat sb;

	if (stat(priv->files[priv->current], &sb)) return FALSE;

	len = sb.st_size;

	*type = XA_INTEGER;
	*format = 32;
	*length = 1;
	*data = (Xv_opaque)&len;
	return TRUE;
}

static int convert_file_stat(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	static Filereq_remote_stat fs;
	struct stat sb;

	if (stat(priv->files[priv->current], &sb)) return FALSE;
	fs.inode = (long)sb.st_ino;
	fs.mtime = (long)sb.st_mtime;
	fs.size = (long)sb.st_size;

	/* type = target */
	*format = 32;
	*length = sizeof(fs) / sizeof(long);
	*data = (Xv_opaque)&fs;
	return TRUE;
}

static int convert_enum_count(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	*type = XA_INTEGER;
	*data = (Xv_opaque)&priv->num_files;
	*length = 1;
	*format = 32;
	return TRUE;
}

static int convert_transport_meth(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	static Atom trans;

	trans = (Atom)xv_get(server, SERVER_ATOM, "_SUN_ATM_FILE_NAME");
	*type = XA_ATOM;
	*data = (Xv_opaque)&trans;
	*length = 1;
	*format = 32;
	return TRUE;
}

static int convert_enum_item(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	Sel_prop_info *info;

	if ((info = (Sel_prop_info *)xv_get(FILEDRAGPUB(priv), SEL_PROP_INFO)) &&
		info->length &&
		info->format == 32 &&
		info->type == XA_INTEGER) {
		priv->current = *((int *)info->data);

		if (priv->current >= priv->num_files) {
			priv->current = 0;
			return FALSE;
		}
		if (priv->current < 0) priv->current = 0;
	}
	else priv->current = 0;

	*type = XA_INTEGER;
	*data = (Xv_opaque)NULL;
	*length = 0;
	*format = 32;
	return TRUE;
}

static int convert_kde_text_uri_list(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	int i, len, nlen;
	struct utsname name;
	char *buf, *cur;

	uname(&name);
	nlen = 10 + strlen(name.nodename); /* comes from file://hostname/file\n" */
	/* sprintf(bigbuf, "file://%s%s\r\n", name.nodename, priv->files[priv->current]); */
	for (i = 0, len = 0; i < priv->num_files; i++) {
		len += nlen;
		len += strlen(priv->files[i]);
	}

	buf = xv_malloc((unsigned long)len);

	for (i = 0, cur = buf; i < priv->num_files; i++) {
		/* firefox and kwrite would understand this: */
		sprintf(cur, "file://%s%s\r\n", name.nodename, priv->files[i]);
#ifdef MANY_IDIOTS_DO_NOT_UNDERSTAND
		/* idiots included:
		 * + konqueror  (drag source)
		 * + nautilus   (drag source)
		 * + audacity   (drop target)
		 */
		sprintf(cur, "file://%s\r\n", priv->files[i]);
#endif /* MANY_IDIOTS_DO_NOT_UNDERSTAND */
		cur += strlen(cur);
	}

	*format = 8;
	*length = strlen(buf);
	*data = (Xv_opaque)buf;
	return TRUE;
}

static int convert_next_file(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	if (++priv->current >= priv->num_files) {
		priv->current = 0;
		return FALSE;
	}

	return convert_file_name(priv, server, type, data, length, format);
}

static int convert_sel_end(FileDrag_private *priv, Xv_server server,
				Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	xv_set(FILEDRAGPUB(priv), SEL_OWN, FALSE, NULL);
	*format = 32;
	*length = 0;
	*data = XV_NULL;
	*type = (Atom)xv_get(server, SERVER_ATOM, "NULL");
	priv->send_filenames_on_STRING = FALSE;
	if (priv->dnd_done_proc) xv_perform_soon((Xv_opaque)priv, note_dnd_done);
	return TRUE;
}

static int convert_string_is_filenames(FileDrag_private *priv, Xv_server server,
		Atom *type, Xv_opaque *data, unsigned long *length, int *format)
{
	*format = 32;
	*length = 0;
	*data = XV_NULL;
	*type = (Atom)xv_get(server, SERVER_ATOM, "NULL");
	priv->send_filenames_on_STRING = TRUE;
	return TRUE;
}

static int convert_delete(FileDrag_private *priv, Xv_server server, Atom *type,
				Xv_opaque *data, unsigned long *length, int *format)
{
	if (priv->do_delete) {
		/* wir loeschen keine Dateien, wenn vorher einer gesagt hat
		 * _DRA_STRING_IS_FILENAMES - denn danach kommt wahrscheinlich die
		 * blanke XView-TEXTSW-Welt dran - und die schickt gnadenlos
		 * DELETE - glaubt aber, nur den selektierten TEXT zu loeschen....
		 */
		if (! priv->send_filenames_on_STRING) {
			if (unlink(priv->files[priv->current])) return FALSE;
		}
	}

	*format = 32;
	*length = 0;
	*data = XV_NULL;
	*type = (Atom)xv_get(server, SERVER_ATOM, "NULL");
	return TRUE;
}


static struct { char *name;
	int (*convert)(FileDrag_private *,Xv_server,Atom *,Xv_opaque *,
							unsigned long *, int *);
} sels[] = {
	{ "DELETE", convert_delete },
	{ "FILE_NAME", convert_file_name },
	{ "_NETSCAPE_URL", convert_netscape_url },
	{ "_SUN_ALTERNATE_TRANSPORT_METHODS", convert_transport_meth },
	{ "_SUN_ENUMERATION_ITEM", convert_enum_item },
	{ "_SUN_ENUMERATION_COUNT", convert_enum_count },
	{ "_SUN_DATA_LABEL", convert_base_name },
	{ "_DRA_STRING_IS_FILENAMES", convert_string_is_filenames },
	{ "_SUN_DRAGDROP_DONE", convert_sel_end },
	/* Xt-applications cannot use "_SUN_ENUMERATION_ITEM" */
	{ "_DRA_NEXT_FILE", convert_next_file },
	{ "_DRA_FILE_STAT", convert_file_stat },
	{ "_SUN_LENGTH_TYPE", convert_length_type },
	{ "LENGTH", convert_length },
	{ "text/uri-list", convert_kde_text_uri_list },
/* lieber nicht: der will sich nach China verbinden oder sowas... */
/* 	{ "text/x-moz-url", convert_netscape_url }, */
	{ "STRING", convert_string } /* must be the last */
};

#define NUM_SEL (sizeof(sels)/sizeof(sels[0]))

static int filedrag_convert_selection(FileDrag_private *priv, Atom *target,
				Xv_opaque *data, unsigned long *length, int *format)
{
	Xv_server server;
	unsigned i;

	server = XV_SERVER_FROM_WINDOW(xv_get(FILEDRAGPUB(priv),XV_OWNER));

	SERVERTRACE((300, "filedrag_convert_selection called for target '%s'\n",
				(char *)xv_get(server, SERVER_ATOM_NAME, *target)));

	if (! priv->files) return False;

	for (i = 0; i < NUM_SEL; i++) {
		if (*target == (Atom)xv_get(server, SERVER_ATOM, sels[i].name)) {
			return (*(sels[i].convert))(priv, server,
								target, data, length, format);
		}
	}

	return False;
}

static void process_files(FileDrag_private *priv, char **files)
{
	unsigned i, cnt;
	Xv_cursor curs, acccurs, rejcurs;

	if (priv->files) {
		for (i = 0; priv->files[i]; i++) free(priv->files[i]);
		free((char *)priv->files);
		priv->files = (char **)0;
	}

	cnt = 0;
	if (files) {
		for (cnt = 0; files[cnt]; cnt++);

		priv->files = (char **)xv_alloc_n(char *, (unsigned long)cnt + 1);

		for (i = 0; i < cnt; i++) {
			priv->files[i] = xv_strsave(files[i]);
/* 			DTRACE(DTL_SET, "\tfiles[%d] = '%s'\n", i, priv->files[i]); */
		}
	}

	priv->num_files = cnt;
	priv->current = 0;

	if (priv->is_move) {
		curs = (cnt <= 1) ? priv->move_1 : priv->move_n;
		acccurs = (cnt <= 1) ? priv->move_accept_1 : priv->move_accept_n;
		rejcurs = (cnt <= 1) ? priv->move_reject_1 : priv->move_reject_n;
	}
	else {
		curs = (cnt <= 1) ? priv->copy_1 : priv->copy_n;
		acccurs = (cnt <= 1) ? priv->copy_accept_1 : priv->copy_accept_n;
		rejcurs = (cnt <= 1) ? priv->copy_reject_1 : priv->copy_reject_n;
	}

	xv_set(FILEDRAGPUB(priv),
					DND_CURSOR, curs,
					DND_ACCEPT_CURSOR, acccurs,
					DND_REJECT_CURSOR, rejcurs,
					NULL);
}

static int filedrag_init(Xv_window owner, Xv_opaque xself, Attr_avlist avlist,
							int *unused)
{
	Xv_filedrag_public *self = (Xv_filedrag_public *)xself;
	FileDrag_private *priv = (FileDrag_private *)xv_alloc(FileDrag_private);
	unsigned amsrc, rmsrc, msrc;
	unsigned namsrc, nrmsrc, nmsrc;
	unsigned acsrc, rcsrc, csrc;
	unsigned nacsrc, nrcsrc, ncsrc;
	Xv_font cursfont;
	XFontStruct *info;

	if (!priv) return XV_ERROR;

/* 	DTRACE(DTL_INTERN+50, "in filedrag_init(%ld)\n", self); */
	priv->public_self = xself;
	self->private_data = (Xv_opaque)priv;

	xv_set((Xv_opaque)self, SEL_DONE_PROC, filedrag_done_proc, NULL);

	cursfont = xv_find(owner, FONT, FONT_FAMILY, FONT_FAMILY_OLCURSOR, NULL);
	info = (XFontStruct *)xv_get(cursfont, FONT_INFO);

	if (info->max_char_or_byte2 >= 0x7c) {
		/* new open look cursor font */
/* 		fprintf(stderr, "in filedrag`filedrag_init: new OL-Cursorfont\n"); */
		csrc = OLC_DOC_COPY_DRAG;
		acsrc = OLC_DOC_COPY_DROP;
		rcsrc = OLC_DOC_COPY_NODROP;

		ncsrc = OLC_DOCS_COPY_DRAG;
		nacsrc = OLC_DOCS_COPY_DROP;
		nrcsrc = OLC_DOCS_COPY_NODROP;

		msrc = OLC_DOC_MOVE_DRAG;
		amsrc = OLC_DOC_MOVE_DROP;
		rmsrc = OLC_DOC_MOVE_NODROP;

		nmsrc = OLC_DOCS_MOVE_DRAG;
		namsrc = OLC_DOCS_MOVE_DROP;
		nrmsrc = OLC_DOCS_MOVE_NODROP;
	}
	else {
		/* old open look cursor font */
		ncsrc = nacsrc = nrcsrc = csrc = acsrc = rcsrc = OLC_COPY_PTR;
		nmsrc = namsrc = nrmsrc = msrc = amsrc = rmsrc = OLC_MOVE_PTR;
	}

	priv->copy_accept_1 = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, acsrc,
					CURSOR_MASK_CHAR, acsrc+1,
					NULL);

	priv->copy_reject_1 = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, rcsrc,
					CURSOR_MASK_CHAR, rcsrc+1,
					NULL);

	priv->copy_1 = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, csrc,
					CURSOR_MASK_CHAR, csrc+1,
					NULL);

	priv->copy_accept_n = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, nacsrc,
					CURSOR_MASK_CHAR, nacsrc+1,
					NULL);

	priv->copy_reject_n = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, nrcsrc,
					CURSOR_MASK_CHAR, nrcsrc+1,
					NULL);

	priv->copy_n = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, ncsrc,
					CURSOR_MASK_CHAR, ncsrc+1,
					NULL);

	priv->move_accept_1 = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, amsrc,
					CURSOR_MASK_CHAR, amsrc+1,
					NULL);

	priv->move_reject_1 = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, rmsrc,
					CURSOR_MASK_CHAR, rmsrc+1,
					NULL);

	priv->move_1 = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, msrc,
					CURSOR_MASK_CHAR, msrc+1,
					NULL);

	priv->move_accept_n = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, namsrc,
					CURSOR_MASK_CHAR, namsrc+1,
					NULL);

	priv->move_reject_n = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, nrmsrc,
					CURSOR_MASK_CHAR, nrmsrc+1,
					NULL);

	priv->move_n = xv_create(owner, CURSOR,
					CURSOR_SRC_CHAR, nmsrc,
					CURSOR_MASK_CHAR, nmsrc+1,
					NULL);

	return XV_OK;
}

static void free_key_data(Xv_opaque obj, int key, char *data)
{
	if (data) xv_free(data);
}

static Xv_opaque filedrag_set(FileDrag self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Filedrag_string_setting enab;
	int max, i;
	FileDrag_private *priv = FILEDRAGPRIV(self);

/* 	DTRACE(DTL_SET, "filedrag_set(%ld, %ld)\n", self, FILEDRAGPUB(priv)); */
	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case FILEDRAG_FILES:
			process_files(priv, (char **)&A1);
			ADONE;

		case FILEDRAG_FILE_ARRAY:
/* 			DTRACE(DTL_SET, "filedrag_set(%ld, %ld): new file array\n", */
/* 									self, FILEDRAGPUB(priv)); */
			process_files(priv, (char **)A1);
/* 			DTRACE(DTL_SET, "filedrag_set: finished with new file array\n"); */
			ADONE;

		case FILEDRAG_ALLOW_MOVE:
			priv->allow_move = (char)A1;
			ADONE;

		case FILEDRAG_ENABLE_STRING:
			enab = (Filedrag_string_setting)A1;
			if (enab == FILEDRAG_NO_STRING &&
				priv->convert_string != FILEDRAG_NO_STRING)
			{
				xv_set(self, ICCC_REMOVE_TARGET, XA_STRING, NULL);
			}

			if (enab != FILEDRAG_NO_STRING &&
				priv->convert_string == FILEDRAG_NO_STRING)
			{
				xv_set(self, ICCC_ADD_TARGET, XA_STRING, NULL);
			}

			priv->convert_string = enab;
			ADONE;

		case FILEDRAG_DND_DONE_PROC:
			priv->dnd_done_proc = (filedrag_dnd_done_proc_t)A1;
			ADONE;

		case FILEDRAG_DO_DELETE:
			priv->do_delete = (char)A1;
			ADONE;

		case DND_TYPE:
/* 			DTRACE(DTL_SET, "filedrag_set: DND_TYPE=DND%s\n", */
/* 					(((DndDragType)A1 == DND_COPY) ? "COPY" : "MOVE")); */
			if (!priv->allow_move) {
				A1 = (Attr_attribute)DND_COPY;
			}

			priv->is_move = (A1 == (Attr_attribute)DND_MOVE);
/* 			DTRACE(DTL_SET, "filedrag_set: priv->is_move = %d\n", priv->is_move); */
			break;

		case XV_END_CREATE:
			max = NUM_SEL;
			if (priv->convert_string == FILEDRAG_NO_STRING) max--;

			for (i = 0; i < max; i++) {
				xv_set(self, ICCC_ADD_TARGET_NAME, sels[i].name, NULL);
			}

			/* Convention: we are dragging **files**   */
			{
				Xv_server server = XV_SERVER_FROM_WINDOW(xv_get(self,XV_OWNER));
				Atom *tl = xv_calloc(4, (unsigned)sizeof(Atom));

				tl[0] = xv_get(server, SERVER_ATOM, "FILE_NAME");

				xv_set(self,
						XV_KEY_DATA, SEL_NEXT_ITEM, tl,
						XV_KEY_DATA_REMOVE_PROC, SEL_NEXT_ITEM, free_key_data,
						NULL);
			}
			break;

		default: xv_check_bad_attr(FILEDRAG, A0);
			break;
	}

	return XV_OK;
}

static Xv_opaque filedrag_get(FileDrag self, int *status, Attr_attribute attr,
				va_list vali)
{
	FileDrag_private *priv = FILEDRAGPRIV(self);
	iccc_convert_t *cvt;
	int *cnt;

	*status = XV_OK;
	switch ((int)attr) {
		case ICCC_CONVERT:
			cvt = va_arg(vali, iccc_convert_t *);
			if (! filedrag_convert_selection(priv, cvt->type,
								cvt->value, cvt->length, cvt->format)) {
				*status = XV_ERROR;
			}
			else {
				return (Xv_opaque)TRUE;
			}
			break;

		case FILEDRAG_FILE_ARRAY:
			cnt = va_arg(vali, int *);
			if (cnt) *cnt = priv->num_files;
			return (Xv_opaque)priv->files;

		case FILEDRAG_ALLOW_MOVE: return (Xv_opaque)priv->allow_move;
		case FILEDRAG_DND_DONE_PROC: return (Xv_opaque)priv->dnd_done_proc;
		case FILEDRAG_ENABLE_STRING: return (Xv_opaque)priv->convert_string;
		case FILEDRAG_DO_DELETE: return (Xv_opaque)priv->do_delete;
		case FILEDRAG_CURRENT_FILE:
			if (priv->files && priv->current < priv->num_files)
				return (Xv_opaque)priv->files[priv->current];
			else return XV_NULL;
		default:
			*status = xv_check_bad_attr(FILEDRAG, attr);

	}
	return (Xv_opaque)XV_OK;
}

static int filedrag_destroy(Xv_opaque self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		FileDrag_private *priv = FILEDRAGPRIV(self);

		if (priv->files) {
			int i;

			for (i = 0; priv->files[i]; i++) free(priv->files[i]);
			free((char *)priv->files);
		}

		xv_destroy(priv->copy_1);
		xv_destroy(priv->copy_accept_1);
		xv_destroy(priv->copy_accept_n);
		xv_destroy(priv->copy_n);
		xv_destroy(priv->copy_reject_1);
		xv_destroy(priv->copy_reject_n);
		xv_destroy(priv->move_1);
		xv_destroy(priv->move_accept_1);
		xv_destroy(priv->move_accept_n);
		xv_destroy(priv->move_n);
		xv_destroy(priv->move_reject_1);
		xv_destroy(priv->move_reject_n);

		free((char *)priv);
	}
	return XV_OK;
}

const Xv_pkg xv_filedrag_pkg = {
	"FileDrag",
	ATTR_PKG_FILEDRAG,
	sizeof(Xv_filedrag_public),
	ICCC,
	filedrag_init,
	filedrag_set,
	filedrag_get,
	filedrag_destroy,
	0
};
