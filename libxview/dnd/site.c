#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)site.c 1.13 93/06/28 DRA: $Id: site.c,v 4.3 2026/05/13 14:06:49 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <sys/time.h>
#include <X11/Xlib.h>
#include <xview/pkg.h>
#include <xview/attr.h>
#include <xview/rect.h>
#include <xview/window.h>
#include <xview_private/xv_list.h>
#include <xview_private/site_impl.h>
#include <xview_private/windowimpl.h>
#include <xview_private/dndimpl.h>
#include <xview_private/xv_list.h>
#include <assert.h>

#define DNDS_BIT_FIELD(field)		unsigned	field:1
#define dnds_status(item, field)		((item)->status_bits.field)
#define dnds_status_set(item, field)       	dnds_status(item, field) = TRUE
#define dnds_status_reset(item, field)     	dnds_status(item, field) = FALSE


#define DND_SITE_PRIVATE(dnd_site_public) \
		XV_PRIVATE(Dnd_site_info, Xv_dropsite, dnd_site_public)
#define DND_SITE_PUBLIC(site)         XV_PUBLIC(site)

typedef enum dnd_region_ops {
	Dnd_Add_Window, 	Dnd_Delete_Window,
	Dnd_Add_Window_Ptr, 	Dnd_Delete_Window_Ptr,
	Dnd_Add_Rect, 		Dnd_Delete_Rect,
	Dnd_Add_Rect_Ptr, 	Dnd_Delete_Rect_Ptr,
	Dnd_Get_Window,		Dnd_Get_Window_Ptr,
	Dnd_Get_Rect,		Dnd_Get_Rect_Ptr,
	Dnd_Delete_All_Rects,	Dnd_Delete_All_Windows
} Dnd_region_ops;

typedef struct dnd_window_list {
	Xv_sl_link	next;
	Xv_Window	window;
} Dnd_window_list;

typedef struct dnd_rect_list {
	Xv_sl_link	next;
	int		real_x;
	int		real_y;
	Rect		rect;
} Dnd_rect_list;

typedef struct dnd_site_info {
	Xv_drop_site	 public_self;
	Xv_window	 owner;
	Window		 owner_xid;
	long		 site_id;
	int		 event_mask;
	unsigned int	 site_size;

	struct {
		DNDS_BIT_FIELD(site_id_set);
		DNDS_BIT_FIELD(window_set);
		DNDS_BIT_FIELD(is_window_region);
		DNDS_BIT_FIELD(created);
	} status_bits;

	union {
	    Dnd_window_list *windows;
	    Dnd_rect_list   *rects;
	} region;
	unsigned int	 num_regions;
} Dnd_site_info;
static void TransCoords(Dnd_site_info *site, Dnd_rect_list *node)
{
    Xv_Window 	frame, window;
    int		x, y;

    frame = win_get_top_level(site->owner);
    assert(frame != (Xv_opaque)XV_ERROR);

    x = node->rect.r_left;
    y = node->rect.r_top;
    window = site->owner;

    while (window != frame) {
	int bw = xv_get(window, WIN_BORDER);
	x += xv_get(window, XV_X) + bw;
	y += xv_get(window, XV_Y) + bw;
	window = xv_get(window, XV_OWNER);
    }
    node->real_x = x;
    node->real_y = y;
}

static Xv_opaque DndDropAreaOps(Dnd_site_info	*site, Dnd_region_ops mode,
							Xv_opaque area)
{
	switch (mode) {
		case Dnd_Add_Window:
			{
				register Dnd_window_list *winNode;

				/* Create the head of the list.  Never used. */
				if (!site->region.windows) {
					site->region.windows = xv_alloc(Dnd_window_list);
					XV_SL_INIT(site->region.windows);
				}

				/* Create the next node and place window in it. */
				winNode = xv_alloc(Dnd_window_list);
				winNode->window = (Xv_Window) area;

				site->num_regions++;

				/* Add the new node to the site list. */
				XV_SL_ADD_AFTER(site->region.windows, site->region.windows,
						winNode);
			}
			break;
		case Dnd_Add_Rect:
			{
				register Dnd_rect_list *rectNode;
				register Rect *rect;

				/* Create the head of the list.  Never used. */
				if (!site->region.rects) {
					site->region.rects = xv_alloc(Dnd_rect_list);
					XV_SL_INIT(site->region.rects);
				}
				/* Create the next node and place the rect in it. */
				rectNode = xv_alloc(Dnd_rect_list);

				rect = (Rect *) area;
				rectNode->rect.r_left = rect->r_left;
				rectNode->rect.r_top = rect->r_top;
				rectNode->rect.r_width = rect->r_width;
				rectNode->rect.r_height = rect->r_height;
				TransCoords(site, rectNode);

				site->num_regions++;

				/* Add the new node to the site list. */
				XV_SL_ADD_AFTER(site->region.rects, site->region.rects,
						rectNode);
			}
			break;
		case Dnd_Delete_Window:
			{
				register Dnd_window_list *winNode, *nodePrev;

				if (!site->region.windows)
					return (XV_ERROR);

				nodePrev = winNode = site->region.windows;

				while ((winNode =
								(Dnd_window_list *) (XV_SL_SAFE_NEXT(winNode))))
				{
					if (winNode->window == (Xv_Window) area) {
						xv_free(XV_SL_REMOVE_AFTER(site->region.windows,
										nodePrev));
						site->num_regions--;
						return (XV_OK);
					}
					nodePrev = winNode;
				}
				return (XV_ERROR);
			}
			/* NOTREACHED */
			break;
		case Dnd_Delete_Rect:
			{
				register Dnd_rect_list *rectNode, *nodePrev;
				register Rect *rect = (Rect *) area;

				if (!site->region.rects)
					return (XV_ERROR);

				nodePrev = rectNode = site->region.rects;

				while ((rectNode =
								(Dnd_rect_list *) (XV_SL_SAFE_NEXT(rectNode))))
				{
					if (rect_equal(rect, &rectNode->rect)) {
						xv_free(XV_SL_REMOVE_AFTER(site->region.rects,
										nodePrev));
						site->num_regions--;
						return (XV_OK);
					}
					nodePrev = rectNode;
				}
				return (XV_ERROR);
			}
			/* NOTREACHED */
			break;
		case Dnd_Add_Window_Ptr:
			{
				register Xv_Window *windows;

				/* Create the head of the list.  Never used. */
				if (!site->region.windows) {
					site->region.windows = xv_alloc(Dnd_window_list);
					XV_SL_INIT(site->region.windows);
				}

				for (windows = (Xv_Window *) area; *windows; windows++) {
					register Dnd_window_list *winNode;

					/* Create the next node and place a window in it. */
					winNode = xv_alloc(Dnd_window_list);
					winNode->window = (Xv_Window) * windows;

					site->num_regions++;

					/* Add the new node to the site list. */
					XV_SL_ADD_AFTER(site->region.windows, site->region.windows,
							winNode);
				}
			}
			break;
		case Dnd_Add_Rect_Ptr:
			{
				register Rect *rects;

				/* Create the head of the list.  Never used. */
				if (!site->region.rects) {
					site->region.rects = xv_alloc(Dnd_rect_list);
					XV_SL_INIT(site->region.rects);
				}

				for (rects = (Rect *) area; rects && !rect_isnull(rects);
						rects++) {
					register Dnd_rect_list *rectNode;

					/* Create the next node and place a rect in it. */
					rectNode = xv_alloc(Dnd_rect_list);
					rectNode->rect.r_left = rects->r_left;
					rectNode->rect.r_top = rects->r_top;
					rectNode->rect.r_width = rects->r_width;
					rectNode->rect.r_height = rects->r_height;
					TransCoords(site, rectNode);

					site->num_regions++;

					/* Add the new node to the site list. */
					XV_SL_ADD_AFTER(site->region.rects, site->region.rects,
							rectNode);
				}
			}
			break;
		case Dnd_Delete_Window_Ptr:
			{
				register Xv_Window *windows;

				if (!site->region.windows)
					return (XV_ERROR);

				/* REMIND: These two loops must be optimized. */

				for (windows = (Xv_Window *) area; *windows; windows++) {
					register Dnd_window_list *winNode, *nodePrev;

					nodePrev = winNode = site->region.windows;

					while ((winNode =
									(Dnd_window_list
											*) (XV_SL_SAFE_NEXT(winNode)))) {
						if (winNode->window == (Xv_Window) * windows) {
							xv_free(XV_SL_REMOVE_AFTER(site->region.windows,
											nodePrev));
							site->num_regions--;
							break;
						}
						nodePrev = winNode;
					}
				}
			}
			break;
		case Dnd_Delete_Rect_Ptr:
			{
				register Rect *rects;

				if (!site->region.rects)
					return (XV_ERROR);

				/* REMIND: These two loops must be optimized. */

				for (rects = (Rect *) area; rects && !rect_isnull(rects);
						rects++) {
					register Dnd_rect_list *rectNode, *nodePrev;

					nodePrev = rectNode = site->region.rects;

					while ((rectNode =
									(Dnd_rect_list
											*) (XV_SL_SAFE_NEXT(rectNode)))) {
						if (rect_equal(rects, &rectNode->rect)) {
							xv_free(XV_SL_REMOVE_AFTER(site->region.rects,
											nodePrev));
							site->num_regions--;
							break;
						}
						nodePrev = rectNode;
					}
				}
			}
			break;
		case Dnd_Get_Window:
			{
				register Dnd_window_list *winNode = site->region.windows;

				if (!site->region.windows)
					return (XV_ERROR);

				/* Since the head of the list is not used, get the next node. */
				winNode = (Dnd_window_list *) (XV_SL_SAFE_NEXT(winNode));

				return (winNode->window);
			}
			/* NOTREACHED */
			break;
		case Dnd_Get_Rect:
			{
				register Dnd_rect_list *rectNode = site->region.rects;
				Rect *rect;

				if (!site->region.rects)
					return (XV_ERROR);

				/* Since the head of the list is not used, get the next node. */
				rectNode = (Dnd_rect_list *) (XV_SL_SAFE_NEXT(rectNode));

				if (!rectNode)
					return (XV_ERROR);

				rect = xv_alloc(Rect);

#ifdef SVR4
				/* This will probably not work right, but it compiles. */
				/* (rectNode->rect) is of the wrong type. */
				memmove(rect, &(rectNode->rect), sizeof(Rect));
#else
				memcpy((char *)rect, (char *)&rectNode->rect, sizeof(Rect));
#endif /* SVR4 */

				return ((Xv_opaque) rect);
			}
			/* NOTREACHED */
			break;
		case Dnd_Get_Window_Ptr:
			{
				register Dnd_window_list *winNode = site->region.windows;
				register Xv_Window *windows;
				register int i;

				if (!site->region.windows)
					return (XV_ERROR);

				/* One extra window for NULL entry */
				windows =
						xv_alloc_n(Xv_Window, (size_t)(site->num_regions + 1));

				for (i = 0; i < site->num_regions; i++) {
					winNode = (Dnd_window_list *) (XV_SL_SAFE_NEXT(winNode));
					assert(winNode != NULL);
					windows[i] = winNode->window;
				}
				windows[site->num_regions] = (Xv_Window) NULL;
				return ((Xv_opaque) windows);
			}
			/* NOTREACHED */
			break;
		case Dnd_Get_Rect_Ptr:
			{
				register Dnd_rect_list *rectNode = site->region.rects;
				register Rect *rects;
				register int i;

				if (!site->region.rects)
					return (XV_ERROR);

				/* One extra window for NULL entry */
				rects = xv_alloc_n(Rect, (size_t)(site->num_regions + 1));

				for (i = 0; i < site->num_regions; i++) {
					rectNode = (Dnd_rect_list *) (XV_SL_SAFE_NEXT(rectNode));
					assert(rectNode != NULL);
					rects[i] = rectNode->rect;
				}
				rects[site->num_regions].r_width = 0;
				rects[site->num_regions].r_height = 0;

				return ((Xv_opaque) rects);
			}
			/* NOTREACHED */
			break;
		case Dnd_Delete_All_Windows:
			{
				register Dnd_window_list *winNode = site->region.windows;

				if (!site->region.windows)
					return (XV_ERROR);

				while ((winNode =
								(Dnd_window_list *) (XV_SL_SAFE_NEXT(winNode))))
					xv_free(XV_SL_REMOVE_AFTER(site->region.windows,
									site->region.windows));

				xv_free(site->region.windows);
				site->region.windows = NULL;

				site->num_regions = 0;
			}
			break;
		case Dnd_Delete_All_Rects:
			{
				register Dnd_rect_list *rectNode = site->region.rects;

				if (!site->region.rects)
					return (XV_ERROR);

				rectNode = (Dnd_rect_list *) (XV_SL_SAFE_NEXT(rectNode));
				while (rectNode) {
					Dnd_rect_list *rectNodeNext =
							(Dnd_rect_list *) (XV_SL_SAFE_NEXT(rectNode));

					xv_free(XV_SL_REMOVE_AFTER(site->region.rects,
									site->region.rects));
					rectNode = rectNodeNext;
				}

				xv_free(site->region.rects);
				site->region.rects = NULL;

				site->num_regions = 0;
			}
			break;
		default:
			return (XV_ERROR);
	}
	return (XV_OK);
}

static void DndSizeOfSite(register Dnd_site_info *site)
{
    site->site_size = 3;             /* Window + site id + flags */

    if (dnds_status(site, is_window_region))
        site->site_size += 2 + site->num_regions;
    else
        site->site_size += 2 + 4 * site->num_regions;
}

Xv_private int DndStoreSiteData(Xv_drop_site site_public, long **prop)
{
    register Dnd_site_info 	*site = DND_SITE_PRIVATE(site_public);
    register Dnd_window_list 	*windows;
    register Dnd_rect_list   	*rects;
    register int	         i;
    register long		*data;

    data = *prop;
		/* If the site has no regions, then we don't update the
		 * interest property with this site.
		 */
    if (!site->num_regions)
	return(0);

    *data++ = site->owner_xid;
    *data++ = site->site_id;

	/* Reference (erfihlwebfrygjbv) : */
    *data++ = (site->event_mask | DND_EXPECT_NEW_PREVIEW_EVENT);

    if (dnds_status(site, is_window_region)) {
	*data++ = DND_WINDOW_SITE;
	*data++ = site->num_regions;
	for (i = 0,
	     windows=(Dnd_window_list *)(XV_SL_SAFE_NEXT(site->region.windows));
	     i < site->num_regions;
	     i++, windows = (Dnd_window_list *) (XV_SL_SAFE_NEXT(windows))) {

	     *data++ = (Window)xv_get(windows->window, XV_XID);
	}
    } else {
	*data++ = DND_RECT_SITE;
	*data++ = site->num_regions;
	for (i = 0,
	     rects = (Dnd_rect_list *)(XV_SL_SAFE_NEXT(site->region.rects));
	     i < site->num_regions;
	     i++, rects = (Dnd_rect_list *) (XV_SL_SAFE_NEXT(rects))) {

	     *data++ = rects->real_x;
	     *data++ = rects->real_y;
	     *data++ = (unsigned)rects->rect.r_width;
	     *data++ = (unsigned)rects->rect.r_height;
	}
    }
    *prop = data;
    return(1);
}

#ifdef NO_XDND
#else /* NO_XDND */

Xv_private int DndSiteContains(Xv_drop_site site_public, int fx, int fy)
{
	Dnd_site_info *site = DND_SITE_PRIVATE(site_public);
    Dnd_rect_list *rects;
	int i;

	if (!site->num_regions)
		return FALSE;

	for (i = 0, rects = (Dnd_rect_list *) (XV_SL_SAFE_NEXT(site->region.rects));
		i < site->num_regions;
		i++, rects = (Dnd_rect_list *) (XV_SL_SAFE_NEXT(rects)))
	{
		Rect rect;

		rect = rects->rect;
		rect.r_left = rects->real_x;
		rect.r_top = rects->real_y;


		if (rect_includespoint(&rect, fx, fy)) {
			return TRUE;
		}
	}
	return FALSE;
}

#endif /* NO_XDND */

#define ADONE ATTR_CONSUME(*attrs);break

static int dnd_site_init(Xv_Window owner, Xv_drop_site site_public,
								Attr_avlist avlist, int *u)
{
	Dnd_site_info *site = NULL;
	Xv_dropsite *site_object;

	site = xv_alloc(Dnd_site_info);
	site->public_self = site_public;
	site_object = (Xv_dropsite *) site_public;
	site_object->private_data = (Xv_opaque) site;

	dnds_status_reset(site, site_id_set);
	dnds_status_reset(site, window_set);
	dnds_status_reset(site, created);

#ifdef WINDOW_SITES
	dnds_status_set(site, is_window_region);
	dnds_status_reset(site, is_window_region);
#else
	dnds_status_reset(site, is_window_region);
#endif /* WINDOW_SITES */

	site->owner = owner;
	site->owner_xid = (Window) xv_get(owner, XV_XID);
	site->region.windows = NULL;
	site->region.rects = NULL;
	site->num_regions = 0;
	site->site_size = 0;
	site->event_mask = 0;

	return (XV_OK);
}

static Xv_opaque dnd_site_set_avlist(Xv_drop_site site_public,
							Attr_attribute	avlist[])
{
	register Dnd_site_info *site = DND_SITE_PRIVATE(site_public);
	register Attr_avlist attrs;

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch (attrs[0]) {

#ifdef WINDOW_SITES
			case DROP_SITE_TYPE:
				if (DND_WINDOW_SITE == (int)attrs[1])
					dnds_status_set(site, is_window_region);
				else
					dnds_status_reset(site, is_window_region);
				ADONE;
#endif /* WINDOW_SITES */

			case DROP_SITE_ID:
				site->site_id = (long)attrs[1];
				dnds_status_set(site, site_id_set);
				ADONE;
			case DROP_SITE_DEFAULT:
				if ((int)attrs[1])
					site->event_mask |= DND_DEFAULT_SITE;
				else
					site->event_mask ^= DND_DEFAULT_SITE;
				ADONE;
			case DROP_SITE_EVENT_MASK:
				site->event_mask &= DND_DEFAULT_SITE;
				site->event_mask |= (int)attrs[1];
				ADONE;
			case DROP_SITE_REGION:
				if (dnds_status(site, is_window_region))
					(void)DndDropAreaOps(site, Dnd_Add_Window, attrs[1]);
				else
					(void)DndDropAreaOps(site, Dnd_Add_Rect, attrs[1]);
				dnds_status_set(site, window_set);
				ADONE;
			case DROP_SITE_DELETE_REGION:
				if (!attrs[1])
					(void)DndDropAreaOps(site, (dnds_status(site,
											is_window_region) ?
									Dnd_Delete_All_Windows :
									Dnd_Delete_All_Rects), attrs[1]);
				else
					(void)DndDropAreaOps(site, (dnds_status(site,
											is_window_region) ?
									Dnd_Delete_Window : Dnd_Delete_Rect),
							attrs[1]);
				ADONE;
			case DROP_SITE_REGION_PTR:
				if (dnds_status(site, is_window_region))
					(void)DndDropAreaOps(site, Dnd_Add_Window_Ptr, attrs[1]);
				else
					(void)DndDropAreaOps(site, Dnd_Add_Rect_Ptr, attrs[1]);
				dnds_status_set(site, window_set);
				ADONE;
			case DROP_SITE_DELETE_REGION_PTR:
				if (!attrs[1])
					(void)DndDropAreaOps(site, (dnds_status(site,
											is_window_region) ?
									Dnd_Delete_All_Windows :
									Dnd_Delete_All_Rects), attrs[1]);
				else
					(void)DndDropAreaOps(site, (dnds_status(site,
											is_window_region) ?
									Dnd_Delete_Window_Ptr :
									Dnd_Delete_Rect_Ptr), attrs[1]);
				ADONE;
			case XV_END_CREATE:{
					if (!dnds_status(site, site_id_set))
						site->site_id = xv_unique_key();

#ifdef WINDOW_SITES
					if (!dnds_status(site, window_set)
							&& dnds_status(site, is_window_region))
						(void)DndDropAreaOps(site, Dnd_Add_Window, site->owner);
#endif /* WIDNOW_SITES */

					dnds_status_set(site, created);
					xv_set(site->owner, WIN_ADD_DROP_ITEM,
							DND_SITE_PUBLIC(site), NULL);
				}
				break;
			default:
				(void)xv_check_bad_attr(&xv_drop_site_item, attrs[0]);
				break;
		}
	}

	/* When ever some attribute of the drop site changes, we update the
	 * intrest property.
	 */
	if (dnds_status(site, created))
		(void)DndSizeOfSite(site);
	if (dnds_status(site, created) && xv_get(site->owner, XV_SHOW)) {
		xv_set(win_get_top_level(site->owner),
				WIN_ADD_DROP_INTEREST, DND_SITE_PUBLIC(site), NULL);
	}

	return ((Xv_opaque) XV_OK);
}

static Xv_opaque dnd_site_get_attr(Xv_drop_site site_public, int *error,
									Attr_attribute attr, va_list args)
{
	Dnd_site_info *site = DND_SITE_PRIVATE(site_public);
	Xv_opaque value;

	switch (attr) {
#ifdef WINDOW_SITES
		case DROP_SITE_TYPE:
			if (dnds_status(site, is_window_region))
				value = (Xv_opaque) DND_WINDOW_SITE;
			else
				value = (Xv_opaque) DND_RECT_SITE;
			break;
#endif /* WINDOW_SITES */

		case DROP_SITE_SIZE:
			value = (Xv_opaque) site->site_size;
			break;
		case DROP_SITE_ID:
			value = (Xv_opaque) site->site_id;
			break;
		case DROP_SITE_DEFAULT:
			value = (Xv_opaque) ((site->event_mask & DND_DEFAULT_SITE) ?
					TRUE : FALSE);
			break;
		case DROP_SITE_EVENT_MASK:
			value = (Xv_opaque) (site->event_mask ^ DND_DEFAULT_SITE);
			break;
		case DROP_SITE_REGION:
			if (dnds_status(site, is_window_region))
				value = (Xv_opaque) DndDropAreaOps(site, Dnd_Get_Window,
						XV_NULL);
			else
				value = (Xv_opaque) DndDropAreaOps(site, Dnd_Get_Rect, XV_NULL);
			if (value == XV_ERROR)
				*error = XV_ERROR;
			break;
		case DROP_SITE_REGION_PTR:
			if (dnds_status(site, is_window_region))
				value = (Xv_opaque) DndDropAreaOps(site, Dnd_Get_Window_Ptr,
						XV_NULL);
			else
				value = (Xv_opaque) DndDropAreaOps(site, Dnd_Get_Rect_Ptr,
						XV_NULL);
			if (value == XV_ERROR)
				*error = XV_ERROR;
			break;
		default:
			if (xv_check_bad_attr(&xv_drop_site_item, attr) == XV_ERROR)
				*error = XV_ERROR;
			value = XV_NULL;
			break;
	}

	return (value);
}

static int dnd_site_destroy(Xv_drop_site site_public, Destroy_status state)
{
	if (state == DESTROY_CLEANUP) {
		Dnd_site_info *site = DND_SITE_PRIVATE(site_public);

		xv_set(site->owner,
					WIN_DELETE_DROP_ITEM, site_public,
					NULL);
		xv_set(win_get_top_level(site->owner),
					WIN_DELETE_DROP_INTEREST, site_public,
					NULL);
		if (dnds_status(site, is_window_region))
			(void)DndDropAreaOps(site, Dnd_Delete_All_Windows, XV_NULL);
		else
			(void)DndDropAreaOps(site, Dnd_Delete_All_Rects, XV_NULL);
		xv_free(site);
	}

	return (XV_OK);
}

const Xv_pkg		xv_drop_site_item = {
    "DropSite", ATTR_PKG_DND,
    sizeof(Xv_dropsite),
    XV_GENERIC_OBJECT,
    dnd_site_init,
    dnd_site_set_avlist,
    dnd_site_get_attr,
    dnd_site_destroy,
    NULL
};
