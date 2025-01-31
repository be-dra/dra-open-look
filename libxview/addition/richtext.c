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
#include <xview/xview.h>
#include <xview/font.h>
#include <xview/cms.h>
#include <xview/richtext.h>
#include <xview_private/svr_impl.h>

char richtext_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: richtext.c,v 1.7 2024/12/13 20:52:26 dra Exp $";

typedef enum {
	RT_NORMAL,
	RT_BOLD,
	RT_ITALICS,
	RT_BOLDITALICS,
	__RT_MODE_COUNT
} rt_textmode_t;

typedef struct _textblock {
	struct _textblock *next;

	rt_textmode_t mode;
	int fileline;
	int viewline;
	int x, width;
	char text[800];
} *textblock_t;

typedef struct {
	Xv_opaque public_self;

	GC gcs[__RT_MODE_COUNT];
	Xv_font fonts[__RT_MODE_COUNT];
	rt_textmode_t lastmode;
	int last_fileline;
	int last_viewline;
	textblock_t list; /* created by append_line / append_block */
	textblock_t current;  /* transient in append_block */
	textblock_t paintlist; /* derived from list depending on XV_WIDTH */
	int lineheight;
	int blank_width;
	int margin_width;
	int report_long_lines;
	int curwidth; /* transient set by RICHTEXT_START */
	int which_list_to_paint;
} Richtext_private;

#define A0 *attrs
#define A1 attrs[1]
#define RICHPRIV(_x_) XV_PRIVATE(Richtext_private, Xv_richtext, _x_)
#define RICHPUB(_x_) XV_PUBLIC(_x_)

#define ADONE ATTR_CONSUME(*attrs);break

static void free_list(textblock_t tb)
{
	if (tb) free_list(tb->next);
	xv_free(tb);
}

static void append_block(Richtext_private *priv, char *txt, int xpos)
{
	textblock_t b;
	char *p, *bs;
	Font_string_dims dims;

	SERVERTRACE((700, "append_block(%s), &dims=%p\n", txt, &dims));
	b = xv_alloc(struct _textblock);
	if (0 == strncmp(txt, "\\it", 3L)) {
		b->mode = RT_ITALICS;
		p = txt + 3;
	}
	else if (0 == strncmp(txt, "\\bo", 3L)) {
		b->mode = RT_BOLD;
		p = txt + 3;
	}
	else if (0 == strncmp(txt, "\\bi", 3L)) {
		b->mode = RT_BOLDITALICS;
		p = txt + 3;
	}
	else if (0 == strncmp(txt, "\\no", 3L)) {
		b->mode = RT_NORMAL;
		p = txt + 3;
	}
	else {
		b->mode = RT_NORMAL;
		p = txt;
	}
	b->fileline = priv->last_fileline;
	b->viewline = b->fileline;  /* nur erstmal */
	b->x = xpos;

	if (priv->current) {
		priv->current->next = b;
		priv->current = b;
	}
	else {
		priv->list = priv->current = b;
	}
	strncpy(b->text, p, sizeof(b->text)-1);
	p = b->text;

	bs = strchr(p, '\\');
	if (bs) {
		if (bs == p) { /* da gab es einen Fall "\i" - sehr obskur */
			xv_get(priv->fonts[b->mode], FONT_STRING_DIMS, b->text, &dims);
		}
		else {
			*bs = '\0';
			xv_get(priv->fonts[b->mode], FONT_STRING_DIMS, b->text, &dims);
			*bs = '\\';
			append_block(priv, bs, xpos + dims.width /* + priv->blank_width */ );
			*bs = '\0';
		}
	}
	else {
		xv_get(priv->fonts[b->mode], FONT_STRING_DIMS, b->text, &dims);
	}
	b->width = dims.width;

	if (priv->report_long_lines) {
		if (b->x + b->width > priv->curwidth) {
			fprintf(stderr, "%s`%s-%d: text in line %d too long: '%s'\n",
								__FILE__, __FUNCTION__,__LINE__,
								b->fileline, b->text);
		}
	}
}

static void append_line(Richtext_private *priv, char *txt)
{
	struct _textblock tb;
	char *p;

	++priv->last_fileline;
	if ((p = strchr(txt, '\n'))) *p = '\0';

	if (strlen(txt) >= sizeof(tb.text)) {
		fprintf(stderr, "text too long: '%s'\n", txt);
	}

	append_block(priv, txt, priv->margin_width);
	xv_set(RICHPUB(priv),
			SCROLLWIN_V_OBJECT_LENGTH, priv->last_fileline + 1,
			NULL);
}

static void update_coordinates(Richtext_private *priv)
{
	textblock_t b;
	int last_fileline = 0;
	int next_x = priv->margin_width;

	for (b = priv->list; b; b = b->next) {
		Font_string_dims dims;

		xv_get(priv->fonts[b->mode], FONT_STRING_DIMS, b->text, &dims);
		if (b->fileline == last_fileline) {
			b->x = next_x;
			next_x += dims.width;
		}
		else {
			b->x = priv->margin_width;
			next_x = b->x + dims.width;
			last_fileline = b->fileline;
		}
		b->width = dims.width;
	}
	xv_set(RICHPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
}

static int make_paintlist(Richtext_private *priv, int windowwidth)
{
	textblock_t b, newb;
	textblock_t bb, last_broken_block = NULL;
	textblock_t cur_paint = NULL;
	Font_string_dims dims;
	int last_fileline = 0;
	int last_paintline = 0;

/* 	fprintf(stderr, "%s-%d\n", __FUNCTION__, __LINE__); */
	free_list(priv->paintlist);
	priv->paintlist = NULL;

	windowwidth -= priv->margin_width;

	/* we assume that in 'list', the attributes x and width are correct */
	for (b = priv->list; b; b = b->next) {
/* 		fprintf(stderr, "%s-%d: b=%p\n", __FUNCTION__, __LINE__, b); */
		if (last_broken_block) {
			textblock_t p = b;
			int lbbx, lbbw;

/* 			fprintf(stderr, "%s-%d: fl=%d, x=%d, fll=%d, xl=%d\n", */
/* 				__FUNCTION__, __LINE__, b->fileline, b->x, */
/* 				last_broken_block->fileline, last_broken_block->x); */

			lbbx = last_broken_block->x;
			lbbw = last_broken_block->width;
			while (p && last_broken_block->fileline == p->fileline) {
				p->x = lbbx + lbbw;
				lbbx = p->x;
				lbbw = p->width;
				p = p->next;
			}
			last_broken_block = NULL;
		}
		if (b->fileline == last_fileline) {
			/* leave last_paintline unchanged */
		}
		else {
			/* a new fileline leads always to a new paintline */
			++last_paintline;
			last_fileline = b->fileline;
		}
/* 		fprintf(stderr, "%s-%d: b=%p\n", __FUNCTION__, __LINE__, b); */
		if (b->x + b->width > windowwidth) {
			char text[sizeof(b->text)];
			char *q;

/* 			fprintf(stderr, "%s-%d: w=%d\n", __FUNCTION__, __LINE__, windowwidth); */
			/* this block is too wide */
			strcpy(text, b->text);
			bb = b;

			for (;;) {
/* 				fprintf(stderr, "%s-%d: text='%s'\n", __FUNCTION__, __LINE__, text); */
				q = strrchr(text, ' ');
				if (q) {
					*q = '\0';
/* 					fprintf(stderr, "%s-%d: try '%s'\n", __FUNCTION__, __LINE__, text); */
					xv_get(priv->fonts[bb->mode], FONT_STRING_DIMS, text,&dims);
					if (bb->x + dims.width > windowwidth) {
						/* still too wide */
/* 						fprintf(stderr, "%s-%d, still too wide\n", __FUNCTION__, __LINE__); */
						continue;
					}
					else {
						char tmp[sizeof(b->text)];

						newb = xv_alloc(struct _textblock);
						*newb = *bb;
						strcpy(newb->text, text);
						newb->next = NULL;
						newb->viewline = last_paintline;
						++last_paintline;
/* 						fprintf(stderr, "%s-%d '%s' vl=%d\n", __FUNCTION__, __LINE__, newb->text, newb->viewline); */
						if (cur_paint) {
							cur_paint->next = newb;
						}
						else {
							priv->paintlist = newb;
						}
						cur_paint = newb;
						strcpy(text, bb->text);
/* 						fprintf(stderr, "%s-%d: text='%s'\n", __FUNCTION__, __LINE__, text); */
						++q;
/* 						fprintf(stderr, "%s-%d: text=%p, q=%p %ld\n", __FUNCTION__, __LINE__, text, q, q-text); */
						strcpy(tmp, q);
						strcpy(text, tmp);
/* 						fprintf(stderr, "%s-%d: text='%s'\n", __FUNCTION__, __LINE__, text); */
						newb = xv_alloc(struct _textblock);
						*newb = *bb;
						strcpy(newb->text, text);
/* 						fprintf(stderr, "%s-%d: new block, initial '%s'\n", __FUNCTION__, __LINE__, text); */
						newb->next = NULL;
						newb->viewline = last_paintline;
/* 						++last_paintline; */
						newb->x = priv->margin_width;
						xv_get(priv->fonts[bb->mode],
										FONT_STRING_DIMS, text, &dims);
						newb->width = dims.width;
						if (newb->x + newb->width > windowwidth) {
							/* the rest is still to wide */
							bb = newb;
/* 							fprintf(stderr, "%s-%d: try again\n", __FUNCTION__, __LINE__); */
							continue;
						}
						else {
							cur_paint->next = newb;
							cur_paint = newb;
						}
/* 						fprintf(stderr, "%s-%d\n", __FUNCTION__, __LINE__); */
					}
				}
				else {
					/* no blank - put the whole block into the 
					 * next viewline
					 */
					newb = xv_alloc(struct _textblock);
					*newb = *bb;
					newb->next = NULL;
					++last_paintline;
					newb->viewline = last_paintline;
					newb->x = priv->margin_width;
					if (cur_paint) cur_paint->next = newb;
					else priv->paintlist = newb;
					cur_paint = newb;
				}
				break;
			}
			last_broken_block = cur_paint;
		}
		else {
			newb = xv_alloc(struct _textblock);
			*newb = *b;
			newb->next = NULL;
			newb->viewline = last_paintline;
			if (cur_paint) cur_paint->next = newb;
			else priv->paintlist = newb;
			cur_paint = newb;
		}
/* 		fprintf(stderr, "%s-%d\n", __FUNCTION__, __LINE__); */
	}

/* 	fprintf(stderr, "%s-%d ================ RESULT =============\n", __FUNCTION__, __LINE__); */
/* 	for (b = priv->paintlist; b; b = b->next) { */
/* 		fprintf(stderr, "%3d: %3d %s\n", b->viewline, b->x, b->text); */
/* 	} */
	return last_paintline;
}

static void repaint_from_expose(Richtext_private *priv,
							Scrollwin_repaint_struct *rs)
{
	Rect fr;
	int scrx, scry;
	Display *dpy = rs->vinfo->dpy;
	Window xid = rs->vinfo->xid;
	textblock_t b;
	textblock_t pl;

	scrx = rs->vinfo->scr_x;
	scry = rs->vinfo->scr_y;

	fr.r_height = priv->lineheight;

	if (priv->which_list_to_paint == 0) pl = priv->list;
	else pl = priv->paintlist;

	for (b = pl; b; b = b->next) {

		if (rs->reason == SCROLLWIN_REASON_SCROLL) {
			XDrawString(dpy, xid, priv->gcs[b->mode], b->x - scrx,
										b->viewline * priv->lineheight - scry,
										b->text, (int)strlen(b->text));
		}
		else {
			fr.r_top = b->viewline * priv->lineheight - scry;
			fr.r_left = b->x - scrx;
			fr.r_width = b->width;

			if (rect_intersectsrect(&rs->win_rect, &fr)) {
				XDrawString(dpy, xid, priv->gcs[b->mode], fr.r_left, fr.r_top,
										b->text, (int)strlen(b->text));
			}
		}
	}
}

static void supply_fonts_and_gcs(Richtext_private *priv, Richtext self, Xv_font font)
{
	XGCValues gcv;
	Display *dpy = (Display *)xv_get(self, XV_DISPLAY);
	Window xid = (Window)xv_get(self, XV_XID);
	Xv_font f;
	Cms cms;
	int fore_index, back_index;
	unsigned long back, fore;
	XFontStruct *finfo;
	Font_string_dims dims;
	int lh = 0;


	cms = xv_get(self, WIN_CMS);  
	fore_index = (int)xv_get(self, WIN_FOREGROUND_COLOR);
	back_index = (int)xv_get(self, WIN_BACKGROUND_COLOR);

	back = (unsigned long)xv_get(cms, CMS_PIXEL, back_index);
	fore = (unsigned long)xv_get(cms, CMS_PIXEL, fore_index);

	gcv.foreground = fore;
	gcv.background = back;
	gcv.function = GXcopy;

	finfo = (XFontStruct *)xv_get(font, FONT_INFO);
	if (lh < finfo->ascent + finfo->descent) lh = finfo->ascent+finfo->descent;
	gcv.font = (Font)xv_get(font, XV_XID);
	xv_get(font, FONT_STRING_DIMS, " ", &dims);
	priv->blank_width = dims.width;

	if (priv->gcs[RT_NORMAL]) XFreeGC(dpy, priv->gcs[RT_NORMAL]);
	priv->gcs[RT_NORMAL] = XCreateGC(dpy, xid,
					GCFunction | GCForeground | GCBackground | GCFont,
					&gcv);
	priv->fonts[RT_NORMAL] = font;

	f = xv_find(XV_SERVER_FROM_WINDOW(self), FONT,
					FONT_FAMILY, xv_get(font, FONT_FAMILY),
					FONT_STYLE, FONT_STYLE_BOLD,
					FONT_SIZE, xv_get(font, FONT_SIZE),
					NULL);

	if (f) {
		finfo = (XFontStruct *)xv_get(f, FONT_INFO);
		if (lh < finfo->ascent + finfo->descent) lh = finfo->ascent+finfo->descent;
		gcv.font = (Font)xv_get(f, XV_XID);

		if (priv->gcs[RT_BOLD]) XFreeGC(dpy, priv->gcs[RT_BOLD]);
		priv->gcs[RT_BOLD] = XCreateGC(dpy, xid,
					GCFunction | GCForeground | GCBackground | GCFont,
					&gcv);
		priv->fonts[RT_BOLD] = f;
	}

	f = xv_find(XV_SERVER_FROM_WINDOW(self), FONT,
					FONT_FAMILY, xv_get(font, FONT_FAMILY),
					FONT_SIZE, xv_get(font, FONT_SIZE),
					FONT_STYLE, FONT_STYLE_ITALIC,
					NULL);

	if (f) {
		finfo = (XFontStruct *)xv_get(f, FONT_INFO);
		if (lh < finfo->ascent + finfo->descent) lh = finfo->ascent+finfo->descent;
		gcv.font = (Font)xv_get(f, XV_XID);

		if (priv->gcs[RT_ITALICS]) XFreeGC(dpy, priv->gcs[RT_ITALICS]);
		priv->gcs[RT_ITALICS] = XCreateGC(dpy, xid,
						GCFunction | GCForeground | GCBackground | GCFont,
					&gcv);
		priv->fonts[RT_ITALICS] = f;
	}

	f = xv_find(XV_SERVER_FROM_WINDOW(self), FONT,
					FONT_FAMILY, xv_get(font, FONT_FAMILY),
					FONT_SIZE, xv_get(font, FONT_SIZE),
					FONT_STYLE, FONT_STYLE_BOLD_ITALIC,
					NULL);

	if (f) {
		finfo = (XFontStruct *)xv_get(f, FONT_INFO);
		if (lh < finfo->ascent + finfo->descent) lh = finfo->ascent+finfo->descent;
		gcv.font = (Font)xv_get(f, XV_XID);

		if (priv->gcs[RT_BOLDITALICS]) XFreeGC(dpy, priv->gcs[RT_BOLDITALICS]);
		priv->gcs[RT_BOLDITALICS] = XCreateGC(dpy, xid,
						GCFunction | GCForeground | GCBackground | GCFont,
						&gcv);
		priv->fonts[RT_BOLDITALICS] = f;
	}

	priv->lineheight = lh;
	xv_set(self, SCROLLWIN_V_UNIT, lh, NULL);
}

static void filling_is_done(Richtext_private *priv, int width)
{
	priv->last_viewline = make_paintlist(priv, width);
	priv->which_list_to_paint = 1;
	xv_set(RICHPUB(priv),
			SCROLLWIN_V_OBJECT_LENGTH, priv->last_viewline + 1,
			SCROLLWIN_TRIGGER_REPAINT,
			NULL);
}


static int handle_events(Richtext_private *priv, Scrollwin_event_struct *es)
{
	if (es->action == ACTION_SELECT
		&& event_is_down(es->event)
		&& event_ctrl_is_down(es->event))
	{
		fprintf(stderr, "make_paintlist\n");
		filling_is_done(priv,  (int)xv_get(es->pw, XV_WIDTH));
	}
	else if (es->action == ACTION_ADJUST
		&& event_is_down(es->event)
		&& event_ctrl_is_down(es->event))
	{
		priv->which_list_to_paint = 0;
		xv_set(RICHPUB(priv), SCROLLWIN_TRIGGER_REPAINT, NULL);
	}

	/* do not consume */
	return FALSE;
}

static Xv_opaque richtext_set(Richtext self, Attr_avlist avlist)
{
	Attr_attribute *attrs;
	Richtext_private *priv = RICHPRIV(self);
	int consumed;
	Scrollwin_event_struct *es;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case RICHTEXT_START:
			free_list(priv->list);
			priv->list = NULL;
			free_list(priv->paintlist);
			priv->paintlist = NULL;
			priv->current = NULL;
			priv->lastmode = RT_NORMAL;
			priv->last_fileline = 0;
			priv->curwidth = (int)xv_get(xv_get(self, OPENWIN_NTH_VIEW, 0),
													XV_WIDTH);
			append_line(priv, (char *)A1);
			ADONE;

		case RICHTEXT_APPEND:
			append_line(priv, (char *)A1);
			ADONE;

		case RICHTEXT_FILLED:
			filling_is_done(priv, priv->curwidth);
			ADONE;

		case RICHTEXT_BASE_FONT:
			supply_fonts_and_gcs(priv, self, (Xv_font)A1);
			update_coordinates(priv);
			ADONE;

		case RICHTEXT_REPORT_LONG_LINES:
			priv->report_long_lines = (int)A1;
			ADONE;

		case SCROLLWIN_REPAINT:
			repaint_from_expose(priv, (Scrollwin_repaint_struct *)A1);
			ADONE;

		case SCROLLWIN_HANDLE_EVENT:
			es = (Scrollwin_event_struct *)A1;
			consumed = handle_events(priv, es);
			if (consumed) {
				es->consumed = TRUE;
				ADONE;
			}
			break;

		case XV_END_CREATE:
			supply_fonts_and_gcs(priv, self, xv_get(self, XV_FONT));
			break;

		default:
			xv_check_bad_attr(RICHTEXT, A0);
	}

	return XV_OK;
}

static Xv_opaque richtext_get(Richtext self, int *status,
								Attr_attribute attr, va_list vali)
{
	Richtext_private *priv = RICHPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case RICHTEXT_REPORT_LONG_LINES:
			return (Xv_opaque)priv->report_long_lines;

		case RICHTEXT_FILLED:
			return (Xv_opaque)priv->last_viewline;

		default:
			*status = xv_check_bad_attr(RICHTEXT, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int richtext_init(Xv_opaque owner, Richtext pubself,
								Attr_avlist avlist, int *unused)
{
	Xv_richtext *self = (Xv_richtext *)pubself;
	Richtext_private *priv;

	priv = (Richtext_private *)xv_alloc(Richtext_private);
	if (!priv) return XV_ERROR;

	priv->public_self = pubself;
	self->private_data = (Xv_opaque)priv;

	priv->margin_width = 8;
	return XV_OK;
}

static int richtext_destroy(Richtext self, Destroy_status status)
{
	Richtext_private *priv = RICHPRIV(self);

	if (status == DESTROY_CLEANUP) {
		int i;
		Display *dpy = (Display *)xv_get(self, XV_DISPLAY);

		free_list(priv->list);
		for (i = 0; i < __RT_MODE_COUNT; i++) {
			if (priv->gcs[i]) XFreeGC(dpy, priv->gcs[i]);
		}
		memset((char *)priv, 0, sizeof(*priv));
		free((char *)priv);
	}
	return XV_OK;
}

Xv_pkg xv_richtext_pkg = {
	"RICHTEXT",
	ATTR_PKG_RICHTEXT,
	sizeof(Xv_richtext),
	SCROLLWIN,
	richtext_init,
	richtext_set,
	richtext_get,
	richtext_destroy,
	NULL
};
