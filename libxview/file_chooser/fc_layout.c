#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fc_layout.c 1.15 93/06/28  DRA: RCS $Id: fc_layout.c,v 4.1 2024/03/28 13:04:11 dra Exp $ ";
#endif
#endif
 

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#include <stdio.h>
#include <xview/xview.h>
#include <xview/font.h>
#include <xview/panel.h>
#include <xview/scrollbar.h>
#include <xview_private/fchsr_impl.h>


/*
 * Substitute for xv_rows/xv_cols, since they may do xv_get
 * on FONT_DEFAULT_CHAR_WIDTH/HEIGHT on each call.
 */
#define COLS(num)	(private->col_width * (num))
#define ROWS(num)	((int)(private->row_height * (num)))


/*
 * Calculate and set X positions and widths
 */
static void fc_calc_xs(Fc_private *private, Rect *exten_rect)
{
	Rect *tmp;
	int pix_width;
	int x_pos;
	int value_x;
	int item_x;
	Scrollbar sb;
	int sb_width;

	pix_width = private->rect.r_width;


	/* initialize X values for extension rect */
	exten_rect->r_width = pix_width;
	exten_rect->r_left = 0;


	/*
	 * Goto button is left-aligned on the second column.
	 */
	xv_set(private->ui.goto_msg, XV_X, COLS(2), PANEL_PAINT, PANEL_NONE, NULL);

	xv_set(private->ui.goto_btn, XV_X, COLS(2), PANEL_PAINT, PANEL_NONE, NULL);


	/*
	 * Put Goto Textfield 1 Column after Goto Button.
	 * Use all remaining width, shy 2 columns.  Also,
	 * even without a label, it puts a bit of space 
	 * beteeen the label and value rects; so we have
	 * to account for it when calculating width.
	 */
	tmp = (Rect *) xv_get(private->ui.goto_btn, XV_RECT);
	x_pos = rect_right(tmp) + COLS(1);
	item_x = (int)xv_get(private->ui.goto_txt, XV_X);
	value_x = (int)xv_get(private->ui.goto_txt, PANEL_VALUE_X);
	xv_set(private->ui.goto_txt,
			XV_X, x_pos,
			PANEL_VALUE_DISPLAY_WIDTH,
			pix_width - x_pos - (value_x - item_x) - COLS(2),
			PANEL_PAINT, PANEL_NONE,
			NULL);



	/*
	 * Place Folder Textfield.
	 */
	x_pos = COLS(4);
	xv_set(private->ui.folder_txt,
			XV_X, x_pos,
			PANEL_VALUE_DISPLAY_WIDTH, pix_width - x_pos - COLS(2),
			PANEL_PAINT, PANEL_NONE,
			NULL);

	/*
	 * Place select message.  Should line up with File List
	 * BUG:  PANEL_MESSAGE seems to ignore the label and item
	 * widths...
	 */
	xv_set(private->ui.select_msg,
			XV_X, COLS(4),
			PANEL_LABEL_WIDTH, pix_width - (int)xv_get(private->ui.select_msg,
									PANEL_LABEL_X) - COLS(4),
			PANEL_PAINT, PANEL_NONE,
			NULL);

	/*
	 * Place File List centered, 4 cols on either side.  Note
	 * that PANEL_LIST_WIDTH does not inlcude scrollbar width.
	 */
	x_pos = COLS(4);
	sb = (Scrollbar) xv_get(private->ui.list, PANEL_LIST_SCROLLBAR);
	sb_width = (int)xv_get(sb, XV_WIDTH);
	xv_set(private->ui.list,
			XV_X, x_pos,
			PANEL_LIST_WIDTH, pix_width - x_pos - sb_width - COLS(4),
			PANEL_PAINT, PANEL_NONE,
			NULL);



	/*
	 * Place document left-aligned with Goto Button.
	 * Note:  there is no doc typein for Open dialog.
	 */
	if (private->ui.document_txt) {
		item_x = (int)xv_get(private->ui.document_txt, XV_X);
		value_x = (int)xv_get(private->ui.document_txt, PANEL_VALUE_X);
		x_pos = COLS(2);
		xv_set(private->ui.document_txt,
				XV_X, x_pos,
				PANEL_VALUE_DISPLAY_WIDTH, pix_width - x_pos
											- (value_x - item_x) - COLS(2),
				PANEL_PAINT, PANEL_NONE,
				NULL);
	}

	/*
	 * Center the Open, Cancel and Save buttons
	 */
	{
		int open_width = (int)xv_get(private->ui.open_btn, XV_WIDTH);
		int cancel_width = (int)xv_get(private->ui.cancel_btn, XV_WIDTH);
		int other_width = 0;
		Panel_item other_btn = XV_NULL;

		if (private->type != FILE_CHOOSER_OPEN)
			other_btn = private->ui.save_btn;
		else if (private->custom)
			other_btn = private->ui.custom_btn;

		if (other_btn)
			other_width = (int)xv_get(other_btn, XV_WIDTH) + COLS(2);

		x_pos = (pix_width - (open_width + cancel_width + other_width + COLS(2))
				) / 2;

		xv_set(private->ui.open_btn,
				XV_X, x_pos, PANEL_PAINT, PANEL_NONE, NULL);

		x_pos += open_width + COLS(2);
		xv_set(private->ui.cancel_btn,
				XV_X, x_pos, PANEL_PAINT, PANEL_NONE, NULL);

		if (other_btn) {
			x_pos += cancel_width + COLS(2);
			xv_set(other_btn, XV_X, x_pos, PANEL_PAINT, PANEL_NONE, NULL);
		}
	}
}

/*
 * Calcualte Y positions for top half of dialog, down
 * to the File List.
 */
static int fc_calc_ys_top_down(Fc_private *private)
{
    return ROWS(5) + (int) xv_get(private->ui.list, XV_HEIGHT);
}






/*
 * Calculate Y's from bottom of frame up to the File List.
 *
 * WARNING:  modifications to this code should be reflected in
 * fc_calc_min_height()!
 */
static int fc_calc_ys_bottom_up(Fc_private *private)
{
    int y_pos;
    int pix_height;

    pix_height = private->rect.r_height;


    /*
     * Open, Cancel and Save buttons are 2 rows above the
     * footer.
     */
    y_pos = pix_height - (int)xv_get(private->ui.open_btn, XV_HEIGHT) -ROWS(1);

    y_pos -= ROWS(2);		/* 2 rows white-space above buttons */

    return y_pos;
}





/*
 * Calc Y's presuming exten_func done.  Adjusts File List
 * to fit into new size.
 */
static int fc_recalc_ys(Fc_private *private, int top, Rect *exten_rect)
{
	int top_of_bottom;
	int list_row_height;
	Rect *list_rect;

	top_of_bottom = fc_calc_ys_bottom_up(private);

	list_row_height = (int)xv_get(private->ui.list, PANEL_LIST_ROW_HEIGHT);


	/*
	 * Account for extension area.  
	 */
	if (exten_rect->r_height > 0)
		top_of_bottom -= ROWS(1.5) + exten_rect->r_height;


	/*
	 * Make up for extra row added to Save dialog size.
	 */
	if (private->type != FILE_CHOOSER_OPEN)
		top_of_bottom -= ROWS(1);


	/*
	 * Update the number of rows displayed in the List according to how
	 * much space is left after the rest of the layout. Note that the 3
	 * is carried over as the minimum number of rows set in fc_calc_ys_
	 * top_down().
	 */
	xv_set(private->ui.list,
			PANEL_LIST_DISPLAY_ROWS,
							((top_of_bottom - top) / list_row_height) + 3,
			PANEL_PAINT, PANEL_NONE,
			NULL);


	/*
	 * Do this here to make sure that the document typein is at a
	 * static position below the List, and all the residual SLACK is
	 * taken up in the space between it and the buttons (and exten
	 * area ).
	 */
	list_rect = (Rect *) xv_get(private->ui.list, XV_RECT);
	if (private->ui.document_txt) {
		Rect *doc_rect;

		/* typein is 1/2 row below the list */
		xv_set(private->ui.document_txt,
				XV_Y, rect_bottom(list_rect) + ROWS(.5),
				PANEL_PAINT, PANEL_NONE, NULL);

		doc_rect = (Rect *) xv_get(private->ui.document_txt, XV_RECT);
		exten_rect->r_top = rect_bottom(doc_rect) + ROWS(1.5);
	}
	else {
		exten_rect->r_top = rect_bottom(list_rect) + ROWS(1.5);
	}

	return top_of_bottom;
}




/*
 * Do complete Y layout.  Return the maximum size that the
 * Extension Rect may expand to.
 */
static int fc_calc_ys(Fc_private *private, int *y_anchor, Rect *exten_rect)
{
    int max_exten;
 
    *y_anchor = fc_calc_ys_top_down( private );
    (void) fc_recalc_ys( private, *y_anchor, exten_rect );


    /*
     * Calculate the maximum the exten rect can expand to and
     * still preserve the layout policy.  Also consider the
     * additional white space perscribed for extension area.
     */
    max_exten = exten_rect->r_top +exten_rect->r_height - *y_anchor - ROWS(1.5);

    /* area for Save type-in includes 1/2 row white space under list */
    if ( private->ui.document_txt )
	max_exten -= (int) xv_get(private->ui.document_txt, XV_HEIGHT) + ROWS(.5);

    return max_exten;
}






/*
 * Do Relative layout of UI objects.  See Open Look "Application
 * File Choosing Human Interface Specification" for engineering
 * diagrams.
 *
 * Called on WIN_RESIZE iff width or height changed.
 */
Pkg_private void file_chooser_position_objects(Fc_private *private)
{
	int max_height;
	int y_anchor;
	Rect exten_rect;

	exten_rect.r_height = private->exten_height;
	fc_calc_xs(private, &exten_rect);
	max_height = fc_calc_ys(private, &y_anchor, &exten_rect);


	/*
	 * Make extension callback and recalc y's to meet the
	 * returned height.
	 */
	if (private->exten_func) {
		int new_height;

		new_height = (*private->exten_func) (FC_PUBLIC(private),
				&private->rect, &exten_rect,
				COLS(2), private->rect.r_width - COLS(2), max_height);

		/*
		 * if height changed, redo the y's 
		 */
		if ((new_height != -1)
				&& (new_height != exten_rect.r_height)
				) {
			if (new_height > max_height)
				new_height = max_height;
			exten_rect.r_height = new_height;
			fc_recalc_ys(private, y_anchor, &exten_rect);
		}
	}


	/*
	 * Finally, repaint the panel.
	 */
	panel_paint(private->ui.panel, PANEL_CLEAR);
}


/*
 * Calculate the minimum size of the frame.
 */
Pkg_private void file_chooser_calc_min_size(Fc_private *private, int *	width, int *	height)
{
	int ph, lh, rh, a_little_margin;
	Rect *r;


	r = (Rect *)xv_get(private->ui.cancel_btn, XV_RECT);
    *width = rect_right(r);

	ph = (int)xv_get(private->ui.panel, XV_HEIGHT);
	lh = (int)xv_get(private->ui.list, XV_HEIGHT);
	rh = (int) xv_get(private->ui.list, PANEL_LIST_ROW_HEIGHT);
	a_little_margin = 22;
    *height = ph - lh + 3 * rh + a_little_margin;
} /* file_chooser_calc_min_size() */


/*
 * Calculate the default size of the frame.
 */
Pkg_private void file_chooser_calc_default_size(Fc_private *private, int	min_width, int	min_height, int *	width, int *	height)
{
    *width = (int)xv_get(private->ui.panel, XV_WIDTH);
    *height = (int)xv_get(private->ui.panel, XV_HEIGHT);
} /* file_chooser_calc_default_size() */

/*----------------------------------------------------------------------------*/
