#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)om_render.c 20.176 93/06/28 DRA: $Id: om_render.c,v 4.11 2025/07/05 12:43:14 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>

#include <pixrect/pixrect.h>
#include <pixrect/pr_util.h>

#ifdef __STDC__ 
#ifndef CAT
#define CAT(a,b)        a ## b 
#endif 
#endif
#include <pixrect/memvar.h>

#include <xview/rect.h>
#include <xview/rectlist.h>
#include <xview/font.h>
#include <xview/defaults.h>
#include <xview/notify.h>
#include <xview/panel.h>
#include <xview/pixwin.h>
#include <xview/server.h>
#include <xview/window.h>
#include <xview/win_input.h>
#include <xview/win_struct.h>


#include <X11/Xlib.h>
#include <xview_private/draw_impl.h>
#include <xview_private/scrn_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/windowimpl.h>

#include <xview_private/om_impl.h>

#define DRA_CHANGED_TITLE_ADD TEXT_LEDGE_HEIGHT

typedef enum {
    CLEANUP_EXIT,
    CLEANUP_CLEANUP,
    CLEANUP_ABORT
}               Cleanup_mode;


/* ------------------------------------------------------------------------ */

typedef void (*scrn_win_ev_proc_t)(Xv_opaque, Event *);

/*
 * XView Private
 */
extern Graphics_info *xv_init_olgx(Xv_Window, int *, Xv_Font);
Xv_private Cms xv_set_control_cms(Xv_Window window_public, Xv_Drawable_info *info, int cms_status);
Xv_private void xv_help_save_image(Xv_Window pw, int client_width, int client_height, int mouse_x, int mouse_y);
Xv_private int xv_help_render(Xv_Window client_window, caddr_t client_data, Event *client_event);

/*
 * Package private
 */

Pkg_private int  menu_active_menu_key;	/* defined in om_public.c */


/* stolen from p_choice.c, see also om_public.c */
#define	TOGGLE_Y_GAP	6	/* # of y pixels between toggle rectangles */


/*
 * Private
 */
static Menu_status render_pullright(
    register Xv_menu_info *parent_menu,
    register Xv_menu_item_info *mi,
    Rect           *position_rect,
    int             stay_up	/* TRUE or FALSE */
);
static short compute_show_submenu(Xv_menu_info *m, Event *event,
    Rect *submenu_region_rect, int *submenu_stay_up);
static void constrainrect(struct rect *rconstrain, struct rect *rbound);
static void cleanup(register Xv_menu_info *m, Cleanup_mode cleanup_mode);
static void compute_menu_item_paint_rect(
    register Xv_menu_info *m,		/* ptr to menu information structure */
    register int    	   item_nbr,	/* current menu item number */
    register Rect  	  *rect,	/* paint rect (output parameter) */
    int            	  *item_top	/* menu item top (output parameter) */
);
static void compute_rects(
    Xv_menu_info	*menu,
    struct inputevent	*iep,
    int  item_height,
    Rect *mrect,
	Rect *srect	/* menu and shadow rect, in scrn coordinates */
);
static void destroy_gen_items(Xv_menu_info *menu);
static void feedback(Xv_menu_info *m,
    int             n,		/* index of current menu item */
    Menu_feedback   state);
static void menu_window_paint(Xv_menu_info *m, Xv_Window window);
static void paint_menu_item(Xv_menu_info *m,
    int             n,		/* current menu item number */
    Menu_feedback   feedback_state
);
static void process_event(register Xv_menu_info *m, Event *event);
static void set_mode(register Xv_menu_info *m, int check_cmc,
				int menu_button_up);
static void submenu_done(register Xv_menu_info *m);



/*
 * Private defs
 */
#define DEFAULT_FG_COLOR -1
#define	MENU_2D_SHADOW	6	/* Width of menu shadow in 2D */

#define MENU_3D_SHADOW MENU_2D_SHADOW	/* Width of menu shadow in 3D */

#define TEXT_LEDGE_HEIGHT 2
/* ACC_XVIEW */
#define WALKMENU_BORDER(m)	\
	((m->class == MENU_CHOICE || \
	  m->class == MENU_MIXED || \
	  m->class == MENU_TOGGLE) ? 6 : 4)
#  define WALKMENU_BORDER_X(m) walkmenu_border_x(m)
#  define WALKMENU_BORDER_Y(m) WALKMENU_BORDER(m) 
/* ACC_XVIEW */
#define SCREEN_MARGIN   10	/* Minimum number of pixels away from the
				 * edge of the screen a menu's left and top
				 * sides should be. (to enable backing out of
				 * pullright menus */
#define MENU_TITLE_MARGIN 3
#define MOUSE_BUMP 5		/* jiggle damping factor on MENU click */


static int walkmenu_border_x(Xv_menu_info *menu)
{
	int font_size = (int)xv_get(menu->default_image.font, FONT_SIZE);

	if (font_size      >= 19) return 13;
	else if (font_size >= 14) return  9;
	else if (font_size >= 12) return  8;
	else                      return  7;
}

static int is_set_default_event(Xv_menu_group_info *grp, Event *event)
{
	return (int)xv_get(grp->server, SERVER_EVENT_HAS_SET_DEFAULT_MODIFIERS, event);
}

/*
 * Compute the dimensions of the menu.
 * menu->ncols > 0 => MENU_NCOLS has been set by application
 * menu->nrows > 0 => MENU_NROWS has been set by application
 * therefore ncols_fixed and nrows_fixed are set
 */
static int compute_dimensions(Xv_menu_info *menu, int iwidth, int iheight,
								Rect  *rect)
{
    register int    ncols, nrows;
    int		    nitems;
    Xv_object       root_window;
    Rect           *fs_screenrect;
    int             max_height, max_width;
    int             old_ncols, old_nrows;

    root_window = xv_get(menu->group_info->client_window, XV_ROOT);
    fs_screenrect = (Rect *) xv_get(root_window, XV_RECT);

    /* BR# 1092662 */
    ncols = menu->ncols_fixed;
    nrows = menu->nrows_fixed;
    /* BR# 1092662 */
 
                                /* since the title isn't included */
                                /* in calc for rows and cols, */
                                /* ignore it */

    if (menu->item_list[0]->title)
        nitems = menu->nitems - 1;
    else
        nitems = menu->nitems;
 
 
    if (menu->ncols_fixed)      /* cols specified by app */
    {
        if (nitems < ncols)
            ncols = nitems;
        nrows = ((nitems - 1) / ncols) + 1;
    }
    else if (menu->nrows_fixed) /* rows specified by app */
    {
        if (nitems < nrows)
            nrows = nitems;
        ncols = ((nitems - 1) / nrows) + 1;
    }
    else                        /* neither cols or rows have been */
    {                           /* set by app, so fit with maximum */
                                /* no. of rows and minimum no. cols */
        ncols = 1;
        nrows = nitems;
    }

	if (menu->class == MENU_MIXED) {
		int i, h = 0;
		int first_toggle = -1;
		int have_choice = FALSE;

		for (i = 0; i < menu->nitems; i++) {
			Xv_menu_item_info *mi = menu->item_list[i];

			h += Button_Height(menu->ginfo);
			if (mi->class == MENU_CHOICE) {
				h += 6 + 1;
				have_choice = TRUE;
			}
			else if (mi->class == MENU_TOGGLE) {
				h += 6;
				if (first_toggle >= 0) h += TOGGLE_Y_GAP;
				else first_toggle = i;
			}
		}
		if (have_choice) h += TOGGLE_Y_GAP;

		/* strange: all mixed menus with a toggle item are to high -
		 * try to shorten...
		 * but then I found a mixed menu where the **first** item 
		 * was a toggle item - and this menu was not too high - sigh
		 */
		if (first_toggle > 0) h -= 16;

    	rect->r_height = h + 2 * WALKMENU_BORDER_Y(menu);
	}
	else {
		/* a homogenious menu: iheight is the same for each item */
    	rect->r_height = (nrows * iheight) + 2 * WALKMENU_BORDER_Y(menu);
	}

    rect->r_width = (ncols * iwidth) + 2 * WALKMENU_BORDER_X(menu);
    if (menu->item_list[0]->title)
        rect->r_height += iheight + DRA_CHANGED_TITLE_ADD;
 
    max_height = fs_screenrect->r_height - SCREEN_MARGIN;
    max_width = fs_screenrect->r_width - SCREEN_MARGIN;
 
    if (menu->ncols_fixed || menu->nrows_fixed) {
        while ((rect->r_height > max_height) ||
               (rect->r_width > max_width))
        {
            if (menu->ncols_fixed) {
                old_ncols=ncols;
                if (rect->r_width > max_width)
                    ncols--;
                else
                    ncols++;
                nrows = ((nitems - 1) / ncols) + 1;
 
                rect->r_height = (nrows * iheight) + 2 * WALKMENU_BORDER_Y(menu);
                rect->r_width = (ncols * iwidth) + 2 * WALKMENU_BORDER_X(menu);
                if (menu->item_list[0]->title)
        			rect->r_height += iheight + DRA_CHANGED_TITLE_ADD;
 
                /*
                 * if number of cols has decreased and menu width
                 * is less than max width, but it still to tall,
                 * then menu won't fit on screen.
                 */
                if ((ncols < old_ncols) &&
                    (rect->r_width < max_width) &&
                    (rect->r_height > max_height))
                {
                    xv_error(XV_NULL,
                        ERROR_STRING, XV_MSG("Menu too large for screen"),
                        ERROR_PKG, MENU,
                        NULL);
                    return FALSE;
                }
            }
            else {
				/* case 1: menu didn't fit cause */
				/* number of row exceeded max */
				/* number of rows allowed */
				/* */
				/* case 2: menu didn't fit cause */
				/* number of col exceeded width */
				/* of screen (ie too many cols */
                old_nrows=nrows;
                if (rect->r_height > max_height)
                    nrows--;
                else
                    nrows++;
                ncols = ((nitems - 1) / nrows) + 1;
 
                rect->r_height = (nrows * iheight) + 2 * WALKMENU_BORDER_Y(menu);
                rect->r_width = (ncols * iwidth) + 2 * WALKMENU_BORDER_X(menu);
                if (menu->item_list[0]->title)
        			rect->r_height += iheight + DRA_CHANGED_TITLE_ADD;
 
                /*
                 * if number of rows has decreased and menu height
                 * is less then max height, but it's still too wide,
                 * then menu won't fit on screen.
                 */
                if ((nrows < old_nrows) &&
                    (rect->r_height < max_height) &&
                    (rect->r_width > max_width))
                {
                    xv_error(XV_NULL,
                       ERROR_STRING, XV_MSG("Menu too large for screen"),
                       ERROR_PKG, MENU,
                       NULL);
                    return FALSE;
                }
            }
        }
    }
    else {
        while ((rect->r_height > max_height) || (rect->r_width > max_width)) {
            ncols++;
            nrows = ((nitems - 1) / ncols) + 1;
 
            rect->r_height = (nrows * iheight) + 2 * WALKMENU_BORDER_Y(menu);
            rect->r_width = (ncols * iwidth) + 2 * WALKMENU_BORDER_X(menu);
            if (menu->item_list[0]->title)
        		rect->r_height += iheight + DRA_CHANGED_TITLE_ADD;
 
            if (rect->r_width > max_width) {
                xv_error(XV_NULL,
                    ERROR_STRING, XV_MSG("Menu too large for screen"),
                    ERROR_PKG, MENU,
                    NULL);
                return FALSE;
            }
        }
    }
 
    if (menu->item_list[0]->title)
        nrows++;
 
    menu->ncols = ncols;
    menu->nrows = nrows;
 
    return TRUE;
}

/* ------------------------------------------------------------------------- */

static void menu_shadow_paint(Xv_Window shadowwin)
{
	Display *dpy;
	Xv_Drawable_info *info;
	Rect rect;
	GC  shadow_gc;
	GC *gc_list;
	Xv_object screen = xv_get(shadowwin, XV_SCREEN);
	XID xid;
	Pixmap pix;

	DRAWABLE_INFO_MACRO(shadowwin, info);
	dpy = xv_display(info);
	xid = xv_xid(info);

	/* Create the shadow's 75% gray pattern Graphics Context */
	gc_list = (GC *) xv_get(screen, SCREEN_OLGC_LIST, shadowwin);
	shadow_gc = gc_list[SCREEN_MENUSHADOW_GC];

	/* Fill in shadow window with shadow pattern */
	rect = *(Rect *) xv_get(shadowwin, WIN_RECT);
	pix = (Pixmap)xv_get(shadowwin, XV_KEY_DATA, MENU_SHADOW_PIXMAP);
	if (! pix) {
		XFillRectangle(dpy, xid, shadow_gc, 0, 0,
						(unsigned)rect.r_width + 1,
						(unsigned)rect.r_height + 1);

		pix = XCreatePixmap(dpy, xid, (unsigned)rect.r_width + 1,
						(unsigned)rect.r_height + 1, xv_depth(info));
		XCopyArea(dpy, xid, pix, gc_list[SCREEN_SET_GC], 0, 0,
						(unsigned)rect.r_width + 1, (unsigned)rect.r_height + 1,
						0, 0);
		xv_set(shadowwin, XV_KEY_DATA, MENU_SHADOW_PIXMAP, pix, NULL);
	}
	else {
		XCopyArea(dpy, pix, xid, gc_list[SCREEN_SET_GC], 0, 0,
						(unsigned)rect.r_width + 1, (unsigned)rect.r_height + 1,
						0, 0);
	}
}


static void menu_shadow_event_proc(Xv_Window shadowwin, Event *event)
{
	Xv_menu_info *m;

	if (event_id(event) == WIN_UNMAP_NOTIFY) {
		Pixmap pix = (Pixmap)xv_get(shadowwin, XV_KEY_DATA, MENU_SHADOW_PIXMAP);
		if (pix) {
			Display *dpy = (Display *)xv_get(shadowwin, XV_DISPLAY);

			xv_set(shadowwin, XV_KEY_DATA, MENU_SHADOW_PIXMAP, XV_NULL, NULL);
			XFreePixmap(dpy, pix);
		}
		return;
	}

	m = (Xv_menu_info *) xv_get(shadowwin, XV_KEY_DATA, MENU_SHADOW_MENU);
	if (!m || !m->group_info) return;

	if (m->group_info->client_window && event_id(event) == WIN_REPAINT)
		menu_shadow_paint(shadowwin);
}

/*
 * Menu_render modifies the inputevent parameter iep. It should contain the
 * last active inputevent read for the fd. Return values: MENU_STATUS_PIN:
 * pushpin was selected MENU_STATUS_ABORT:	no menu item was selected;
 * abort menu chain MENU_STATUS_DONE:	a menu item was selected
 * MENU_STATUS_PARENT:  no menu item was selected; use last event
 * MENU_STATUS_IGNORE:  no menu item was selected; ignore last event
 * MENU_STATUS_DEFAULT: select default item from menu
 */
Pkg_private void menu_render(Xv_menu_info *menu, Xv_menu_group_info *group,
    Xv_menu_item_info *parent)
{
	register Xv_menu_info *m;
	register Event *iep;
	Xv_Color_info *color_info;

	/*
	 * Extra registers not available on 80386
	 */
	register int i;

	short check_cmc;	/* boolean: checking for Click-Move-Click

						   * mode */
	int item_width, item_height;
	int heightsum;
	Menu gen_menu;
    Menu_gen_proc_t	gen_proc;
	Xv_Drawable_info *menu_window_info;
	int n = 0;	/* item number needing to be cleared */
	int new_window;	/* TRUE or FALSE */

	/* ACC_XVIEW */
	struct image *std_image, *std_qual_image, *std_key_image;
	int key_pos, mark_pos, label_pos;

	/* ACC_XVIEW */
	Xv_object root_window = xv_get(group->client_window, XV_ROOT);
	Xv_Drawable_info *root_window_info;
	Xv_object screen;
	Rect shadowrect;
	int status;
	Rect used_window_rect;	/* width = 0 => window not reused */

	/*
	 * Initial setup: Pull out group information.
	 */
	DRAWABLE_INFO_MACRO(root_window, root_window_info);
	screen = xv_screen(root_window_info);
	menu->group_info = group;
	menu->parent = parent;
	group->depth++;
	iep = &group->last_event;

	/*
	 * From here on any returns from this procedure should go thru exit:
	 */

	/*
	 * Dynamically create the menu if requested.
	 */
	if ((gen_proc = menu->gen_proc)) {
		gen_menu = gen_proc(MENU_PUBLIC(menu), MENU_DISPLAY);
		if (! gen_menu) {
			xv_error((Xv_opaque) menu,
					ERROR_STRING, XV_MSG("menu_render: menu's gen_proc failed to generate a menu"),
					ERROR_PKG, MENU,
					NULL);
			cleanup(menu, CLEANUP_ABORT);
			return;
		}
		m = MENU_PRIVATE(gen_menu);
		xv_set(group->server, XV_KEY_DATA, menu_active_menu_key, m, NULL);
		m->busy_proc = menu->busy_proc;
		m->done_proc = menu->done_proc;
		m->enable_rect = menu->enable_rect;
		m->popup = menu->popup;
		m->position_rect = menu->position_rect;
		m->pulldown = menu->pulldown;
		m->rendered = FALSE;
		m->stay_up = menu->stay_up;
		m->gen_proc = gen_proc;
		m->group_info = group;
		m->menu_mark = 0;
		xv_set(gen_menu,
				XV_KEY_DATA, PANEL_BUTTON,
						xv_get(MENU_PUBLIC(menu), XV_KEY_DATA, PANEL_BUTTON),
				NULL);
	}
	else {
		m = menu;
	}

	/* hier koennte eine gute Stelle sein, in einem MENU_MIXED Menue
	 * die 'zusammenhaengenden' CHOICE-Items zu verbinden: gleiche choice_group
	 */
	if (m->class == MENU_MIXED) {
		int ch_grp = 1;
		int last_choice_index = -99;

		for (i = 0; i < m->nitems; i++) {
			Xv_menu_item_info *mi = m->item_list[i];

			if (mi->class == MENU_CHOICE) {
				if (mi->choice_group > 0) {
					/* already grouped */
					ch_grp = mi->choice_group;
				}
				else {
					if (last_choice_index == i - 1) {
						/* same group */
					}
					else if (last_choice_index < 0) {
						/* first of a new group */
						ch_grp = i;
					}
					else {
						/* new group */
						cleanup(m, CLEANUP_ABORT);
						xv_error(XV_NULL,
							ERROR_STRING, XV_MSG("Several choice groups in one mixed menu not supported"),
							ERROR_PKG, MENU,
							NULL);
						return;
					}
					mi->choice_group = ch_grp;
				}
				last_choice_index = i;
			}
			else {
				/* a non-MENU_CHOICE item: prepare for the next choice_group */
				ch_grp = 1;
				last_choice_index = -99;
			}
		}

		/* und die beiden Rects belegen wir auch gleich (r_top und r_height) */
		/* das machen wir nicht, denn es ist zu frueh ! */
	}
	else {
		for (i = 0; i < m->nitems; i++) {
			m->item_list[i]->class = m->class;
		}
	}

	m->active = TRUE;
	m->parent = parent;
	m->pending_default_position = m->default_position;
	m->gen_items = FALSE;
	m->read_inputevent = FALSE;
	m->group_info->setting_default = is_set_default_event(m->group_info, iep);

	/* Get the menu window */
	rect_construct(&used_window_rect, 0, 0, 0, 0);
	if (!m->window) {
		m->window = screen_get_cached_window(screen,
						(Notify_func)menu_window_event_proc,
						m->group_info->three_d ? FALSE : TRUE,	/* borders */
							FALSE,	/* transparent */
							m->group_info->vinfo->visual, &new_window);
		if (!m->window) {
			xv_error(XV_NULL,
					ERROR_STRING, XV_MSG("menu_create: unable to allocate menu window"),
					ERROR_PKG, MENU,
					NULL);
			cleanup(m, CLEANUP_ABORT);
			return;
		}
		if (!new_window)
			used_window_rect = *(Rect *) xv_get(m->window, XV_RECT);
		xv_set(m->window, XV_KEY_DATA, MENU_WINDOW_MENU, m, NULL);
		/* fix to make xv_window_loop work for menus */
		if (WIN_IS_IN_LOOP)
			notify_interpose_event_func(m->window,
						(Notify_event_interposer_func) menu_window_event_proc,
						NOTIFY_IMMEDIATE);
	}

	/* Insure default image fonts valid */
	/* ACC_XVIEW */
	if (m->default_image.font == XV_NULL)
		m->default_image.font =
			m->default_qual_image.font =
			m->default_key_image.font = xv_get(m->window, XV_FONT);
	/* ACC_XVIEW */
	if (m->default_image.bold_font == 0) {
		char *family = (char *)xv_get(m->default_image.font, FONT_FAMILY);
		int size = (int)xv_get(m->default_image.font, FONT_SIZE);

		if (family && *family)
			m->default_image.bold_font = xv_find(group->server, FONT,
								FONT_FAMILY, family,
								FONT_STYLE, FONT_STYLE_BOLD,
								FONT_SIZE, (size) ? size : FONT_SIZE_DEFAULT,
								NULL);
		if (!m->default_image.bold_font)
			m->default_image.bold_font = m->default_image.font;
	}
	/* ACC_XVIEW */
	m->default_qual_image.bold_font = m->default_key_image.bold_font =
						m->default_image.bold_font;
	/* ACC_XVIEW */

	m->glyph_font = xv_get(m->window, WIN_GLYPH_FONT);

	color_info = (Xv_Color_info *) xv_get(group->client_window, WIN_COLOR_INFO);
	DRAWABLE_INFO_MACRO(m->window, menu_window_info);
	if (m->group_info->vinfo->visual ==
						(Visual *) xv_get(group->client_window, XV_VISUAL))
	{
		xv_set(m->window, WIN_COLOR_INFO, color_info, NULL);
	}

	if (!m->ginfo)
		m->ginfo = xv_init_olgx(m->window, &m->group_info->three_d,
							m->default_image.font);

	if (SCREEN_UIS_2D_BW!=xv_get(screen, SCREEN_UI_STYLE)) {
		int cms_status;

		/* Use OpenWindows.WindowColor as background color.  */
		cms_status = (int)xv_get(xv_cms(menu_window_info), CMS_STATUS_BITS);
		if (!CMS_STATUS(cms_status, CMS_STATUS_CONTROL)) {
			(void)xv_set_control_cms(m->window, menu_window_info, cms_status);
			if (!m->ginfo)
				m->ginfo = xv_init_olgx(m->window, &m->group_info->three_d,
									m->default_image.font);
		}
		xv_set(m->window, WIN_BACKGROUND_COLOR, 0, NULL);
	}

	/* Get the shadow window */
	if (!m->shadow_window) {
		m->shadow_window = screen_get_cached_window(screen,
							(Notify_func)menu_shadow_event_proc, FALSE,
							TRUE,	/* transparent */
							group->vinfo->visual, &new_window);
		if (!m->shadow_window) {
			xv_error(XV_NULL,
								ERROR_STRING,
								XV_MSG
								("menu_create: unable to allocate shadow window"),
								ERROR_PKG, MENU, NULL);
			cleanup(m, CLEANUP_ABORT);
			return;
		}
		xv_set(m->shadow_window, XV_KEY_DATA, MENU_SHADOW_MENU, m, NULL);
	}

	/*
	 * If there are no items or just a pushpin, return to the
	 * parent menu, if any.
	 */
	if (m->nitems == 0 || (m->nitems == 1 && m->pin)) {
		m->status = MENU_STATUS_PARENT;
		cleanup(m, CLEANUP_EXIT);
		return;
	}

	/* Remember initial selections */
	switch (m->class) {
		case MENU_CHOICE:
			for (i = 0; i < m->nitems; i++)
				if (m->item_list[i]->selected) {
					n = m->selected_position = i + 1;
					break;
				}
			if (n == 0) {
				/*
				 * No choice selected by client: force default choice to be
				 * selected
				 */
				if (m->item_list[m->default_position - 1]->title)
					/* Can't select title */
					++m->default_position;
				m->item_list[m->default_position - 1]->selected = TRUE;
				n = m->selected_position = m->default_position;
			}
			break;

		case MENU_TOGGLE:
			/* Set each menu item's toggle_on = selected */
			for (i = 0; i < m->nitems; i++)
				m->item_list[i]->toggle_on = m->item_list[i]->selected;
			break;
		case MENU_COMMAND:
			/* das kam anscheinend bei JEDEM Menuaufruf */
			break;
		case MENU_MIXED:
			{
				Xv_menu_item_info *mi;
				int first_choice_index = -1;

				n = 0;
				for (i = 0; i < m->nitems; i++) {
					mi = m->item_list[i];

					if (mi->class == MENU_CHOICE) {
						if (first_choice_index < 0) {
							first_choice_index = i;
						}
						if (mi->selected) {
							n = m->selected_position = i + 1;
						}
					}
					else if (mi->class == MENU_TOGGLE) {
						mi->toggle_on = mi->selected;
					}
				}
				if ((first_choice_index >= 0) && (n == 0)) {
					/*
					 * No choice selected by client: force default choice to be
					 * selected
					 */
					mi = m->item_list[m->default_position - 1];
					if (mi->title) {
						/* Can't select title */
						++m->default_position;
						mi = m->item_list[m->default_position - 1];
					}
					if (mi->class == MENU_CHOICE) {
						mi->selected = TRUE;
						/* that is one of the reasons why several 
						 * choice_groups are not supported:
						 */
						m->selected_position = m->default_position;
					}
					else {
						/* select the first */
						mi = m->item_list[first_choice_index];
						mi->selected = TRUE;
						m->selected_position = first_choice_index + 1;
					}
				}
			}
			break;
	}

	/*
	 * Find the current menu item
	 */
	if (m->class == MENU_CHOICE)
		/* Pulldown or pullright choice menu */
		m->curitem = m->group_info->setting_default ? m->default_position :
							m->selected_position;
	else if (!event_is_button(&m->group_info->last_event) &&
						event_action(&m->group_info->last_event) != LOC_DRAG) {
		for (m->curitem = m->default_position;
				m->item_list[m->curitem - 1]->inactive
				|| (!m->item_list[m->curitem - 1]->title
					&& m->item_list[m->curitem - 1]->no_feedback)
				|| (m->item_list[m->curitem - 1]->title && !m->pin);
			)
		{
			m->curitem++;
			if (m->curitem > m->nitems)
				m->curitem = 1;
			if (m->curitem == m->default_position)
				break;	/* no other active item found */
		}
	}
	else
		m->curitem = 0;

	/*
	 * Compute the size of an item.
	 */


	/* ACC_XVIEW */
	std_image = &m->default_image;
	std_qual_image = &m->default_qual_image;	/* stores default qualifier image */
	std_key_image = &m->default_key_image;	/* stores default key image */

	std_qual_image->image_type = QUALIFIER;
	std_image->image_type = LABEL;
	std_key_image->image_type = KEY;

	m->gen_items = menu_compute_max_item_size(m, std_image, &status, TRUE,
								&heightsum);
	menu_compute_max_item_size(m, std_qual_image, &status, FALSE,
								NULL);
	menu_compute_max_item_size(m, std_key_image, &status, FALSE,
								NULL);
	/* ACC_XVIEW */

	if (status != XV_OK) {
		cleanup(m, CLEANUP_ABORT);
		return;
	}

	{
		int j, top;

		/* und die beiden Rects belegen wir auch gleich (r_top und r_height) */
		/* Ref (jg4ferjgfv)  */
		top = WALKMENU_BORDER_Y(m);
		for (j = 0; j < m->nitems; j++) {
			Xv_menu_item_info *mi = m->item_list[j];

			mi->item_rect.r_top = mi->paint_rect.r_top = top;
			switch (mi->class) {
				case MENU_COMMAND:
					mi->paint_rect.r_height = mi->image.button_size.y;
					mi->item_rect.r_height = mi->image.button_size.y;

					/* beautification: if a command item is followed by
					 * a choice item, we want to see a little gap after
					 * the command item.
					 */
					if (j < m->nitems - 1) { /* is not the last item */
						if (m->item_list[j+1]->class == MENU_CHOICE) {
							mi->item_rect.r_height += TOGGLE_Y_GAP;
						}
					}
					break;
				case MENU_TOGGLE:
					mi->item_rect.r_height = mi->image.height + TOGGLE_Y_GAP;
					mi->paint_rect.r_height = mi->image.height;
					mi->paint_rect.r_top += (TOGGLE_Y_GAP / 2);
					break;
				case MENU_CHOICE:
				default:
					mi->item_rect.r_height = mi->image.height;
					if (!m->group_info->three_d) {
						mi->item_rect.r_height++;
					}
					mi->paint_rect.r_height = mi->item_rect.r_height;
					break;
			}
    		top += mi->item_rect.r_height;
		}
	}

	/* ACC_XVIEW */
	label_pos = MainLabel_Pos(m->ginfo, std_image->left_edge);
	if (std_qual_image->width == 0) {
		if (m->menu_mark)
			mark_pos = label_pos + std_image->width +
								ButtonSpace_Width(m->ginfo);
		else
			mark_pos = label_pos + std_image->width;
	}
	else {
		mark_pos = ButtonMark_Pos(m->ginfo, label_pos,
							std_image->width, std_qual_image->width);
	}
	if (std_key_image->width == 0)
		key_pos = mark_pos + m->menu_mark * MenuMark_Width(m->ginfo);
	else
		key_pos = KeyLabel_Pos(m->ginfo, mark_pos);
	std_key_image->left_edge = key_pos;
	item_width = Button_Width(m->ginfo, std_image->left_edge, key_pos,
						std_key_image->width);
	/* ACC_XVIEW */

	item_height = std_image->height;

	/*
	 * Compute the dimensions of the menu rectangle
	 */
	if (!compute_dimensions(m, item_width, item_height, &m->fs_menurect)) {
		cleanup(m, CLEANUP_ABORT);
		return;
	}

	/*
	 * Compute the rects:
	 *   m->fs_menurect - represents the area of the menu including its borders
	 *   shadowrect - shadow rectangle
	 * Both rects are in screen coordinates.
	 */
	compute_rects(m, iep, item_height, &m->fs_menurect, &shadowrect);

	/* Compute pushpin coordinates */
	if (m->pin) {
		m->pushpin_rect.r_left = m->menurect.r_left + m->pushpin_left;
		m->pushpin_rect.r_top = m->menurect.r_top + m->pushpin_top;
		m->pushpin_rect.r_width = PushPinOut_Width(m->ginfo);
		m->pushpin_rect.r_height = PushPinOut_Height(m->ginfo);
	}

	/*
	 * Define the menu and shadow window dimensions.  Note: shadow rect width &
	 * height = menu rect width & height
	 */

	xv_set(m->window, XV_RECT, &m->fs_menurect, NULL);
	xv_set(m->shadow_window, XV_RECT, &shadowrect, NULL);

	XFlush(XV_DISPLAY_FROM_WINDOW(m->window));

	/* fix to make xv_window_loop work for menus */
	if (WIN_IS_IN_LOOP) {
		window_set_tree_flag(m->window, XV_NULL, FALSE, TRUE);
		window_set_tree_flag(m->shadow_window, XV_NULL, FALSE, TRUE);
	}
	else {
		window_set_tree_flag(m->window, XV_NULL, FALSE, FALSE);
		window_set_tree_flag(m->shadow_window, XV_NULL, FALSE, FALSE);
	}

	/*
	 * Display the menu (i.e., map the menu and shadow windows). Note:
	 * Windows must be mapped after the width and height have been set.
	 * Changing the size of a save-under window will invalidate the
	 * save-under, resulting in damage-repaint events.
	 */

	/* Note: Shadow window must be mapped before menu window.  */
	xv_set(m->shadow_window, XV_SHOW, TRUE, NULL);

	xv_set(m->window, XV_SHOW, TRUE, NULL);

	SERVERTRACE((44, "SERVER_JOURNALLING?\n"));
	if (xv_get(xv_server(root_window_info), SERVER_JOURNALLING)) {
		SERVERTRACE((44, " YES \n"));
		xv_set(xv_server(root_window_info), SERVER_JOURNAL_SYNC_EVENT, 1, NULL);
	}

	if (m->group_info->depth <= 1) {

		if (m->popup && defaults_get_boolean("openWindows.menuClickExecutesDefault",
							"OpenWindows.MenuClickExecutesDefault", FALSE))
		{
			/* stayup for popup menus is disabled */
			m->state = MENU_STATE_TRACK_MOUSE;
			m->status = MENU_STATUS_BUSY;
			return;
		}
		else {
			/*
			 * We're bringing up the base menu: determine if this is a stay-up
			 * menu.
			 */
			if ((event_action(&m->group_info->first_event) == ACTION_MENU &&
				event_is_up(&m->group_info->first_event))
				|| !event_is_button(&m->group_info->first_event))
			{
				m->stay_up = TRUE;
				if (!event_is_button(&m->group_info->first_event)) {
					/* Set state to GET_MODE in order to interpret
					 * the MENU-up or DEFAULT_ACTION-up as the second half
					 * of this MENU-down (which could be a translated
					 * DEFAULT_ACTION-down, coming from panel_default_event).
					 */
					m->state = MENU_STATE_GET_MODE;
					return;
				}
			}
			else {
				m->state = MENU_STATE_GET_MODE;
				m->status = MENU_STATUS_BUSY;
				return;
			}
		}
	}
	check_cmc = m->group_info->depth <= 1 || parent->parent->stay_up;
	set_mode(m, check_cmc, TRUE);
	switch (event_action(iep)) {
		case ACTION_MENU:
			if (event_is_button(&m->group_info->first_event))
				process_event(m, iep);
			break;
		case LOC_DRAG:
			process_event(m, iep);
			break;
	}
}

static int absolute_value(int x)
{
    return (x < 0 ? -x : x);
}


static void get_mode(Xv_menu_info *m, Event *event)
{
    short           mouse_dragged;
    /*
     * boolean: mouse was dragged at least 5 pixels in any direction.
     */
    short           menu_button_up;	/* boolean */

    menu_button_up = event_action(event) == ACTION_MENU && event_is_up(event);
    if (event_is_button(event) || event_action(event) == LOC_DRAG) {
		/*
		 * Determine stay-up mode based on mouse drag and MENU click.  Mouse is
		 * said to have been dragged if it was moved more than the MENU-click
		 * jiggle damping factor (MOUSE_BUMP).
		 */
		mouse_dragged =
		    (event_action(event) == LOC_DRAG ||
				event_action(event) == ACTION_MENU) &&
		    ((absolute_value(event_x(event) -
			     event_x(&m->group_info->first_event)) >= MOUSE_BUMP) ||
		     (absolute_value(event_y(event) -
			      event_y(&m->group_info->first_event)) >= MOUSE_BUMP));
		if (!mouse_dragged && !menu_button_up)
		    /* Menu state is still MENU_STATE_GET_MODE */
		    return;		/* go back to notifer; wait for next mouse event */
		/* Mouse was dragged and/or MENU button is up */
		m->stay_up = !mouse_dragged && menu_button_up;
    }
	else {
		/*
		 * Keyboard commands are being used: Declare CMC if MENU-up.
		 */
		m->stay_up = menu_button_up;
    }
    if (m->group_info->depth <= 1 && m->stay_up && m->busy_proc)
		(m->busy_proc) (MENU_PUBLIC(m));

    set_mode(m, TRUE, menu_button_up);
}


static void set_mode(register Xv_menu_info *m,
    int    check_cmc,	/* boolean: checking for Click-Move-Click mode */
    int    menu_button_up	/* boolean */
)
{

    if (check_cmc && !m->stay_up) {
	/*
	 * Press-drag-release: process a MENU-up
	 */
	m->read_inputevent = menu_button_up;
    }
    m->state = MENU_STATE_TRACK_MOUSE;
}


static int find_item_nbr(register Xv_menu_info *m, Event *event, int *row,
    int *column)
{
	int newitem;
	int title_off = 0;

	*column = event->ie_locx - m->menurect.r_left - WALKMENU_BORDER_X(m);
	if (*column < 0)
		*column = m->nitems;	/* Outside menu proper */
	else {
		/* ACC_XVIEW */
		*column /= Button_Width(m->ginfo, m->default_image.left_edge,
				m->default_key_image.left_edge, m->default_key_image.width);
		/* ACC_XVIEW */
		if (*column >= m->ncols)
			*column = m->ncols - 1;
	}

	if (m->item_list[0]->title) title_off = DRA_CHANGED_TITLE_ADD;

	if (m->class == MENU_MIXED) {
		int i;
		/* event wurde manipuliert: ie_locy = 0 kommt, wenn ich 
		 * schon (etwa) in der Mitte des ersten items (also unterhalb
		 * des Title). Also muss ich das geradebiegen:
		 */
		int real_y = event->ie_locy - m->menurect.r_top;

		for (i = 0; i < m->nitems; i++) {
			register Xv_menu_item_info *mi = m->item_list[i];

			if (real_y >= mi->item_rect.r_top &&
				real_y < mi->item_rect.r_top + mi->item_rect.r_height) 
			{
				*row = i;
				break;
			}
		}
	}
	else {
		*row = (event->ie_locy - title_off - m->menurect.r_top -
				WALKMENU_BORDER_Y(m))
				/ m->default_image.height;
	}
	if (*row < 0)
		*row = 0;
	else if (*row >= m->nrows)
		*row = m->nrows - 1;

	if (m->column_major) {
		newitem = *column * m->nrows + *row + 1;
		if (*column > 0 && m->item_list[0]->title)
			/* Account for the title at the top of each column after column 0.
			 * Column 0 is not a problem because in Column 0, the title is
			 * an actual menu item.
			 */
			newitem -= *column;
	}
	else
		newitem = *row * m->ncols + *column + 1;

	return newitem;
}


static void process_event(register Xv_menu_info *m, Event *event)
{
	register Xv_menu_item_info *mi;
	int column, row;	/* Note: first row & column is 0. */
	int default_position = m->default_position;
	Xv_Drawable_info *info;
	int keyboard_event = FALSE;
	int left_bdry;	/* left boundary of menu button stack */

#ifdef OW_I18N
	wchar_t match_char;
#else
	unsigned char match_char;
#endif /* OW_I18N */

	int mi_top;	/* menu item top */
	int newitem = 0;
	int newitem_tracks_pointer;

	/*
	 * TRUE: newitem is derived from pointer position; FALSE: newitem is
	 * derived from default position (pointer is off the menu or over the
	 * menu title)
	 */
	Menu pullright_menu;
	int state;
	int submenu_stay_up = FALSE;
	Menu_feedback feedback_state;
	Rect itemrect;
	Rect menu_mark_rect;
	Rect mi_paint_rect;	/* rect in menu item containing oval or box */

	/*
	 * Track the mouse.
	 */
	newitem_tracks_pointer = TRUE;
	m->group_info->previous_event = m->group_info->last_event;
	m->group_info->last_event = *event;
	m->group_info->setting_default = is_set_default_event(m->group_info, event);
	switch (event_action(event)) {
		case ACTION_CANCEL:
			cleanup(m, CLEANUP_ABORT);
			return;
		case ACTION_DEFAULT_ACTION:
		case ACTION_SELECT:
			if (!event_is_button(event)) {
				newitem = m->curitem;
				keyboard_event = TRUE;
			}
			break;
		case ACTION_UP:
			/* Go to previous active menu item, if any.  Cycle through top
			 * if necessary.  If no other active item, then ignore key.
			 */
			newitem = m->curitem;	/* could be 0 */
			do {
				newitem--;
				if (newitem == m->curitem)	/* includes m->curitem == 0 */
					return;	/* no other active item found */
				if (newitem <= 0)
					newitem = m->nitems;
			} while (m->item_list[newitem - 1]->inactive ||
					(!m->item_list[newitem - 1]->title &&
							m->item_list[newitem - 1]->no_feedback) ||
					(m->item_list[newitem - 1]->title && !m->pin));
			keyboard_event = TRUE;
			break;
		case ACTION_DOWN:
			/* Go to next active menu item, if any.  Cycle through bottom
			 * if necessary.  If no other active item, then ignore key.
			 */
			newitem = m->curitem;	/* could be 0 */
			do {
				newitem++;
				if (newitem > m->nitems) {
					if (m->curitem == 0)
						return;	/* no other active item found */
					newitem = 1;
				}
				if (newitem == m->curitem)
					return;	/* no other active item found */
			} while (m->item_list[newitem - 1]->inactive ||
					(!m->item_list[newitem - 1]->title &&
							m->item_list[newitem - 1]->no_feedback) ||
					(m->item_list[newitem - 1]->title && !m->pin));
			keyboard_event = TRUE;
			break;
		case ACTION_LEFT:
			/* Return to parent, if any */
			if (m->group_info->depth > 1) {
				m->status = MENU_STATUS_PARENT;
				cleanup(m, CLEANUP_CLEANUP);
				return;
			}
			else if (!m->popup && !m->pulldown)
				/* Pullright base menu: abort menu */
				cleanup(m, CLEANUP_ABORT);
			return;	/* ignore event */
		case ACTION_RIGHT:
			/* Invoke submenu, if any */
			newitem = m->curitem;
			if (!newitem || !m->item_list[newitem - 1]->pullright)
				return;	/* ignore event */
			keyboard_event = TRUE;
			submenu_stay_up = TRUE;
			break;
		default:

#ifdef OW_I18N
			if (event_is_string(event) || event_is_iso(event)) {
				/* Go to next active menu item that matches the first 
				 * event->ie_string character, if any.  Cycle through 
				 * the bottom if necessary.  If no other active
				 * item matches the character, then ignore the event.
				 */
				wchar_t target_char;

				newitem = m->curitem;	/* could be 0 */
				if (event_is_string(event))
					target_char = *((wchar_t *)event_string(event));
				else
					target_char = event_action(event);
				do {
					newitem++;
					if (newitem > m->nitems) {
						if (m->curitem == 0)
							return;	/* no other active item found */
						newitem = 1;
					}
					if (newitem == m->curitem)
						return;	/* no other active item found */
					mi = m->item_list[newitem - 1];
					match_char = 0;
					if (mi->title && m->pin)
						match_char = 'p';
					else if (_xv_is_string_attr_exist_nodup(&mi->image.string)) {
						_xv_use_pswcs_value_nodup(&mi->image.string);
						match_char = mi->image.string.pswcs.value[0];
						if (match_char >= 'A' && match_char <= 'Z' ||
								match_char >= 0xC0 && match_char <= 0xDE)
							match_char += 0x20;
					}
				} while (mi->inactive || (!mi->title && mi->no_feedback) ||
						(mi->title && !m->pin) || match_char != target_char);
				keyboard_event = TRUE;
			}
#else
			if (event_is_iso(event)) {
				/* Go to next active menu item that matches the iso character, if
				 * any.  Cycle through the bottom if necessary.  If no other active
				 * item matches the iso character, then ignore the iso event.
				 */
				newitem = m->curitem;	/* could be 0 */
				do {
					newitem++;
					if (newitem > m->nitems) {
						if (m->curitem == 0)
							return;	/* no other active item found */
						newitem = 1;
					}
					if (newitem == m->curitem)
						return;	/* no other active item found */
					mi = m->item_list[newitem - 1];
					match_char = 0;
					if (mi->title && m->pin)
						match_char = 'p';
					else if (mi->image.string) {
						match_char = (unsigned char)mi->image.string[0];
						if ((match_char >= 'A' && match_char <= 'Z') ||
								(match_char >= 0xC0 && match_char <= 0xDE))
							match_char += 0x20;
					}
				} while (mi->inactive || (!mi->title && mi->no_feedback) ||
						(mi->title && !m->pin) ||
						match_char != event_action(event));
				keyboard_event = TRUE;
			}
#endif /* OW_I18N */

			break;
	}
	if (rect_includespoint(&m->menurect, event->ie_locx, event->ie_locy) ||
			(keyboard_event && newitem))
	{
		/*
		 * Pointer is in the current menu
		 */
		if (!keyboard_event) {
			newitem = find_item_nbr(m, event, &row, &column);
		}
		else {
			column = (newitem - 1) / m->nrows;
			row = (newitem - 1) % m->nrows;
		}
		if (newitem == 1 && m->pin && !m->item_list[0]->inactive) {
			/*
			 * Determine if pointer is over pushpin
			 */
			if (keyboard_event ||
					rect_includespoint(&m->pushpin_rect, event->ie_locx,
							event->ie_locy)) {
				/*
				 * Pointer is over pushpin: Change image from pin-out to
				 * pin-in or default-pin-out
				 */
				if (m->rendered) {
					if (m->group_info->setting_default)
						state = OLGX_ERASE | OLGX_PUSHPIN_OUT | OLGX_DEFAULT;
					else
						state = OLGX_ERASE | OLGX_PUSHPIN_IN;
					DRAWABLE_INFO_MACRO(m->window, info);
					olgx_draw_pushpin(m->ginfo, xv_xid(info),
							m->pushpin_left, m->pushpin_top, state);
				}
			}
			else {
				newitem = 0;	/* pointer is not over any item or pushpin */
				newitem_tracks_pointer = FALSE;
			}
		}
		if (newitem) {
			if (newitem > m->nitems) {
				newitem = 0;
				newitem_tracks_pointer = FALSE;
			}
			else {
				/* Get menu window rect containing menu item oval/box */
				compute_menu_item_paint_rect(m, newitem, &mi_paint_rect,
						&mi_top);
				/*
				 * Translate to client window space and account for oval/box
				 * whitespace-border width.  Use full menuitem rect height to
				 * avoid selecting default when dragging up or down items.
				 */
				mi_paint_rect.r_left += m->menurect.r_left + 1;
				mi_paint_rect.r_top = m->menurect.r_top + mi_top;
				mi_paint_rect.r_height = m->default_image.height;
				/* If we're not over the oval/box, then there's no new item */
				if (!keyboard_event &&
						!rect_includespoint(&mi_paint_rect,
								event->ie_locx, event->ie_locy)) {
					newitem = 0;
					newitem_tracks_pointer = FALSE;
				}
			}
		}
	}
	else if (!keyboard_event) {	/* Pointer is not in the current menu */
		if (!m->stay_up
				&& m->group_info->depth > 1
				&& event->ie_locx <= m->menurect.r_left) {
			/* Pointer is to the left of the submenu: return to parent */
			m->status = MENU_STATUS_PARENT;
			cleanup(m, CLEANUP_CLEANUP);
			return;
		}
		newitem = 0;
		newitem_tracks_pointer = FALSE;
	}

	if (m->pin	/* there's a pushpin */
			&& !m->item_list[0]->inactive	/* it's not inactive */
			&& ((!keyboard_event &&
							!rect_includespoint(&m->pushpin_rect,
									event->ie_locx, event->ie_locy))
					|| (keyboard_event && newitem != 1))) {
		/*
		 * Pointer is not over pushpin
		 */
		if (newitem != m->curitem && m->rendered) {
			/* Change pushpin glyph */
			state = OLGX_ERASE | OLGX_PUSHPIN_OUT;
			if (!newitem && m->default_position == 1)
				state |= OLGX_DEFAULT;
			DRAWABLE_INFO_MACRO(m->window, info);
			olgx_draw_pushpin(m->ginfo, xv_xid(info),
					m->pushpin_left, m->pushpin_top, state);
		}
	}
	/*
	 * Provide feedback for new item.
	 */
	/* clear old item */
	if (newitem != m->curitem) {
		m->drag_right_distance = 0;
		switch (m->class) {
			case MENU_COMMAND:
				if (!m->curitem)
					m->curitem = m->default_position;
				feedback(m, m->curitem, MENU_REMOVE_FEEDBACK);
				break;

			case MENU_CHOICE:
				if (newitem == 0) {
					/*
					 * Pointer has been moved off of menu. Note that this is only
					 * true when m->group_info->setting_default == FALSE.
					 */
					if (m->selected_position > 0)
						newitem = m->selected_position;
					else if (default_position > 0)
						newitem = default_position;
					else
						newitem = 1;
					newitem_tracks_pointer = FALSE;
				}
				if (newitem != m->curitem) {
					feedback_state = MENU_REMOVE_FEEDBACK;
					if (!m->group_info->setting_default &&
							m->curitem == default_position)
						feedback_state = MENU_DEFAULT_FEEDBACK;
					feedback(m, m->curitem, feedback_state);
				}
				break;

			case MENU_TOGGLE:
				if (!m->curitem && m->group_info->setting_default)
					m->curitem = m->default_position;
				if (m->curitem) {
					if (!m->group_info->setting_default)
						m->item_list[m->curitem - 1]->toggle_on =
								(!m->item_list[m->curitem - 1]->toggle_on);
					if (m->group_info->setting_default)
						feedback_state = MENU_REMOVE_FEEDBACK;
					else if (m->curitem == default_position) {
						if (m->item_list[m->curitem - 1]->toggle_on)
							feedback_state = MENU_SELECTED_DEFAULT_FEEDBACK;
						else
							feedback_state = MENU_DEFAULT_FEEDBACK;
					}
					else if (m->item_list[m->curitem - 1]->toggle_on)
						feedback_state = MENU_PROVIDE_FEEDBACK;
					else
						feedback_state = MENU_REMOVE_FEEDBACK;
					feedback(m, m->curitem, feedback_state);
				}
				break;
			case MENU_MIXED:
				/* first handle m->curitem in analogy to the old menus */
				if (!m->curitem)
					m->curitem = m->default_position; /* fragwuerdig */
				mi = m->item_list[m->curitem - 1];

				switch (mi->class) {
					case MENU_COMMAND:
						feedback(m, m->curitem, MENU_REMOVE_FEEDBACK);
						break;

					case MENU_CHOICE:
						if (newitem == 0) {
							/*
							 * Pointer has been moved off of menu. Note that this is only
							 * true when m->group_info->setting_default ==FALSE.
							 */
							if (m->selected_position > 0)
								newitem = m->selected_position;
							else if (default_position > 0)
								newitem = default_position;
							else
								newitem = 1;
							newitem_tracks_pointer = FALSE;
						}
						if (newitem != m->curitem) {
							feedback_state = MENU_REMOVE_FEEDBACK;
							if (!m->group_info->setting_default &&
									m->curitem == default_position)
								feedback_state = MENU_DEFAULT_FEEDBACK;
							feedback(m, m->curitem, feedback_state);
						}
						break;

					case MENU_TOGGLE:
						if (m->curitem) {
							if (!m->group_info->setting_default)
								mi->toggle_on = ! mi->toggle_on;
							if (m->group_info->setting_default)
								feedback_state = MENU_REMOVE_FEEDBACK;
							else if (m->curitem == default_position) {
								if (mi->toggle_on)
									feedback_state = MENU_SELECTED_DEFAULT_FEEDBACK;
								else
									feedback_state = MENU_DEFAULT_FEEDBACK;
							}
							else if (mi->toggle_on)
								feedback_state = MENU_PROVIDE_FEEDBACK;
							else
								feedback_state = MENU_REMOVE_FEEDBACK;
							feedback(m, m->curitem, feedback_state);
						}
						break;
					case MENU_MIXED:
						/* blast */
						MIXED();
				}

				break;
		}

		/* Update new item, if any */
		if (!newitem &&
			(m->class != MENU_TOGGLE || m->group_info->setting_default))
		{
			/* INCOMPLETE MIXED() ??? */
			newitem = m->default_position;
		}
		if (newitem) {
			feedback_state = m->group_info->setting_default ?
					MENU_DEFAULT_FEEDBACK :
					(default_position != newitem) ?
						MENU_PROVIDE_FEEDBACK :
						(newitem_tracks_pointer || m->class == MENU_CHOICE) ?
						MENU_SELECTED_DEFAULT_FEEDBACK : MENU_DEFAULT_FEEDBACK;
			/* INCOMPLETE MIXED() ??? */

			switch (m->class) {
				case MENU_COMMAND:
					feedback(m, newitem, feedback_state);
					break;

				case MENU_CHOICE:
					if (newitem != m->curitem) {
						feedback(m, newitem, feedback_state);
						m->curitem = newitem;
					}
					break;

				case MENU_TOGGLE:
					if (m->group_info->setting_default == 0) {
						if ((m->item_list[newitem - 1]->toggle_on =
										(!m->item_list[newitem -
														1]->toggle_on))) {
							if (default_position == newitem)
								feedback_state = MENU_SELECTED_DEFAULT_FEEDBACK;
							else
								feedback_state = MENU_PROVIDE_FEEDBACK;
						}
						else if (default_position == newitem)
							feedback_state = MENU_DEFAULT_FEEDBACK;
						else
							feedback_state = MENU_REMOVE_FEEDBACK;
					}
					else
						feedback_state = MENU_DEFAULT_FEEDBACK;
					feedback(m, newitem, feedback_state);
					break;

				case MENU_MIXED:
					mi = m->item_list[newitem - 1];

					switch (mi->class) {
						case MENU_COMMAND:
							feedback(m, newitem, feedback_state);
							break;

						case MENU_CHOICE:
							if (newitem != m->curitem) {
								feedback(m, newitem, feedback_state);
								m->curitem = newitem;
							}
							break;

						case MENU_TOGGLE:
							if (m->group_info->setting_default == 0) {
								if ((mi->toggle_on = !mi->toggle_on)) {
									if (default_position == newitem)
										feedback_state = MENU_SELECTED_DEFAULT_FEEDBACK;
									else
										feedback_state = MENU_PROVIDE_FEEDBACK;
								}
								else if (default_position == newitem)
									feedback_state = MENU_DEFAULT_FEEDBACK;
								else
									feedback_state = MENU_REMOVE_FEEDBACK;
							}
							else
								feedback_state = MENU_DEFAULT_FEEDBACK;
							feedback(m, newitem, feedback_state);
							break;
						case MENU_MIXED:
							MIXED();
					}
					break;
			}
			if (m->group_info->setting_default)
				m->pending_default_position = default_position = newitem;
			if (!newitem_tracks_pointer)
				newitem = 0;
		}

		/* INCOMPLETE MIXED() ??? */
		if (m->class != MENU_CHOICE)
			m->curitem = newitem;
	}
	if (newitem) {
		/*
		 * If item is a menu, recurse.
		 */
		mi = m->item_list[newitem - 1];
		if (mi->pullright) {
			/* ACC_XVIEW */
			left_bdry = m->menurect.r_left + WALKMENU_BORDER_X(m)
					+ (column * Button_Width(m->ginfo,
							m->default_image.left_edge,
							m->default_key_image.left_edge,
							m->default_key_image.width));
			/* ACC_XVIEW */
			/*
			 * Note: Right margin computation mimics that employed by
			 * panel_paint_button.
			 */
			itemrect.r_left = left_bdry + mi->image.button_pos.x;
			if (m->class == MENU_MIXED) {   /* Ref (jkgfewrffgvwerfghv) */
				Rect def_rect;
				int mi_top;

				compute_menu_item_paint_rect(m, newitem, &def_rect, &mi_top);

				itemrect.r_top = m->menurect.r_top + WALKMENU_BORDER_Y(m) +
									def_rect.r_top - (def_rect.r_height / 2) +
									mi->image.button_pos.y;
			}
			else {
				itemrect.r_top = m->menurect.r_top + WALKMENU_BORDER_Y(m) +
						row * m->default_image.height + mi->image.button_pos.y;
			}
			/* ACC_XVIEW */
			itemrect.r_width =
					Button_Width(m->ginfo, m->default_image.left_edge,
					m->default_key_image.left_edge, m->default_key_image.width);
			/* ACC_XVIEW */
			itemrect.r_height = mi->image.button_size.y;
			menu_mark_rect.r_top = itemrect.r_top;
			menu_mark_rect.r_height = itemrect.r_height;
			menu_mark_rect.r_width = MenuMark_Width(m->ginfo) +
					ButtonEndcap_Width(m->ginfo);
			menu_mark_rect.r_left = itemrect.r_left + itemrect.r_width -
					menu_mark_rect.r_width;
			if (!mi->inactive	/* if active item... */
					&& (mi->value || mi->gen_pullright)	/* has submenu... */
					&&(((action_menu_is_down(event)
											|| event_action(event) ==
											ACTION_MENU)
									/* MENU is down... */
									&& compute_show_submenu(m, event,
											&menu_mark_rect, &submenu_stay_up)
							) || (event_action(event) == ACTION_RIGHT)
					)
					) {	/* ... then render submenu */
				/*
				 * If showing submenu in stay up mode, then gray-out menu
				 * item
				 */
				if (m->class == MENU_COMMAND && submenu_stay_up &&
						!m->group_info->setting_default)
				{
					feedback(m, m->curitem, MENU_BUSY_FEEDBACK);
				}
				if (m->class == MENU_MIXED && submenu_stay_up &&
						!m->group_info->setting_default)
				{
					feedback(m, m->curitem, MENU_BUSY_FEEDBACK);
				}
				/* Recurse */
				m->group_info->first_event = m->group_info->last_event = *event;

				if (render_pullright(m, mi, &itemrect, submenu_stay_up)
						== MENU_STATUS_ABORT) {
					cleanup(m, CLEANUP_ABORT);
					return;
				}

				goto get_next_input_event;
			}
		}
	}
	/*
	 * If button up is the menu button, then exit this menu. If SELECT-up in
	 * Click-Move-Click (m->stay_up == TRUE), then select menu item and exit
	 * menu.  Execute key (== RETURN) selects and/or exits.
	 */
	if (m->read_inputevent &&
			(event_action(event) == ACTION_DEFAULT_ACTION ||
					(event_is_up(event) &&
							(event_action(event) == ACTION_MENU ||
									(m->stay_up
											&& event_action(event) ==
											ACTION_SELECT))))) {
		if (m->curitem == 0 || m->item_list[m->curitem - 1]->inactive
				|| (m->item_list[m->curitem - 1]->no_feedback
						&& !m->item_list[m->curitem - 1]->title)) {
			if (!m->parent || !m->stay_up) {
				/*
				 * MENU-up on base menu, or MENU-up on Press-Drag-Release
				 * submenu: abort menu chain.
				 */
				cleanup(m, CLEANUP_ABORT);
				return;
			}
			else {
				/* MENU-up on Click-Move-Click submenu: reuse event */
				m->status = MENU_STATUS_PARENT;
				cleanup(m, CLEANUP_CLEANUP);
				return;
			}
		}
		mi = m->item_list[m->curitem - 1];
		if (mi->pullright) {
			if ((!m->stay_up && event_action(event) == ACTION_MENU) ||
					event_action(event) == ACTION_SELECT ||
					event_action(event) == ACTION_DEFAULT_ACTION) {
				if (mi->gen_pullright) {
					pullright_menu = (mi->gen_pullright) (MENU_ITEM_PUBLIC(mi),
							MENU_NOTIFY);
				}
				else
					pullright_menu = mi->value;
				if (pullright_menu)
					menu_select_default(pullright_menu);
				else
					newitem_tracks_pointer = FALSE;	/* abort menu group */
			}
			else
				/*
				 * MENU or SELECT up over row containing pullright, but not
				 * over the active area of the pullright: abort menu group
				 */
				newitem_tracks_pointer = FALSE;
		}
		if (newitem_tracks_pointer || keyboard_event) {
			if (m->pin && !m->item_list[0]->inactive && newitem == 1 &&
					!m->group_info->setting_default) {
				m->pin_window_pos.x = m->fs_menurect.r_left;
				m->pin_window_pos.y = m->fs_menurect.r_top;
				m->group_info->pinned_menu = m;
				m->status = MENU_STATUS_PIN;
				m->item_list[0]->inactive = TRUE;
				/* Draw inactive pushpin */
				state = OLGX_ERASE | OLGX_PUSHPIN_OUT | OLGX_INACTIVE;
				if (m->default_position == 1)
					state |= OLGX_DEFAULT;
				DRAWABLE_INFO_MACRO(m->window, info);
				olgx_draw_pushpin(m->ginfo, xv_xid(info),
						m->pushpin_left, m->pushpin_top, state);
			}
			else
				m->status = MENU_STATUS_DONE;
		}
		else
			m->status = MENU_STATUS_ABORT;
		cleanup(m, CLEANUP_EXIT);
		return;
	}
	/*
	 * Get next input event.
	 */
  get_next_input_event:
	m->status = MENU_STATUS_BUSY;
	return;
}


static void cleanup(register Xv_menu_info *m, Cleanup_mode cleanup_mode)
{
	Xv_object root_window = xv_get(m->group_info->client_window, XV_ROOT);
	Xv_Drawable_info *info;

	/*
	 * m->status        Actions at the next higher level
	 * ---------        --------------------------------
	 * MENU_STATUS_{PIN,ABORT}  abort menu chain.
	 * MENU_STATUS_DONE     valid selection, save selected item.
	 * MENU_STATUS_PARENT   cursor has entered a parent menu.
	 * MENU_STATUS_IGNORE   no menu item was selected; ignore last event
	 * MENU_STATUS_DEFAULT  select default item.
	 */

	switch (cleanup_mode) {
		case CLEANUP_EXIT:
			if (m->group_info->setting_default) {
				m->default_position = m->pending_default_position;
				if (m->group_info->depth == 1)
					goto abort;
			}
		case CLEANUP_CLEANUP:
		  cleanup:
			if (!m->group_info->setting_default
				&& (m->status == MENU_STATUS_DONE
					|| m->status == MENU_STATUS_PARENT
					|| m->status == MENU_STATUS_PIN
					|| m->status == MENU_STATUS_ABORT)
				&& m->curitem)
			{
				m->selected_position = m->curitem;
			}
			else if (m->status == MENU_STATUS_DEFAULT)
				m->selected_position = m->default_position;

			DRAWABLE_INFO_MACRO(root_window, info);
			if (m->window) {
				/* Note: Menu window must be unmapped before shadow window */
				xv_set(m->window, XV_SHOW, FALSE, NULL);
				m->active = FALSE;
				screen_set_cached_window_busy(xv_screen(info), m->window,
									FALSE);
				/* fix to make xv_window_loop work for menus */
				if (WIN_IS_IN_LOOP)
					notify_remove_event_func(m->window,
							(Notify_event_interposer_func)menu_window_event_proc,
							NOTIFY_IMMEDIATE);
				m->window = 0;
			}
			if (m->shadow_window) {
				Pixmap pix = (Pixmap)xv_get(m->shadow_window, XV_KEY_DATA,
									MENU_SHADOW_PIXMAP);

				xv_set(m->shadow_window, XV_SHOW, FALSE, NULL);
				if (pix) {
					xv_set(m->shadow_window,
							XV_KEY_DATA, MENU_SHADOW_PIXMAP, XV_NULL,
							NULL);
					XFreePixmap(xv_display(info), pix);
				}
				screen_set_cached_window_busy(xv_screen(info), m->shadow_window,
									FALSE);
				m->shadow_window = XV_NULL;
			}
			SERVERTRACE((44, "SERVER_JOURNALLING?\n"));
			if (xv_get(xv_server(info), SERVER_JOURNALLING)) {
				SERVERTRACE((44, " YES \n"));
				xv_set(xv_server(info), SERVER_JOURNAL_SYNC_EVENT, 1, NULL);
			}
			--m->group_info->depth;
			if (m->gen_items)
				destroy_gen_items(m);
			if (m->gen_proc) {
				(m->gen_proc) (MENU_PUBLIC(m), MENU_DISPLAY_DONE);
			}
			if (m->group_info->depth)
				submenu_done(m);
			else
				menu_done(m);
			return;

		case CLEANUP_ABORT:
		  abort:
			/* No selection has been made. */
			m->status = MENU_STATUS_ABORT;
			goto cleanup;
	}
}


/*
 * Compute max item size.  Only zero sized items have to be recomputed
 * The call_gen_proc parameter was added for menu accelerators.
 * We need to call this function more than once for the same menu.
 * The call_gen_proc parameter prevents us from calling the menu
 * item's gen proc more than once.
 */
Pkg_private int menu_compute_max_item_size(Xv_menu_info *menu,
	struct image *std_image,
    int	*status,	/* output parameter */
    int	call_gen_proc,
	int *height_sum)
{
	register int width, height, nitems, recompute;
	register Xv_menu_item_info *mi, **mip;
	int gen_items = FALSE;
	struct image *im = 0;
	int margin = 0;
	int heightsum = 0;
	int mixcmd = 0, mixchc = 0, mixtogg = 0;
	struct pr_size max_button_size;
	int pushpin_height;
	/* das ganze title_offset-Zeug dient nur dazu, die Hoehe eines
	 * Menues naeher an die Hoehe des Pin-Windows zu bringen:
	 * Jedes Menue hat ein 'default_image', das irgendwie die
	 * Groesse eines Items beschreibt. Vor dieser Aenderung war auch
	 * ein eventueller Title / Pushpin default_image-groessenrelevant.
	 * Aber letztlich habe ich nicht verstanden, wozu die Felder
	 * in struct image eigentlich alle dienen....
	 */
	int title_offset = 0;

	nitems = menu->nitems;
	width = height = max_button_size.x = max_button_size.y = 0;
	recompute = std_image->width == 0;

	/*
	 * This causes the menu to shrink around the items. When the std_image is
	 * available at the client interface zeroing the size of std_image should
	 * be rethought.
	 */
	std_image->width = std_image->height = 0;


	switch (menu->class) {
		case MENU_COMMAND:
		case MENU_CHOICE:
			margin = 0;
			break;
		case MENU_TOGGLE:
			/* see p_choice.c */
			margin = TOGGLE_Y_GAP;
			break;
		case MENU_MIXED:
			margin = 0; /* is totally unused (what about std_image ????) */
	}
	std_image->margin = margin;

	/* Compute max size if any of the items have changed */
	for (mip = menu->item_list; nitems--; mip++) {

		mi = *mip;
		mi->parent = menu;

		/* ACC_XVIEW */
		/*
		 * ISA: Skip if title item, but std_image passed is not label
		 */
		if (mi->title && (std_image->image_type != LABEL)) {
			continue;
		}
		if (mi->pullright || mi->mark_type & OLGX_DIAMOND_MARK)
			menu->menu_mark = 1;

		if (std_image->image_type == QUALIFIER) {

#ifdef OW_I18N
			_xv_use_pswcs_value_nodup(&mi->qual_image.string);
			if (!mi->qual_image.string.pswcs.value)
#else
			if (mi->qual_image.string == (char *)NULL)
#endif /* OW_I18N */
			{
				mi->qual_image.width = 0;
				continue;
			}
			im = &mi->qual_image;
		}
		if (std_image->image_type == KEY) {

#ifdef OW_I18N
			_xv_use_pswcs_value_nodup(&mi->key_image.string);
			if (!mi->key_image.string.pswcs.value)
#else
			if (mi->key_image.string == (char *)NULL)
#endif /* OW_I18N */
			{
				mi->key_image.width = 0;
				continue;
			}
			im = &mi->key_image;
		}
		if (std_image->image_type == LABEL)
			im = &mi->image;
		/* ACC_XVIEW */


		im->margin = margin;
		switch (mi->class) {
			case MENU_COMMAND:
				im->margin = 0;
				mixcmd++;
				break;
			case MENU_CHOICE:
				im->margin = 0;
				mixchc++;
				break;
			case MENU_TOGGLE:
				/* see p_choice.c */
				/* this margin seems to be something different:
				 *        im->margin = TOGGLE_Y_GAP;
				 */
				im->margin = 0;
				mixtogg++;
				break;
			case MENU_MIXED:
				MIXED();
		}

		/*
		 * Call menu item's gen proc if call_gen_proc set and gen proc exists
		 */
		if (call_gen_proc && mi->gen_proc) {
			*mip = mi = MENU_ITEM_PRIVATE((mi->gen_proc) (MENU_ITEM_PUBLIC(mi),
										  MENU_DISPLAY));
			gen_items = TRUE;
		}
		if (recompute)
			/* Force the image size to be recomputed */
			im->width = 0;

/* 		compare_height = im->height; */

		if (im->width == 0) {
			/* Compute standard image sizes */
			*status = menu_image_compute_size(menu, im, std_image);
			if (*status != XV_OK)
				return gen_items;

			/*
			 * Adjust MENU_COMMAND menu item image sizes to account for
			 * button outlines and vertically centered text.
			 */
			/* ACC_XVIEW */
			if (im->title && std_image->image_type == LABEL) {	/* only do this once */
				int defhig = im->height;

				/* bei Pulldown-Menues mit Pushpin war hier defhig = 0,
				 * das fuehrte zu ZU GROSSEN title_offsets
				 */
				if (defhig == 0) {
					defhig = Button_Height(menu->ginfo);
				}

				/* ACC_XVIEW */
				if (menu->pin) {
					/* The pushpin is width/2 from the left, and
					 * height/4 from the top.  Leave height/4
					 * as white space on the bottom of the pushpin.
					 */
					menu->pushpin_left = PushPinOut_Width(menu->ginfo) >> 1;
					menu->pushpin_top = PushPinOut_Height(menu->ginfo) >> 2;
					pushpin_height = (PushPinOut_Height(menu->ginfo) * 3) >> 1;
					im->width += 2 * PushPinOut_Width(menu->ginfo);
					if (im->height <= pushpin_height) {
						im->height = im->button_size.y = pushpin_height + 1;
					}
				}
				im->height += TEXT_LEDGE_HEIGHT;
				title_offset = im->height - defhig;
				im->button_size.y += TEXT_LEDGE_HEIGHT;
			}
			else if (menu->class == MENU_COMMAND) {
				if (im->svr_im) {
					im->button_size.x += OLGX_VAR_HEIGHT_BTN_MARGIN;
					im->button_size.y += OLGX_VAR_HEIGHT_BTN_MARGIN;
					im->width += OLGX_VAR_HEIGHT_BTN_MARGIN;
					im->height += OLGX_VAR_HEIGHT_BTN_MARGIN;
				}
				else {
					im->button_size.y = Button_Height(menu->ginfo);
					im->height = im->button_size.y;
					im->button_pos.y = title_offset;
				}
				if (mi->pullright) {
					/* Insure that the height of the menu item is
					 * at least as high as the menu mark.
					 */
					if (im->height < MenuMark_Height(menu->ginfo))
						im->height = MenuMark_Height(menu->ginfo);
					if (im->button_size.y < MenuMark_Height(menu->ginfo))
						im->button_size.y = MenuMark_Height(menu->ginfo);
				}
/* 				compare_height = im->height; */
			}
			else if (menu->class == MENU_MIXED) {
				if (mi->class == MENU_COMMAND) {
					if (im->svr_im) {
						im->button_size.x += OLGX_VAR_HEIGHT_BTN_MARGIN;
						im->button_size.y += OLGX_VAR_HEIGHT_BTN_MARGIN;
						im->width += OLGX_VAR_HEIGHT_BTN_MARGIN;
						im->height += OLGX_VAR_HEIGHT_BTN_MARGIN;
					}
					else {
						im->button_size.y = Button_Height(menu->ginfo);
						im->height = im->button_size.y;
						im->button_pos.y = title_offset;
					}
					if (mi->pullright) {
						/* Insure that the height of the menu item is
						 * at least as high as the menu mark.
						 */
						if (im->height < MenuMark_Height(menu->ginfo))
							im->height = MenuMark_Height(menu->ginfo);
						if (im->button_size.y < MenuMark_Height(menu->ginfo))
							im->button_size.y = MenuMark_Height(menu->ginfo);
					}
				}
				else {
					im->height += 6;
				}
			}
			else {
				/*
				 * Adjust height for 3 pixel-wide border, which extends in 1
				 * pixel on each side.
				 */
				im->height += 6;
			}
		}
		heightsum += im->height;
		width = imax(im->width, width);
		if (!(im->title && std_image->image_type == LABEL)) {
			height = imax(im->height, height);
			max_button_size.y = imax(im->button_size.y, max_button_size.y);
		}
		max_button_size.x = imax(im->button_size.x, max_button_size.x);

	}  /* end of for(each menu item) loop */

	std_image->width = width;
	std_image->height = height;
	std_image->button_size = max_button_size;

	if (height_sum) {
		/* ausprobiert: in etwa */
		switch (menu->class) {
			case MENU_COMMAND: heightsum += 8; break;
			case MENU_CHOICE: heightsum += 17; break;
			case MENU_TOGGLE: heightsum += 29; break;
			case MENU_MIXED: /* was nun ? */
				/* nur Raetselraten.... */
/* 				if (mixtogg > 0 && (mixcmd + mixchc > 0)) { */
					/* mindestens ein Toggle und noch was anderes */
/* 					heightsum += 20; */
/* 				} */
				break;
		}
		*height_sum = heightsum;
	}

	*status = XV_OK;
	return gen_items;
}


static void compute_item_row_column(Xv_menu_info *menu, int item_nbr,
	int	*row, int *column)
{
	item_nbr--;

	if (menu->column_major) {	/* column major layout */
		if (item_nbr > menu->nrows - 1 && menu->item_list[0]->title) {
			item_nbr = item_nbr - menu->nrows;
			*column = item_nbr / (menu->nrows - 1);
			*row = item_nbr % (menu->nrows - 1);
			(*column)++;
			(*row)++;
		}
		else {
			*column = item_nbr / menu->nrows;
			*row = item_nbr % menu->nrows;
		}

	}
	else {	/* row major layout */
		if (menu->item_list[0]->title) {
			if (item_nbr) {
				*row = ((item_nbr - 1) / menu->ncols) + 1;
				*column = (item_nbr - 1) % menu->ncols;
			}
			else
				*row = *column = 0;	/* title item */
		}
		else {
			*row = item_nbr / menu->ncols;
			*column = item_nbr % menu->ncols;
		}
	}
}

/*
 * Compute 3 rects:
 *	menu->menurect = menu rectangle relative to input event window
 *	mrect = menu rectangle in screen coordinates
 *	srect = shadow rectangle in screen coordinates
 */
static void compute_rects(Xv_menu_info *menu, struct inputevent *iep,
    int  item_height, Rect *mrect,
	Rect *srect	/* menu and shadow rect, in scrn coordinates */
)
{
	int left, top, row, column;
	Rect *enable_rect, *position_rect, *rootrect;
	Xv_object rootwindow;

	enable_rect = &menu->enable_rect;
	position_rect = &menu->position_rect;
	rootwindow = xv_get(iep->ie_win, XV_ROOT);
	rootrect = (Rect *) xv_get(rootwindow, WIN_RECT);

	/* Return the row and column of the default item. */
	compute_item_row_column(menu, menu->default_position, &row, &column);

	if (menu->popup) {
		/* Popup menu */
		menu->menurect.r_left = iep->ie_locx;
		if (menu->group_info->depth > 1) {
			/*
			 * Submenu: position pointer over default item.  Center of
			 * submenu default item equals center of parent menu item.
			 */
			menu->menurect.r_left -= WALKMENU_BORDER_X(menu) +
				menu->item_list[menu->default_position-1]->image.button_pos.x;
			menu->menurect.r_top = position_rect->r_top +
								position_rect->r_height / 2;
		}
		else {
			/*
			 * Base menu: position pointer just outside of menu.  Center of
			 * submenu default item equals pointer y-coordinate.
			 */

			if (defaults_get_boolean("openWindows.menuClickExecutesDefault",
									"OpenWindows.MenuClickExecutesDefault",
									FALSE))
				menu->menurect.r_left -= (PushPinOut_Width(menu->ginfo) >> 1);
			else
				menu->menurect.r_left++;

			menu->menurect.r_top = iep->ie_locy;
		}

		/* das funktioniert bei den 'homogenen' Menues,
		 * NICHT aber bei MENU_MIXED
		 */
		if (menu->class == MENU_MIXED) {   /* Ref (jkgfewrffgvwerfghv) */
			Rect def_rect;
			int mi_top;

			compute_menu_item_paint_rect(menu, menu->default_position,
										&def_rect, &mi_top);

			menu->menurect.r_top -= def_rect.r_top + def_rect.r_height / 2;
		}
		else {
			menu->menurect.r_top -= row * item_height + (3 * item_height) / 4;
		}
	}
	else {
		if (enable_rect->r_width)	/* if an enable rect has been defined */
			position_rect = enable_rect;
		if (menu->pulldown) {
			/* Pulldown menu */
			menu->menurect.r_left = position_rect->r_left +
						(position_rect->r_width ?
						/* Centered pulldown */
						(position_rect->r_width / 2 - mrect->r_width / 2) :
						/* Flush left: account for menu border width */
						1);
			menu->menurect.r_top = position_rect->r_top +
								position_rect->r_height;
		}
		else {
			/* Pullright menu */
			menu->menurect.r_left = position_rect->r_left +
								position_rect->r_width;
			menu->menurect.r_top = position_rect->r_top +
								position_rect->r_height / 2 -
								row * item_height - item_height / 2;
		}
	}
	menu->menurect.r_width = mrect->r_width;
	menu->menurect.r_height = mrect->r_height;

	/* Convert to screen coordinates */
	win_translate_xy(iep->ie_win, rootwindow,
						menu->menurect.r_left, menu->menurect.r_top,
						&left, &top);
	mrect->r_left = left;
	mrect->r_top = top;

	/*
	 * Make sure menu and shadow windows are entirely visible on the screen
	 */
	if (menu->group_info->three_d) {
		mrect->r_width += MENU_3D_SHADOW;
		mrect->r_height += MENU_3D_SHADOW;
	}
	else {
		mrect->r_width += MENU_2D_SHADOW;
		mrect->r_height += MENU_2D_SHADOW;
	}
	/* adjust fullscreen menu rect */
	constrainrect(mrect, rootrect);
	if (menu->group_info->three_d) {
		mrect->r_width -= MENU_3D_SHADOW;
		mrect->r_height -= MENU_3D_SHADOW;
	}
	else {
		mrect->r_width -= MENU_2D_SHADOW;
		mrect->r_height -= MENU_2D_SHADOW;
	}
	/* adjust input-event window menu rect */
	menu->menurect.r_left += mrect->r_left - left;
	menu->menurect.r_top += mrect->r_top - top;

	/* Shadow rect is menu rect offset by MENU_SHADOW */
	*srect = *mrect;
	if (menu->group_info->three_d) {
		srect->r_left += MENU_3D_SHADOW;
		srect->r_top += MENU_3D_SHADOW;
	}
	else {
		srect->r_left += MENU_2D_SHADOW;
		srect->r_top += MENU_2D_SHADOW;
		srect->r_width += 2 * WIN_DEFAULT_BORDER_WIDTH;
		srect->r_height += 2 * WIN_DEFAULT_BORDER_WIDTH;
	}
}


/*
 * Handle recursive calls for pullright items
 */
static Menu_status render_pullright(
    register Xv_menu_info *parent_menu,
    register Xv_menu_item_info *mi,
    Rect           *position_rect,
    int             stay_up	/* TRUE or FALSE */
)
{
	register Xv_menu_info *m;
	register Menu gen_menu;
	Menu_item_gen_proc_t gen_proc;

	if ((gen_proc = mi->gen_pullright)) {
		gen_menu = gen_proc(MENU_ITEM_PUBLIC(mi), MENU_DISPLAY);
		if (!gen_menu) {
			xv_error((Xv_opaque) mi,
					ERROR_STRING,
					XV_MSG
					("Pullright Generate Procedure failed to generate a pullright menu"),
					ERROR_PKG, MENU, NULL);
			return MENU_STATUS_ABORT;
		}
		m = MENU_PRIVATE(gen_menu);
		mi->value = gen_menu;
	}
	else {
		m = MENU_PRIVATE(mi->value);
	}

	/* Active menu is now the submenu */
	xv_set(mi->parent->group_info->server,
			XV_KEY_DATA, menu_active_menu_key, m, NULL);
	mi->parent->group_info->selected_menu = m;

	/* Ensure default image fonts valid */
	if (m->default_image.font == 0)
		/* ACC_XVIEW */
		m->default_image.font = m->default_key_image.font =
				m->default_qual_image.font = parent_menu->default_image.font;
	/* ACC_XVIEW */

	if (m->default_image.bold_font == 0)
		/* ACC_XVIEW */
		m->default_image.bold_font = m->default_key_image.bold_font =
				m->default_qual_image.bold_font =
				parent_menu->default_image.bold_font;
	/* ACC_XVIEW */

	/* Render submenu */
	m->enable_rect.r_width = 0;	/* submenus do not have an enable rect */
	m->popup = !stay_up;
	m->position_rect = *position_rect;
	m->pulldown = FALSE;
	m->state = MENU_STATE_INIT;
	m->stay_up = stay_up;
	menu_render(m, mi->parent->group_info, mi);
	return MENU_STATUS_DONE;
}


static void submenu_done(register Xv_menu_info *m)
{
	register Xv_menu_item_info *mi = m->parent;
	register Menu_item_gen_proc_t gen_proc;
	Xv_menu_info *parent_menu = mi->parent;

	if ((gen_proc = mi->gen_pullright))
		mi->value = (Xv_opaque) (gen_proc)(MENU_ITEM_PUBLIC(mi),
											MENU_DISPLAY_DONE);

	parent_menu->status = m->status;

	/* Active menu is now the parent of this submenu */
	xv_set(m->group_info->server,
						XV_KEY_DATA, menu_active_menu_key, parent_menu, NULL);

	/* Clean up the parent menu */
	switch (m->status) {
		case MENU_STATUS_PIN:
		case MENU_STATUS_DONE:
			cleanup(parent_menu, CLEANUP_EXIT);
			break;
		case MENU_STATUS_ABORT:
			cleanup(parent_menu, CLEANUP_ABORT);
			break;
		case MENU_STATUS_PARENT:
			m->group_info->selected_menu = parent_menu;
			if (event_action(&m->group_info->last_event) == ACTION_LEFT) {
				parent_menu->selected_position = parent_menu->curitem;
				paint_menu_item(parent_menu, parent_menu->curitem,
									parent_menu->default_position ==
									parent_menu->
									curitem ? MENU_SELECTED_DEFAULT_FEEDBACK :
									MENU_PROVIDE_FEEDBACK);
			}
			break;
		case MENU_STATUS_IGNORE:
			break;
		case MENU_STATUS_DEFAULT:
			m->status = MENU_STATUS_DONE;
			cleanup(parent_menu, CLEANUP_EXIT);
			break;
		case MENU_STATUS_BUSY:
			break;
	}
}


/*
 * Provide feedback directly to the pixwin. Someday this should be a client
 * settable option.
 */
static void feedback(
    Xv_menu_info *m,	/* ptr to menu information structure */
    int             n,		/* index of current menu item */
    Menu_feedback   state)
{
    Xv_menu_item_info *mi = m->item_list[n - 1];

    if (mi && !mi->no_feedback && !mi->inactive)
	paint_menu_item(m, n, state);
}


static void paint_menu_item(Xv_menu_info *m, int n,	/* menu item number */
    Menu_feedback   feedback_state)
{
	int color_index = -1;
	int height;
	Xv_Drawable_info *info;
	Xv_opaque label;
	Xv_opaque qual;
	Xv_opaque key;
	Xv_menu_item_info *mi;
	int mi_top;	/* placeholder for
				 * compute_menu_item_paint_rect output
				 * parameter */
	int olgx_state = 0;
	Pixlabel pixlabel;
	Rect rect;
	unsigned long save_black = 0;

	/* ACC_XVIEW */
	int label_pos, qual_pos, mark_pos, key_pos;
	int width;

	/* ACC_XVIEW */

	if (!m->window)
		return;	/* in case of race condition between unmap and expose */

	DRAWABLE_INFO_MACRO(m->window, info);
	mi = m->item_list[n - 1];
	if (mi->inactive)
		olgx_state = OLGX_INACTIVE;

	if (mi->title && m->pin) {
		switch (feedback_state) {
			case MENU_REMOVE_FEEDBACK:
				olgx_state |= OLGX_ERASE | OLGX_PUSHPIN_OUT;
				break;
			case MENU_PROVIDE_FEEDBACK:
			case MENU_BUSY_FEEDBACK:
				olgx_state |= OLGX_ERASE | OLGX_PUSHPIN_IN;
				break;
			case MENU_DEFAULT_FEEDBACK:
				olgx_state |= OLGX_ERASE | OLGX_PUSHPIN_OUT | OLGX_DEFAULT;
				break;
			case MENU_SELECTED_DEFAULT_FEEDBACK:
				olgx_state |= OLGX_ERASE | OLGX_PUSHPIN_IN | OLGX_DEFAULT;
				break;
		}
		olgx_draw_pushpin(m->ginfo, xv_xid(info), m->pushpin_left,
				m->pushpin_top, olgx_state);
		return;
	}

	olgx_state |= OLGX_MENU_ITEM;
	if (m->group_info->depth > 1
			&& feedback_state == MENU_DEFAULT_FEEDBACK
			&& !m->group_info->setting_default)
		feedback_state = MENU_REMOVE_FEEDBACK;

	if (mi->image.svr_im) {
		height = mi->image.button_size.y;
		pixlabel.pixmap = (XID) xv_get(mi->image.svr_im, XV_XID);
		pixlabel.width = ((Pixrect *) mi->image.svr_im)->pr_width;
		pixlabel.height = ((Pixrect *) mi->image.svr_im)->pr_height;
		label = (Xv_opaque) & pixlabel;
		/* ACC_XVIEW */
		qual = (Xv_opaque) NULL;
		key = (Xv_opaque) NULL;
		/* ACC_XVIEW */
		olgx_state |= OLGX_LABEL_IS_PIXMAP;
	}
	else {
		height = 0;	/* not used by olgx_draw_button */

#ifdef OW_I18N
		_xv_use_pswcs_value_nodup(&mi->image.string);
		label = (Xv_opaque) mi->image.string.pswcs.value;

		_xv_use_pswcs_value_nodup(&mi->qual_image.string);
		qual = (Xv_opaque) mi->qual_image.string.pswcs.value;

		_xv_use_pswcs_value_nodup(&mi->key_image.string);
		key = (Xv_opaque) mi->key_image.string.pswcs.value;

		olgx_state |= OLGX_LABEL_IS_WCS;
#else
		label = (Xv_opaque) mi->image.string;
		qual = (Xv_opaque) mi->qual_image.string;
		key = (Xv_opaque) mi->key_image.string;
#endif
	}
	if (mi->pullright) {
		olgx_state |= OLGX_HORIZ_MENU_MARK;
		/* ACC_XVIEW */
		mi->mark_type |= OLGX_HORIZ_MENU_MARK;
		/* ACC_XVIEW */
	}
	switch (feedback_state) {
		case MENU_BUSY_FEEDBACK:
			olgx_state |= OLGX_BUSY;
			break;
		case MENU_REMOVE_FEEDBACK:
			olgx_state |= OLGX_NORMAL | OLGX_ERASE;
			break;
		case MENU_PROVIDE_FEEDBACK:
			olgx_state |= OLGX_INVOKED;
			break;
		case MENU_SELECTED_DEFAULT_FEEDBACK:
			olgx_state |= OLGX_INVOKED | OLGX_DEFAULT;
			break;
		case MENU_DEFAULT_FEEDBACK:
			olgx_state |= OLGX_NORMAL | OLGX_ERASE | OLGX_DEFAULT;
			break;
	}

	if (mi->preview_proc) {
		unsigned new_invoked = ((olgx_state & OLGX_INVOKED) != 0);

		if (mi->is_invoked != new_invoked) {
			mi->is_invoked = new_invoked;
			(mi->preview_proc)(MENU_ITEM_PUBLIC(mi), new_invoked);
		}
	}

	/* Determine dimensions of menu item paint rectangle */
	compute_menu_item_paint_rect(m, n, &rect, &mi_top);

	if (xv_depth(info) > 1) {
		color_index = mi->color_index;
		if (color_index < 0)
			color_index = m->group_info->color_index;
	}
	if (color_index >= 0) {
		save_black = olgx_get_single_color(m->ginfo, OLGX_BLACK);
		olgx_set_single_color(m->ginfo, OLGX_BLACK,
				xv_get(xv_cms(info), CMS_PIXEL, color_index), OLGX_SPECIAL);
	}
	if (mi->image.font && mi->image.font != m->default_image.font)

#ifdef OW_I18N
		olgx_set_text_fontset(m->ginfo,
				xv_get(mi->image.font, FONT_SET_ID), OLGX_SPECIAL);
#else
		olgx_set_text_font(m->ginfo,
				(XFontStruct *) xv_get(mi->image.font, FONT_INFO),
				OLGX_SPECIAL);
#endif

	/* ACC_XVIEW */
	label_pos = MainLabel_Pos(m->ginfo, rect.r_left);
	if (m->default_qual_image.width == 0) {
		if (m->menu_mark) {
			mark_pos = label_pos + m->default_image.width +
					ButtonSpace_Width(m->ginfo);
		}
		else {
			mark_pos = label_pos + m->default_image.width;
		}
		qual_pos = mark_pos;
	}
	else {
		mark_pos = ButtonMark_Pos(m->ginfo, label_pos,
				m->default_image.width, m->default_qual_image.width);
		qual_pos = QualifierLabel_Pos(m->ginfo, mark_pos, mi->qual_image.width);
	}
	if (m->default_key_image.width == 0)
		key_pos = mark_pos + m->menu_mark * MenuMark_Width(m->ginfo);
	else
		key_pos = KeyLabel_Pos(m->ginfo, mark_pos);
	width = Button_Width(m->ginfo, rect.r_left, key_pos,
			m->default_key_image.width);


	/* ACC_XVIEW */

	/* Paint menu item */
	switch (m->class) {
		case MENU_COMMAND:
			/* ACC_XVIEW */
			olgx_draw_accel_button(m->ginfo, xv_xid(info), rect.r_left,
					rect.r_top, width, height,
					(void *)label, label_pos, (void *)qual, qual_pos,
					(int)mi->mark_type, mark_pos, (void *)key,
					key_pos, NULL, olgx_state);
			/* ACC_XVIEW */
			break;

		case MENU_TOGGLE:
		case MENU_CHOICE:
			/* ACC_XVIEW */
			olgx_draw_accel_choice_item(m->ginfo, xv_xid(info), rect.r_left,
					rect.r_top, width, rect.r_height,
					(void *)label, label_pos, (void *)qual, qual_pos,
					(int)mi->mark_type, mark_pos, (void *)key,
					key_pos, NULL, olgx_state);
			/* ACC_XVIEW */
			break;

		case MENU_MIXED:
			switch (mi->class) {
				case MENU_COMMAND:
					/* ACC_XVIEW */
					olgx_draw_accel_button(m->ginfo, xv_xid(info), rect.r_left,
							rect.r_top, width, height,
							(void *)label, label_pos, (void *)qual, qual_pos,
							(int)mi->mark_type, mark_pos, (void *)key,
							key_pos, NULL, olgx_state);
					/* ACC_XVIEW */
					break;

				case MENU_TOGGLE:
				case MENU_CHOICE:
					/* ACC_XVIEW */
					olgx_draw_accel_choice_item(m->ginfo, xv_xid(info),
							rect.r_left, rect.r_top, width, rect.r_height,
							(void *)label, label_pos, (void *)qual, qual_pos,
							(int)mi->mark_type, mark_pos, (void *)key,
							key_pos, NULL, olgx_state);
					/* ACC_XVIEW */
					break;

				case MENU_MIXED:
					MIXED();
					break;
			}
			break;
	}

	if (color_index >= 0)
		olgx_set_single_color(m->ginfo, OLGX_BLACK, save_black, OLGX_SPECIAL);
	if (mi->image.font && mi->image.font != m->default_image.font)

#ifdef OW_I18N
		olgx_set_text_fontset(m->ginfo,
				xv_get(m->default_image.font, FONT_SET_ID), OLGX_SPECIAL);
#else
		olgx_set_text_font(m->ginfo,
				(XFontStruct *) xv_get(m->default_image.font, FONT_INFO),
				OLGX_SPECIAL);
#endif
}


static void compute_menu_item_paint_rect(
    register Xv_menu_info *m,		/* ptr to menu information structure */
    register int item_nbr,	/* current menu item number */
    register Rect *rect,	/* paint rect (output parameter) */
    int *item_top	/* menu item top (output parameter) */
)
{
	int column, row, item_left, margin = m->default_image.margin;
	Xv_menu_item_info *mi = m->item_list[item_nbr - 1];

	compute_item_row_column(m, item_nbr, &row, &column);

	item_left = WALKMENU_BORDER_X(m) + column *
						Button_Width(m->ginfo, m->default_image.left_edge,
						m->default_key_image.left_edge,
						m->default_key_image.width);
	if (m->class == MENU_MIXED) {
		Xv_menu_item_info *mitem = m->item_list[row];

		*item_top = mitem->paint_rect.r_top;
	}
	else {
		*item_top = WALKMENU_BORDER_Y(m) + row * m->default_image.height;
	}

	switch (m->class) {
		case MENU_COMMAND:
			rect->r_left = item_left + mi->image.button_pos.x;
			rect->r_top = *item_top + mi->image.button_pos.y;
			/* ACC_XVIEW */
			rect->r_width = Button_Width(m->ginfo, m->default_image.left_edge,
								m->default_key_image.left_edge,
								m->default_key_image.width);
			/* ACC_XVIEW */
			rect->r_height = mi->image.button_size.y;
			break;

		case MENU_TOGGLE:
		case MENU_CHOICE:
			rect->r_left = item_left + margin - 2;
			rect->r_top = *item_top + margin - 2;
			/* ACC_XVIEW */
			rect->r_width = Button_Width(m->ginfo, m->default_image.left_edge,
								m->default_key_image.left_edge,
								m->default_key_image.width);
			/* ACC_XVIEW */
			rect->r_height = m->default_image.height - 2 * margin;
			/* On 2D choices, we want the previous choice item to overlap the
			 * next choice item by one pixel so that we have a single pixel
			 * thick line separating the choices.
			 * On 3D choices, we don't want any overlap so that the 3D effect
			 * is not destroyed.
			 */
			if (m->class == MENU_CHOICE && !m->group_info->three_d) {
				rect->r_width++;
				rect->r_height++;
			}
			break;

		case MENU_MIXED:
			/*  siehe oben Ref (jg4ferjgfv) - da wurden r_top und r_height
			 *  schon belegt.
			 */
			switch (mi->class) {
				case MENU_COMMAND:
					rect->r_left = item_left + mi->image.button_pos.x;
					rect->r_width = Button_Width(m->ginfo,
										m->default_image.left_edge,
										m->default_key_image.left_edge,
										m->default_key_image.width);
					break;

				case MENU_TOGGLE:
					rect->r_left = item_left + mi->image.margin - 2;
					rect->r_width = Button_Width(m->ginfo,
										m->default_image.left_edge,
										m->default_key_image.left_edge,
										m->default_key_image.width);
					break;

				case MENU_CHOICE:
					rect->r_left = item_left + mi->image.margin - 2;
					rect->r_width = Button_Width(m->ginfo,
										m->default_image.left_edge,
										m->default_key_image.left_edge,
										m->default_key_image.width);
					/* On 2D choices, we want the previous choice item to overlap the
					 * next choice item by one pixel so that we have a single pixel
					 * thick line separating the choices.
					 * On 3D choices, we don't want any overlap so that the 3D effect
					 * is not destroyed.
					 */
					if (!m->group_info->three_d) {
						rect->r_width++;
					}
					break;

				case MENU_MIXED:
					MIXED();
					break;
			}
			rect->r_top = mi->paint_rect.r_top;
			rect->r_height = mi->paint_rect.r_height;
			break;
	}
}


/*
 * Menu must be completely on the screen.
 */
static void constrainrect(struct rect *rconstrain, struct rect *rbound)
{
	/*
	 * Bias algorithm to have too big rconstrain fall off right/bottom.
	 */
	if (rect_right(rconstrain) > rect_right(rbound)) {
		rconstrain->r_left = rbound->r_left + rbound->r_width
							- rconstrain->r_width;
	}
	if (rconstrain->r_left < rbound->r_left) {
		rconstrain->r_left = rbound->r_left + SCREEN_MARGIN;
	}
	if (rect_bottom(rconstrain) > rect_bottom(rbound)) {
		rconstrain->r_top = rbound->r_top + rbound->r_height
							- rconstrain->r_height;
	}
	if (rconstrain->r_top < rbound->r_top) {
		rconstrain->r_top = rbound->r_top + SCREEN_MARGIN;
	}
	return;
}


/*
 * Clean up any client generated items
 */
static void destroy_gen_items(Xv_menu_info *menu)
{
    register int    nitems;
    register Xv_menu_item_info *mi, **mip;

    nitems = menu->nitems;
    /* Give client a chance to clean up any generated items */
    for (mip = menu->item_list; mi = *mip, nitems--; mip++)
	if (mi->gen_proc)
	    *mip = MENU_ITEM_PRIVATE(
		  (mi->gen_proc) (MENU_ITEM_PUBLIC(mi), MENU_DISPLAY_DONE));
}



/* Return: TRUE= show submenu; FALSE= don't show submenu */
static short compute_show_submenu(
    Xv_menu_info   *m,
    Event          *event,
    Rect           *submenu_region_rect,
    int            *submenu_stay_up
)
{
	register short x_delta;

	*submenu_stay_up = FALSE;	/* assume Press-Drag-Release */

	/* In Click-Move-Click mode, if event is MENU-up, then show submenu. */
	if (m->stay_up && event_action(event) == ACTION_MENU && event_is_up(event)
						&& m->group_info->menu_down_event.action) {
		*submenu_stay_up = TRUE;
		return (TRUE);
	}
	/*
	 * If event is LOC_DRAG, and we've continuously moved to the right the
	 * specified drag-right distance, or we're over the submenu region, then
	 * show the submenu. FYI, event->ie_locx == m->group_info->last_event.
	 */
	if (event_action(event) == LOC_DRAG) {
		if (rect_includespoint(submenu_region_rect,
												event->ie_locx,
												event->ie_locy)) {
			/* Over the submenu region: bring up submenu */
			m->drag_right_distance = 0;
			return TRUE;
		}
		x_delta = event->ie_locx - m->group_info->previous_event.ie_locx;
		if (x_delta < 0) {
			/* User has moved left: cancel the drag right */
			m->drag_right_distance = 0;
			return FALSE;
		}
		else {
			m->drag_right_distance += x_delta;
			if (m->drag_right_distance > m->pullright_delta) {
				/*
				 * User has moved the pullright-delta distance right: bring
				 * up the submenu
				 */
				m->drag_right_distance = 0;
				return TRUE;
			}
		}
	}
	return FALSE;
}


void menu_window_event_proc(Xv_Window window, Event *event)
{
	register Xv_menu_info *m;

	m = (Xv_menu_info *) xv_get(window, XV_KEY_DATA, MENU_WINDOW_MENU);
	if (!m || !m->group_info)
		return;
	if (m->group_info->client_window && event_id(event) == WIN_REPAINT)
		menu_window_paint(m, window);
}


static void menu_window_paint(Xv_menu_info *m, Xv_Window window)
{
	register int i;
	int default_item = 0;
	Xv_Drawable_info *info;
	Menu_feedback feedback_state;
	Font font;

#ifdef OW_I18N
	XFontSet font_set;
	Display *display;
	XRectangle overall_ink_extents = { 0 };
	XRectangle overall_logical_extents = { 0 };
#else
	XFontStruct *font_info;
#endif /* OW_I18N */

	Xv_menu_item_info *mi;
	Rect mi_rect;
	int mi_top;
	int on = 0;
	int state;
	int text_ascent = 0;
	int text_descent = 0;
	int text_direction = 0;
	int text_length;

#ifndef OW_I18N
	XCharStruct text_overall_return;
#endif /* OW_I18N */

	int width;
	int x;
	Drawable xid;
	int y;

#ifndef OW_I18N
	/*
	 * Initialize text_overall_return to zeros.
	 * It is not initialized automatically above because the MIT 
	 * build (using cc), complains about "no automatic aggregate initialization"
	 */
	XV_BZERO(&text_overall_return, sizeof(XCharStruct));
#endif /* OW_I18N */

	if (!m->group_info)	/* catch unexplained race condition */
		return;
	m->rendered = TRUE;

	DRAWABLE_INFO_MACRO(window, info);
	xid = xv_xid(info);

#ifdef OW_I18N
	display = xv_display(info);
#endif /* OW_I18N */

	/*
	 * If 3D, then draw the (shadow) border
	 */
	if (m->group_info->three_d) {
		XDrawRectangle(xv_display(info), xid, m->ginfo->gc_rec[OLGX_BLACK]->gc,
				0, 0, (unsigned)m->menurect.r_width - 1,
				(unsigned)m->menurect.r_height - 1);
	}

	/*
	 * Draw the menu pushpin, title and items
	 */
	if (m->pin) {
		if (m->curitem == 1)
			state = OLGX_ERASE | OLGX_PUSHPIN_IN;
		else
			state = OLGX_ERASE | OLGX_PUSHPIN_OUT;
		if (m->default_position == 1)
			state |= OLGX_DEFAULT;
		if (m->item_list[0]->inactive)
			state |= OLGX_INACTIVE;
		olgx_draw_pushpin(m->ginfo, xid,
				m->pushpin_left, m->pushpin_top, state);
	}
	i = 1;
	for (; i <= m->nitems; i++) {
		mi = m->item_list[i - 1];

#ifdef  OW_I18N
		if (_xv_is_string_attr_exist_nodup(&mi->image.string) && mi->title) {
			compute_menu_item_paint_rect(m, i, &mi_rect, &mi_top);
			font = m->default_image.bold_font;
			font_set = (XFontSet) xv_get(font, FONT_SET_ID);
			_xv_use_pswcs_value_nodup(&mi->image.string);
			text_length = wslen(mi->image.string.pswcs.value);
			XwcTextExtents(font_set, mi->image.string.pswcs.value,
					text_length, &overall_ink_extents,
					&overall_logical_extents);
#else
		if (mi->image.string && mi->title) {
			compute_menu_item_paint_rect(m, i, &mi_rect, &mi_top);
			font = m->default_image.bold_font;
			font_info = (XFontStruct *) xv_get(font, FONT_INFO);
			text_length = strlen(mi->image.string);
			XTextExtents(font_info, mi->image.string, text_length,
					&text_direction, &text_ascent, &text_descent,
					&text_overall_return);
#endif /* OW_I18N */

			x = MENU_TITLE_MARGIN;
			width = m->menurect.r_width - 2 * MENU_TITLE_MARGIN;
			if (m->pin) {
				x += 2 * PushPinOut_Width(m->ginfo);
				width -= 2 * PushPinOut_Width(m->ginfo);
			}

#ifdef OW_I18N
			x += (width - overall_logical_extents.width) / (unsigned)2;
			y = mi_rect.r_top - overall_logical_extents.y - TEXT_LEDGE_HEIGHT +
					(mi_rect.r_height -
					overall_logical_extents.height) / (unsigned)2;
			olgx_set_text_fontset(m->ginfo, xv_get(font, FONT_SET_ID),
					OLGX_SPECIAL);
			olgx_draw_text(m->ginfo, xid, mi->image.string.pswcs.value, x, y, 0,
					OLGX_NORMAL | OLGX_LABEL_IS_WCS);
			olgx_set_text_fontset(m->ginfo, xv_get(m->default_image.font,
							FONT_SET_ID), OLGX_SPECIAL);
#else
			x += (width - text_overall_return.width) / 2;
			y = mi_rect.r_top + text_ascent - TEXT_LEDGE_HEIGHT +
					(mi_rect.r_height - (text_ascent + text_descent)) / 2;
			olgx_set_text_font(m->ginfo, (XFontStruct *) xv_get(font,
							FONT_INFO), OLGX_SPECIAL);
			olgx_draw_text(m->ginfo, xid, mi->image.string, x, y, 0,
					OLGX_NORMAL);
			olgx_set_text_font(m->ginfo,
					(XFontStruct *) xv_get(m->default_image.font, FONT_INFO),
					OLGX_SPECIAL);
#endif /* OW_I18N */

			olgx_draw_text_ledge(m->ginfo, xid, MENU_TITLE_MARGIN,
					rect_bottom(&mi_rect) - TEXT_LEDGE_HEIGHT - 1,
					m->menurect.r_width - 2 * MENU_TITLE_MARGIN);
		}
		if (!mi->no_feedback) {
			if (m->group_info->setting_default)
				feedback_state = (i == m->default_position) ?
						MENU_DEFAULT_FEEDBACK : MENU_REMOVE_FEEDBACK;
			else {
				switch (m->class) {
					case MENU_TOGGLE:
						on = mi->toggle_on;
						default_item = m->default_position == i;
						break;
					case MENU_CHOICE:
						on = m->curitem == i;
						default_item = m->default_position == i;
						break;
					case MENU_COMMAND:
						on = m->curitem == i;
						if (!m->curitem && m->group_info->depth == 1)
							default_item = m->default_position == i;
						else
							default_item = FALSE;
						break;
					case MENU_MIXED:
						switch (mi->class) {
							case MENU_TOGGLE:
								on = mi->toggle_on;
								default_item = m->default_position == i;
								break;
							case MENU_CHOICE:
								on = mi->selected;
								default_item = m->default_position == i;
								break;
							case MENU_COMMAND:
								on = m->curitem == i;
								if (!m->curitem && m->group_info->depth == 1)
									default_item = m->default_position == i;
								else
									default_item = FALSE;
								break;
							case MENU_MIXED:
								MIXED();
								break;
						}
						break;
				}
				if (!mi->inactive && on) {
					if (default_item)
						feedback_state = MENU_SELECTED_DEFAULT_FEEDBACK;
					else
						feedback_state = MENU_PROVIDE_FEEDBACK;
				}
				else {
					if (default_item)
						feedback_state = MENU_DEFAULT_FEEDBACK;
					else
						feedback_state = MENU_REMOVE_FEEDBACK;
				}
			}
			paint_menu_item(m, i, feedback_state);
		}
	}
}


static void
repaint_menu_group(Xv_menu_info *m)
{
	Xv_menu_item_info *mi;

	while (m) {
		if (m->group_info->setting_default) {
			if (m->curitem && m->curitem != m->pending_default_position)
				paint_menu_item(m, m->curitem, MENU_REMOVE_FEEDBACK);
			paint_menu_item(m, m->pending_default_position,
								MENU_DEFAULT_FEEDBACK);
		}
		else {
			if (m->curitem != m->default_position)
				/* Paint default ring for base menu; no feedback for
				 * submenus.
				 */
				paint_menu_item(m, m->default_position,
									m->parent == NULL ? MENU_DEFAULT_FEEDBACK :
									MENU_REMOVE_FEEDBACK);
			if (m->curitem)
				paint_menu_item(m, m->curitem, MENU_PROVIDE_FEEDBACK);
		}
		mi = m->parent;
		if (!mi)
			break;
		m = mi->parent;
	}
}


Pkg_private Notify_value menu_client_window_event_proc(
    Xv_Window       win,	/* menu client window */
    Notify_event ev, Notify_arg      arg, Notify_event_type typ)
{
    Event *event = (Event *)ev;
	register Xv_menu_info *m;
	Xv_Window client_window;
	int client_height;
	int client_width;
	int column;
	char *help_data, help_data_buf[100];
	int help_item;
	int keyboard_event = FALSE;
	int mi_top;	/* menu item top */
	Rect rect;
	int row;

	m = (Xv_menu_info *) xv_get(XV_SERVER_FROM_WINDOW(win),
						XV_KEY_DATA, menu_active_menu_key);
	if (!m)
		return notify_next_event_func(win, ev, arg, typ);

	if (m->select_is_menu) {
		if (event_action(event) == ACTION_SELECT)
			event_set_action(event, ACTION_MENU);
		if (action_select_is_down(event))
			/* BUG ALERT: The following is not remappable */
			event_set_shiftmask(event, MS_RIGHT_MASK);
	}

	/* Translate unmodified ISO (ASCII) Mouseless Keyboard Commands
	 * used inside a menu.
	 */
	if (xv_translate_iso(event)) keyboard_event = TRUE;

	switch (event_action(event)) {
		case KBD_USE:
		case KBD_DONE:
			/* Don't set keyboard focus on client window */
			return (NOTIFY_IGNORED);
		case WIN_REPAINT:
			/* Repaint client window */
			return notify_next_event_func(win, ev, arg, typ);
		case ACTION_VERTICAL_SCROLLBAR_MENU:
		case ACTION_HORIZONTAL_SCROLLBAR_MENU:
			event_set_action(event, ACTION_MENU);
			/* ... fall through to ACTION_MENU */
		case ACTION_MENU:
			if (event_is_down(event))
				m->group_info->menu_down_event = *event;
			else if (!event_is_button(event)) {
				m->state = MENU_STATE_TRACK_MOUSE;
				return (NOTIFY_IGNORED);	/* ignore keyboard MENU-up event */
			}
			break;
		case ACTION_SELECT:
		case LOC_DRAG:
			break;
		case ACTION_UP:
		case ACTION_DOWN:
		case ACTION_LEFT:
		case ACTION_RIGHT:
			keyboard_event = TRUE;
			/* Use the down event in order to allow auto-repeat to work */
			if (event_is_up(event))
				return (NOTIFY_IGNORED);
			break;
		case ACTION_CANCEL:
		case ACTION_DEFAULT_ACTION:
			keyboard_event = TRUE;
			/* Dismiss the menu on the up keystroke so there is no up event
			 * left hanging around for the client window to process.
			 */
			if (event_is_down(event))
				return (NOTIFY_IGNORED);
			else if (m->state == MENU_STATE_GET_MODE) {
				m->state = MENU_STATE_TRACK_MOUSE;
				return (NOTIFY_IGNORED);
			}
			break;
		case ACTION_HELP:
		case ACTION_MORE_HELP:
		case ACTION_INPUT_FOCUS_HELP:
			if (event_is_down(event)
				|| !rect_includespoint(&m->menurect, event->ie_locx,
							  		event->ie_locy))
				return (NOTIFY_IGNORED);
			if (m->pin
				&& rect_includespoint(&m->pushpin_rect, event->ie_locx,
														 event->ie_locy))
				help_data = "xview:menuPushpin";
			else {
				/* Find the menu item under the pointer */
				help_item = find_item_nbr(m, event, &row, &column);
				if (help_item > m->nitems)
					help_item = XV_NULL;	/* inside the menu, but not no menu items */
				/* Get the help data string for the menu item */
				help_data = NULL;
				if (help_item) {
					help_data = (char *)xv_get(MENU_ITEM_PUBLIC(
									m->item_list[help_item - 1]),
									XV_HELP_DATA);
					if (help_data) {
						if (xv_get(MENU_ITEM_PUBLIC(m->item_list[help_item-1]),
									MENU_INACTIVE)) {
							sprintf(help_data_buf, "%s_D", help_data);
							help_data = help_data_buf;
						}
					}
				}
				if (!help_data)
					help_data = (char *)xv_get(MENU_PUBLIC(m), XV_HELP_DATA);
				if (!help_data)
					help_data = "xview:menu";
			}
			/* Translate the event from client window to menu window coordinates */
			event->ie_locx -= m->menurect.r_left;
			event->ie_locy -= m->menurect.r_top;
			/* Show help for menu item under the pointer */
			xv_set(m->window,
				XV_KEY_DATA, WIN_FRAME,
								xv_get(m->group_info->client_window, WIN_FRAME),
				NULL);

			/*
			 * Instead of calling xv_help_show, we: 
			 * -    save the image with xv_help_save_image()
			 * -    bring down the menu and release the grabs with cleanup()
			 * -    render the help frame with xv_help_render()
			 */

			/* 
			 * remember client window before cleanup() 
			 */
			client_window = m->group_info->client_window;

			client_width = (int)xv_get(m->window, XV_WIDTH);
			client_height = (int)xv_get(m->window, XV_HEIGHT);
			if (event_action(event) != ACTION_MORE_HELP &&
								event_action(event) != ACTION_MORE_TEXT_HELP)
				xv_help_save_image(m->window, client_width, client_height,
									event_x(event), event_y(event));

			m->curitem = 0;
			cleanup(m, CLEANUP_ABORT);

			xv_help_render(client_window, help_data, event);
			return (NOTIFY_DONE);
		case SHIFT_LEFTCTRL:
		case SHIFT_RIGHTCTRL:
			m->group_info->setting_default =
								event_is_down(event) ? TRUE : FALSE;
			if (m->curitem && m->item_list[m->curitem - 1]->inactive)
				return (NOTIFY_DONE);
			if (m->group_info->setting_default)
				m->pending_default_position = m->curitem ? m->curitem :
									m->default_position;
			else
				m->default_position = m->pending_default_position;
			repaint_menu_group(m);
			return (NOTIFY_DONE);
		case ACTION_NEXT_ELEMENT:
		case ACTION_PREVIOUS_ELEMENT:
			cleanup(m, CLEANUP_ABORT);
			return notify_next_event_func(win, ev, arg, typ);
		case ACTION_JUMP_MOUSE_TO_INPUT_FOCUS:
			if (m->curitem && m->window) {
				/* Position the cursor so that if the user clicks
				 * the SELECT mouse button, the item is selected.
				 */
				if (m->curitem == 1 && m->pin) {
					xv_set(m->window,
						WIN_MOUSE_XY, m->pushpin_rect.r_left-m->menurect.r_left,
									m->pushpin_rect.r_top -m->menurect.r_top,
						NULL);
				}
				else {
					compute_menu_item_paint_rect(m, m->curitem, &rect, &mi_top);
					xv_set(m->window, WIN_MOUSE_XY, rect.r_left + 1, mi_top, NULL);
				}
			}
			return (NOTIFY_DONE);
		default:
			if (event_is_iso(event)) {
				/* Do pattern matching to find requested menu item */
				if ((event_action(event) >= 'A' && event_action(event) <= 'Z')
					|| (event_action(event) >= 0xC0
					&& event_action(event) <= 0xDE))
					event->action = event_action(event) + 0x20;

				keyboard_event = TRUE;
				/* Use the down event in order to allow auto-repeat to work */
				if (event_is_up(event))
					return (NOTIFY_IGNORED);
				break;
			}
			/*
			 * Call the next interposed event func
			 */
			return notify_next_event_func(win, ev, arg, typ);
	}

	if (m->state == MENU_STATE_TRACK_MOUSE)
		m->read_inputevent = TRUE;
	else if (m->state == MENU_STATE_GET_MODE)
		get_mode(m, event);	/* may change menu state and
							   * m->read_inputevent */

	if (m->state == MENU_STATE_TRACK_MOUSE)
		do {
			process_event(m, event);
			if (!m->group_info)
				return (NOTIFY_DONE);	/* menu_done executed */
			m = (Xv_menu_info *) xv_get(m->group_info->server,
								XV_KEY_DATA, menu_active_menu_key);
		} while (!keyboard_event && m && m->status == MENU_STATUS_PARENT);

	if (m && event_action(event) == ACTION_MENU && event_is_up(event))
		/* MENU-up processed: corresponding MENU-down event no longer valid */
		m->group_info->menu_down_event.action = 0;

	return (NOTIFY_DONE);
}
