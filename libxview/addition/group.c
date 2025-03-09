/*
 * This file is a product of Sun Microsystems, Inc. and is provided for
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
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043

char group_c_sccsid[] = "@(#)group.c 2.28 92/08/03 Copyright 1991 Sun Microsystems RCS: $Id: group.c,v 4.2 2025/03/08 13:37:48 dra Exp $";
 */

/*
 * Routines for relative layout support through groups
 */

#include <xview/group.h>

char group_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: group.c,v 4.2 2025/03/08 13:37:48 dra Exp $";

typedef struct {
	Xv_object public_self;
	GROUP_TYPES group_type;
	Xv_opaque *members;
	int cols;
	GROUP_COLUMN_ALIGNMENTS col_alignment;
	int rows;
	GROUP_ROW_ALIGNMENTS row_alignment;
	int hspacing;
	int vspacing;
	Xv_opaque anchor_obj; /* Object anchored to */
	GROUP_COMPASS_POINTS anchor_point; /* Point on anchor obj */
	GROUP_COMPASS_POINTS reference_point; /* Point on group */
	int hoffset;
	int voffset;
	Rect group_rect;
	Rect value_rect;
	int initial_x;
	int initial_y;
	unsigned int flags;
} Group_private;

#define GROUP_PUBLIC(item) XV_PUBLIC(item)
#define GROUP_PRIVATE(item) XV_PRIVATE(Group_private, Group_public, item)
#define ADONE ATTR_CONSUME(*attrs);break

typedef enum {
        CREATED = (1L << 0),
        LAYOUT = (1L << 1),
        INACTIVE = (1L << 2),
        SHOWING = (1L << 3),
        ROWFIRST = (1L << 4),
        COLFIRST = (1L << 5)
} GROUP_FLAGS;


/*
 * Allocate space for an array of Xv_opaques to store new members.
 */
static void group_set_members(Group_private *priv, Xv_opaque *members)
{
	int i;
	int num;
	Xv_opaque *xvptr = members;

	/*
	 * Free up space from old members list
	 */
	if (priv->members)
		xv_free(priv->members);

	if (!members)
	{
		priv->members = NULL;
		return;
	}

	/*
	 * Count number of new members
	 */
	for (num = 0; *xvptr++; num++)
		;

	/*
	 * Allocate space for new members list.  Should use xv_alloc_n
	 * here, but it was incorrect in V2 and we want this package
	 * to stay portable back to V2.
	 */
	priv->members = (Xv_opaque *)calloc((size_t)num+1, sizeof(Xv_opaque));

	/*
	 * Walk through list and store members, mark each
	 * new member in this group.
	 */
	for (i = 0; i < num; i++)
	{
		priv->members[i] = members[i];
		xv_set(priv->members[i],
			XV_KEY_DATA, GROUP_PARENT, GROUP_PUBLIC(priv), NULL);
	}
}

/*
 * Move a group to a new (absolute) x/y position
 */
static void group_set_xy(Group_private *priv, int x, int y)
{
	int i;
	Xv_opaque cur;

	if (!priv->members)
		return;

	for (i = 0; priv->members[i]; i++)
	{
		cur = priv->members[i];
		xv_set(cur, XV_X, x + (xv_get(cur, XV_X) - priv->group_rect.r_left),
			    XV_Y, y + (xv_get(cur, XV_Y) - priv->group_rect.r_top),
			    NULL);
	}

	priv->group_rect.r_left = x;
	priv->group_rect.r_top = y;

	if (priv->members) {
		priv->value_rect.r_left =
			xv_get(priv->members[0], PANEL_VALUE_X);
		priv->value_rect.r_top =
			xv_get(priv->members[0], PANEL_VALUE_X);
	}
	else
	{
		priv->value_rect.r_left = x;
		priv->value_rect.r_top = y;
	}
}

/*
 * Calculate the x/y locatiions for a compass point on an object
 */
static void get_compass_point(Xv_opaque handle, GROUP_COMPASS_POINTS point, int *x, int *y)
{
	switch (point)
	{
	case GROUP_NORTHWEST:
	case GROUP_WEST:
	case GROUP_SOUTHWEST:
		*x = xv_get(handle, XV_X);
		break;
	case GROUP_NORTH:
	case GROUP_CENTER:
	case GROUP_SOUTH:
		*x = xv_get(handle, XV_X) + xv_get(handle, XV_WIDTH)/2;
		break;
	case GROUP_NORTHEAST:
	case GROUP_EAST:
	case GROUP_SOUTHEAST:
		*x = xv_get(handle, XV_X) + xv_get(handle, XV_WIDTH);
		break;
	}

	switch (point)
	{
	case GROUP_NORTHWEST:
	case GROUP_NORTH:
	case GROUP_NORTHEAST:
		*y = xv_get(handle, XV_Y);
		break;
	case GROUP_WEST:
	case GROUP_CENTER:
	case GROUP_EAST:
		*y = xv_get(handle, XV_Y) + xv_get(handle, XV_HEIGHT)/2;
		break;
	case GROUP_SOUTHWEST:
	case GROUP_SOUTH:
	case GROUP_SOUTHEAST:
		*y = xv_get(handle, XV_Y) + xv_get(handle, XV_HEIGHT);
		break;
	}
}

/*
 * Anchor a group
 */
void group_anchor(Group group_public)
{
	int new_x;
	int new_y;
	int anchor_x;
	int anchor_y;
	int ref_x;
	int ref_y;
	Group_private *priv;

	if (!group_public)
		return;

	priv = GROUP_PRIVATE(group_public);

	if (!priv || !priv->anchor_obj)
		return;

	get_compass_point(priv->anchor_obj, priv->anchor_point,
			  &anchor_x, &anchor_y);
	get_compass_point(group_public, priv->reference_point,
			  &ref_x, &ref_y);

	if (xv_get(priv->anchor_obj, XV_OWNER) !=
	    xv_get(group_public, XV_OWNER)) {
		anchor_x -= (int)xv_get(priv->anchor_obj, XV_X);
		anchor_y -= (int)xv_get(priv->anchor_obj, XV_Y);
	}

	new_x = anchor_x + (priv->group_rect.r_left - ref_x) +
		priv->hoffset;
	new_y = anchor_y + (priv->group_rect.r_top - ref_y) +
		priv->voffset;

	group_set_xy(priv, new_x, new_y);
}

/*
 * Layout an as-is group.  Usually means do nothing, check to see
 * if members are anchored first though.
 */
static void layout_none(Group_private *priv)
{
	int i;
	Xv_opaque cur;

	for (i = 0; priv->members[i]; i++)
	{
		cur = priv->members[i];

		/*
		 * Check to see if this member is a group, if so
		 * lay it out so it will be anchored correctly.
		 */
		if (xv_get(cur, XV_IS_SUBTYPE_OF, GROUP)) group_anchor((Group)cur);
	}
}

/*
 * Layout a row group
 */
static void layout_row(Group_private *priv)
{
	int i;
	int base_y = 0;
	int new_y = 0;
	Xv_opaque cur;
	Xv_opaque prev;

	switch (priv->row_alignment)
	{
	case GROUP_TOP_EDGES:
		base_y = xv_get(priv->members[0], XV_Y);
		break;

	case GROUP_HORIZONTAL_CENTERS:
		base_y = xv_get(priv->members[0], XV_Y) + 
			xv_get(priv->members[0], XV_HEIGHT)/2;
		break;

	case GROUP_BOTTOM_EDGES:
		base_y = xv_get(priv->members[0], XV_Y) + 
			xv_get(priv->members[0], XV_HEIGHT);
		break;
	}

	for (i = 1; priv->members[i]; i++)
	{
		cur = priv->members[i];
		prev = priv->members[i-1];

		switch (priv->row_alignment)
		{
		case GROUP_TOP_EDGES:
			new_y = base_y;
			break;

		case GROUP_HORIZONTAL_CENTERS:
			new_y = base_y - xv_get(cur, XV_HEIGHT)/2;
			break;

		case GROUP_BOTTOM_EDGES:
			new_y = base_y - xv_get(cur, XV_HEIGHT);
			break;
		}

		xv_set(cur, XV_X, xv_get(prev, XV_X) +
				  xv_get(prev, XV_WIDTH) +
				  priv->hspacing,
			    XV_Y, new_y,
			    NULL);
	}
}

/*
 * Layout a column group
 */
static void layout_col(Group_private *priv)
{
	int i;
	int new_x = 0;
	int base_x = 0;
	Xv_opaque cur;
	Xv_opaque prev;

	switch (priv->col_alignment)
	{
	case GROUP_LEFT_EDGES:
		base_x = xv_get(priv->members[0], XV_X);
		break;

	case GROUP_LABELS:
		base_x = xv_get(priv->members[0], PANEL_VALUE_X);
		break;

	case GROUP_VERTICAL_CENTERS:
		base_x = xv_get(priv->members[0], XV_X) + 
			xv_get(priv->members[0], XV_WIDTH)/2;
		break;

	case GROUP_RIGHT_EDGES:
		base_x = xv_get(priv->members[0], XV_X) + 
			xv_get(priv->members[0], XV_WIDTH);
		break;
	}

	for (i = 1; priv->members[i]; i++)
	{
		cur = priv->members[i];
		prev = priv->members[i-1];

		switch (priv->col_alignment)
		{
		case GROUP_LEFT_EDGES:
		case GROUP_LABELS:
			new_x = base_x;
			break;

		case GROUP_VERTICAL_CENTERS:
			new_x = base_x - xv_get(cur, XV_WIDTH)/2;
			break;

		case GROUP_RIGHT_EDGES:
			new_x = base_x - xv_get(cur, XV_WIDTH);
			break;
		}

		if (priv->col_alignment == GROUP_LABELS)
		{
			xv_set(cur, PANEL_VALUE_X, new_x,
			       XV_Y, xv_get(prev, XV_Y) +
			       xv_get(prev, XV_HEIGHT) +
			       priv->vspacing,
			       NULL);
		}
		else
		{
			xv_set(cur, XV_X, new_x,
			       XV_Y, xv_get(prev, XV_Y) +
			       xv_get(prev, XV_HEIGHT) +
			       priv->vspacing,
			       NULL);
		}
	}
}

static void get_rowcol_info(Group_private *priv, int *cell_width, int *cell_height)
{
	int i;
	int lw;
	int vw;
	int max_lw = -1;
	int max_vw = -1;
	int count;
	Xv_opaque cur;

	/*
	 * Calculate rows and column based on number of members
	 * and current fill order.
	 */
	if ((priv->rows == 0) && (priv->cols == 0))
		priv->rows = 1;

	for (count = 0; priv->members[count]; count++)
		;

	if (priv->flags & ROWFIRST) {
		priv->cols = count / priv->rows;

		if (count % priv->rows)
			priv->cols++;
	} else {
		priv->rows = count  / priv->cols;

		if (count % priv->cols)
			priv->rows++;
	}

	/*
	 * Walk through the list, find maximum cell size.  Row/Col
	 * groups aligned on labels are special.  The widest item may
	 * not determine the cell size.  It is determined by the
	 * widest label plus the widest value field, phew.
	 */
	*cell_width = -1;
	*cell_height = -1;

	if ((priv->group_type == GROUP_ROWCOLUMN) &&
	    (priv->col_alignment == GROUP_LABELS)) {
		for (i = 0; priv->members[i]; i++) {
			cur = priv->members[i];

			lw = xv_get(cur, PANEL_VALUE_X) - xv_get(cur, XV_X);
			vw = xv_get(cur, XV_WIDTH) - lw;

			if (lw > max_lw)
				max_lw = lw;
			if (vw > max_vw)
				max_vw = vw;
			if ((int)xv_get(cur, XV_HEIGHT) > *cell_height)
				*cell_height = xv_get(cur, XV_HEIGHT);
		}

		*cell_width = max_lw + max_vw;
	}
	else
	{
		for (i = 0; priv->members[i]; i++)
		{
			cur = priv->members[i];

			if ((int)xv_get(cur, XV_WIDTH) > *cell_width)
				*cell_width = xv_get(cur, XV_WIDTH);
			if ((int)xv_get(cur, XV_HEIGHT) > *cell_height)
				*cell_height = xv_get(cur, XV_HEIGHT);
		}
	}
}

static int get_value_x(Group_private *priv, int col)
{
	int i;
	int j;
	int tmp;
	int start;
	int incr;
	int vx = -1;

	/*
	 * Walk through the list, find maximum cell size
	 */
	if (priv->flags & ROWFIRST)
	{
		start = col;
		incr = priv->cols;
	}
	else
	{
		start = col * priv->rows;
		incr = 1;
	}

	for (j = 0, i = start;
	     priv->members[i] && j < priv->rows;
	     i += incr, j++) {
		tmp = xv_get(priv->members[i], PANEL_VALUE_X) -
		      xv_get(priv->members[i], XV_X);

		if (tmp > vx)
			vx = tmp;
	}

	return vx;
}

/*
 * Place an object correctly inside a cell in a row/col group
 */
static void place_cell(Group_private *priv, int i, int cell_width, int cell_height, int vx, int row, int col)
{
	int x = 0;
	int y = 0;
	int cell_x;
	int cell_y;
	Xv_opaque cur = priv->members[i];

	/*
	 * Calculate the upper left corner for this cell
	 */
	cell_x = priv->group_rect.r_left +
		col * (cell_width + priv->hspacing);
	cell_y = priv->group_rect.r_top +
		row * (cell_height + priv->vspacing);

	switch (priv->col_alignment)
	{
	case GROUP_LEFT_EDGES:
		x = cell_x;
		break;
	case GROUP_LABELS:
		x = xv_get(cur, XV_X) +
			((cell_x + vx) - xv_get(cur, PANEL_VALUE_X));
		break;
	case GROUP_VERTICAL_CENTERS:
		x = (cell_x + cell_width/2) - xv_get(cur, XV_WIDTH)/2;
		break;
	case GROUP_RIGHT_EDGES:
		x = (cell_x + cell_width) - xv_get(cur, XV_WIDTH);
		break;
	}

	switch (priv->row_alignment)
	{
	case GROUP_TOP_EDGES:
		y = cell_y;
		break;
	case GROUP_HORIZONTAL_CENTERS:
		y = (cell_y + cell_height/2) - xv_get(cur, XV_HEIGHT)/2;
		break;
	case GROUP_BOTTOM_EDGES:
		y = (cell_y + cell_height) - xv_get(cur, XV_HEIGHT);
		break;
	}

	xv_set(cur, XV_X, x, XV_Y, y, NULL);
}

/*
 * Layout a row/column group
 */
static void layout_rowcol(Group_private *priv)
{
	int i;
	int vx = 0;
	int current_row = 0;
	int current_col = 0;
	int cell_width;
	int cell_height;
	Xv_opaque *members = priv->members;

	get_rowcol_info(priv, &cell_width, &cell_height);

	if (priv->col_alignment == GROUP_LABELS)
		vx = get_value_x(priv, 0);

	/*
	 * Walk through the list, place each object in it's "cell". 
	 */
	for (i = 0; members[i]; i++) {
		place_cell(priv, i, cell_width, cell_height,
			   vx, current_row, current_col);

		if (priv->flags & ROWFIRST) {
			if (++current_col >= priv->cols) {
				current_row++;
				current_col = 0;
			}
			if ((priv->col_alignment == GROUP_LABELS) &&
			    members[i+1])
				vx = get_value_x(priv, current_col);
		} else {
			if (++current_row >= priv->rows) {
				current_col++;
				current_row = 0;
				if ((priv->col_alignment == GROUP_LABELS) &&
				    members[i+1])
					vx = get_value_x(priv, current_col);
			}
		}
	}
}

/*
 * Return the bounding rectangle for a group
 */
static Rect *get_rect_for_group(Group_private *priv)
{
	int i;
	int cell_width;
	int cell_height;
	static Rect bbox;
	Rect r;
	Rect *r1;

	r = rect_null;

	if (!priv->members)
	{
		bbox = r;
		return &bbox;
	}

	if (priv->group_type == GROUP_ROWCOLUMN)
	{
		get_rowcol_info(priv, &cell_width, &cell_height);
		r.r_left = priv->group_rect.r_left;
		r.r_top = priv->group_rect.r_top;
		r.r_width = (priv->cols * cell_width) +
			((priv->cols-1) * priv->hspacing);
		r.r_height = (priv->rows * cell_height) +
			((priv->rows-1) * priv->vspacing);
	}
	else
	{
		for (i = 0; priv->members[i]; i++)
		{
			if ((r1 = (Rect *)xv_get(priv->members[i], XV_RECT)))
				r = rect_bounding(&r, r1);
		}
	}

	bbox = r;
	return &bbox;
}

/*
 * Return a pointer to a parent group, NULL if none
 */
static Group_public *get_parent_group(Group_private *priv)
{
	return (Group_public *)xv_get(GROUP_PUBLIC(priv),
				      XV_KEY_DATA, GROUP_PARENT);
}

static int group_init(Xv_opaque owner, Xv_opaque slf, Attr_avlist avlist,int *u)
{
	Group_public *group_public = (Group_public *)slf;
	Group_private *priv = xv_alloc(Group_private);

	if (!priv)
		return XV_ERROR;

	group_public->private_data = (Xv_opaque)priv;
	priv->public_self = (Xv_opaque)group_public;

	/*
	 * Initialize the defaults for certain values
	 */
	priv->group_type = GROUP_NONE;
	priv->hspacing = 10;
	priv->vspacing = 10;
	priv->row_alignment = GROUP_HORIZONTAL_CENTERS;
	priv->col_alignment = GROUP_LABELS;
	priv->reference_point = GROUP_NORTHWEST;
	priv->hoffset = 10;
	priv->voffset = 10;
	priv->flags |= (SHOWING|LAYOUT);

	return XV_OK;
}

static Xv_opaque group_set(Group group_public, Attr_avlist avlist)
{
	int i;
	int x = 0;
	int x_changed = FALSE;
	int y = 0;
	int y_changed = FALSE;
	int replaced;
	int need_layout = FALSE;
	int initial_position = FALSE;
	Xv_opaque old, new;
	Attr_attribute *attrs;
	Group_private *priv = GROUP_PRIVATE(group_public);

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) {
		switch ((int) attrs[0]) {
		case XV_X:
			x = (int)attrs[1];
			x_changed = TRUE;
			if (!(priv->flags & CREATED)) {
				priv->initial_x = x;
				priv->group_rect.r_left = x;
			}
			ADONE;

		case XV_Y:
			y = (int)attrs[1];
			y_changed = TRUE;
			if (!(priv->flags & CREATED)) {
				priv->initial_y = y;
				priv->group_rect.r_top = y;
			}
			ADONE;

		case PANEL_VALUE_X:
			x = priv->group_rect.r_left + 
			    ((int)attrs[1] - priv->value_rect.r_left);
			x_changed = TRUE;
			ADONE;
			
		case XV_SHOW:
			for (i = 0; priv->members[i]; i++) {
				xv_set(priv->members[i],
					XV_SHOW, attrs[1], NULL);
			}
			if ((int)attrs[1])
				priv->flags |= SHOWING;
			else
				priv->flags &= ~SHOWING;
			ADONE;

		case PANEL_INACTIVE:
			for (i = 0; priv->members[i]; i++) {
				xv_set(priv->members[i],
					PANEL_INACTIVE, (int)attrs[1], NULL);
			}
			if ((int)attrs[1])
				priv->flags |= INACTIVE;
			else
				priv->flags &= ~INACTIVE;
			ADONE;

		case GROUP_TYPE:
			need_layout = TRUE;
			priv->group_type = (GROUP_TYPES)attrs[1];
			ADONE;

		case GROUP_ROWS:
			need_layout = TRUE;
			if ((priv->rows = (int)attrs[1]) > 0) {
				priv->flags |= ROWFIRST;
				priv->flags &= ~COLFIRST;
			}
			ADONE;

		case GROUP_COLUMNS:
			need_layout = TRUE;
			if ((priv->cols = (int)attrs[1]) > 0) {
				priv->flags |= COLFIRST;
				priv->flags &= ~ROWFIRST;
			}
			ADONE;

		case GROUP_HORIZONTAL_SPACING:
			need_layout = TRUE;
			priv->hspacing = (int)attrs[1];
			ADONE;

		case GROUP_VERTICAL_SPACING:
			need_layout = TRUE;
			priv->vspacing = (int)attrs[1];
			ADONE;

		case GROUP_ROW_ALIGNMENT:
			need_layout = TRUE;
			priv->row_alignment = (GROUP_ROW_ALIGNMENTS)attrs[1];
			ADONE;

		case GROUP_COLUMN_ALIGNMENT:
			need_layout = TRUE;
			priv->col_alignment = (GROUP_COLUMN_ALIGNMENTS)attrs[1];
			ADONE;

		case GROUP_REPLACE_MEMBER:
			old = (Xv_opaque)attrs[1];
			new = (Xv_opaque)attrs[2];

			replaced = FALSE;

			for (i = 0; priv->members[i]; i++) {
				if (priv->members[i] == old) {
					replaced = TRUE;
					need_layout = TRUE;
					priv->members[i] = new;
				}
			}
			
			if (!replaced)
				xv_error((Group)group_public,
				    ERROR_STRING, "member not found, not replaced",
				    ERROR_PKG,    GROUP,
				    NULL);

			ADONE;

		case GROUP_MEMBERS:
			group_set_members(priv, &attrs[1]);
			need_layout = TRUE;
			ADONE;

		case GROUP_MEMBERS_PTR:
			group_set_members(priv, (Xv_opaque *)attrs[1]);
			need_layout = TRUE;
			ADONE;

		case GROUP_LAYOUT:
			if ((int)attrs[1]) {
				need_layout = TRUE;
				priv->flags |= LAYOUT;
			} else
				priv->flags &= ~LAYOUT;
			ADONE;

		case GROUP_ANCHOR_OBJ:
			need_layout = TRUE;
			priv->anchor_obj = (Xv_opaque)attrs[1];
			ADONE;

		case GROUP_ANCHOR_POINT:
			need_layout = TRUE;
			priv->anchor_point = (GROUP_COMPASS_POINTS)attrs[1];
			ADONE;

		case GROUP_REFERENCE_POINT:
			need_layout = TRUE;
			priv->reference_point = (GROUP_COMPASS_POINTS)attrs[1];
			ADONE;

		case GROUP_HORIZONTAL_OFFSET:
			need_layout = TRUE;
			priv->hoffset = (int)attrs[1];
			ADONE;

		case GROUP_VERTICAL_OFFSET:
			need_layout = TRUE;
			priv->voffset = (int)attrs[1];
			ADONE;

		case XV_END_CREATE:
			need_layout = TRUE;
			initial_position = TRUE;
			priv->flags |= CREATED;
			break;

		default:
			xv_check_bad_attr(GROUP, attrs[0]);
			break;
		}
	}

	/*
	 * Don't go any further until object has been created
	 */
	if (!(priv->flags & CREATED))
		return (Xv_opaque)XV_OK;

	/*
	 * If something changed that would change the layout of
	 * the group, percolate!
	 */
	if (need_layout)
		group_layout((Group)group_public);

	/*
	 * Check to see if x or y were set, if so we need to move the group.
	 */
	if (x_changed || y_changed) {
		if (!x_changed)
			x = priv->group_rect.r_left;
		if (!y_changed)
			y = priv->group_rect.r_top;

		group_set_xy(priv, x, y);
	}
	
	/*
	 * If this is the first time this group has been positioned,
	 * make sure it is placed correctly if it is not anchored.  If
	 * it is anchored it was placed correctly earlier.
	 */
	if (initial_position && !priv->anchor_obj) {
		group_set_xy(priv, priv->initial_x, priv->initial_y);
	}

	return (Xv_opaque)XV_OK;
}

static Xv_opaque group_get(Group group_public, int *status, Attr_attribute attr, va_list vali)
{
	Group_private *priv = GROUP_PRIVATE(group_public);

	switch ((int) attr) {
		case XV_X:
			return (Xv_opaque)priv->group_rect.r_left;

		case PANEL_VALUE_X:
			return (Xv_opaque)priv->value_rect.r_left;

		case XV_Y:
			return (Xv_opaque)priv->group_rect.r_top;

		case PANEL_VALUE_Y:
			return (Xv_opaque)priv->value_rect.r_top;

		case XV_WIDTH:
			return (Xv_opaque)priv->group_rect.r_width;

		case XV_HEIGHT:
			return (Xv_opaque)priv->group_rect.r_height;

		case XV_RECT:
			return (Xv_opaque)&(priv->group_rect);

		case XV_SHOW:
			return (Xv_opaque)(priv->flags & SHOWING);

		case PANEL_INACTIVE:
			return (Xv_opaque)(priv->flags & INACTIVE);

		case GROUP_TYPE:
			return (Xv_opaque)priv->group_type;

		case GROUP_ROWS:
			return (Xv_opaque)priv->rows;

		case GROUP_COLUMNS:
			return (Xv_opaque)priv->cols;

		case GROUP_HORIZONTAL_SPACING:
			return (Xv_opaque)priv->hspacing;

		case GROUP_VERTICAL_SPACING:
			return (Xv_opaque)priv->vspacing;

		case GROUP_ROW_ALIGNMENT:
			return (Xv_opaque)priv->row_alignment;

		case GROUP_COLUMN_ALIGNMENT:
			return (Xv_opaque)priv->col_alignment;

		case GROUP_MEMBERS:
			return (Xv_opaque)priv->members;

		case GROUP_LAYOUT:
			return (Xv_opaque)(priv->flags & LAYOUT);

		case GROUP_ANCHOR_OBJ:
			return (Xv_opaque)priv->anchor_obj;

		case GROUP_ANCHOR_POINT:
			return (Xv_opaque)priv->anchor_point;

		case GROUP_REFERENCE_POINT:
			return (Xv_opaque)priv->reference_point;

		case GROUP_HORIZONTAL_OFFSET:
			return (Xv_opaque)priv->hoffset;

		case GROUP_VERTICAL_OFFSET:
			return (Xv_opaque)priv->voffset;

		case GROUP_PARENT:
			return (Xv_opaque)get_parent_group(priv);

		default:
			if (xv_check_bad_attr(GROUP, attr) == XV_ERROR)
				*status = XV_ERROR;
			break;
	}

	return (Xv_opaque)XV_OK;
}

static int group_destroy(Group group_public, Destroy_status status)
{
	Group_private *priv = GROUP_PRIVATE(group_public);

	if (status == DESTROY_CLEANUP) {
		/*
		 * Mark all members free of this group, then free up
		 * space allocated for members list.
		 */
		if (priv->members) {
			/*
			 * MOOSE - we could only do this if we could tell
			 * whether the XView handle is still valid...
			 *
			for (i = 0; priv->members[i]; i++)
				xv_set(priv->members[i],
					XV_KEY_DATA, GROUP_PARENT, NULL, NULL);
			 */

			xv_free(priv->members);
		}

		xv_free(priv);
	}

	return XV_OK;
}

void group_layout(Group group_public)
{
	Rect *r;
	Group_public *parent;
	Group_private *priv;

	if (!group_public)
		return;

	priv = GROUP_PRIVATE(group_public);

	if (!priv || !priv->members)
		return;

	if (!(priv->flags & LAYOUT))
		return;

	switch (priv->group_type) {
		case GROUP_NONE:
			layout_none(priv);
			break;
		case GROUP_ROW:
			layout_row(priv);
			break;
		case GROUP_COLUMN:
			layout_col(priv);
			break;
		case GROUP_ROWCOLUMN:
			layout_rowcol(priv);
			break;
		default:
			break;
	}

	/*
	 * Update x/y/width/height for this group, values may have changed
	 */
	r = get_rect_for_group(priv);
	priv->group_rect.r_left = r->r_left;
	priv->group_rect.r_top = r->r_top;
	priv->group_rect.r_width = r->r_width;
	priv->group_rect.r_height = r->r_height;

	if (priv->members) {
		priv->value_rect.r_left = 
			(int)xv_get(priv->members[0], PANEL_VALUE_X);
		priv->value_rect.r_top = 
			(int)xv_get(priv->members[0], PANEL_VALUE_Y);
	} else {
		priv->value_rect.r_left = priv->group_rect.r_left;
		priv->value_rect.r_top = priv->group_rect.r_top;
	}

	/*
	 * If this group belongs to a parent group, we need to recurse
	 * upwards and layout the parent group now.
	 *
	 * Otherwise, if this group is anchored, place it relative to anchor
	 */
	if ((parent = get_parent_group(priv)))
		group_layout((Group)parent);
	else if (priv->anchor_obj)
		group_anchor((Group)group_public);
}

const Xv_pkg	group_pkg = {
	"Group",			/* Package name */
	ATTR_PKG_GROUP,		/* Package ID */
	sizeof(Group_public),		/* Size of public struct */
	XV_GENERIC_OBJECT,		/* Subclass of XV_GENERIC_OBJECT */
	group_init,
	group_set,
	group_get,
	group_destroy,
	NULL				/* No xv_find() */
};
