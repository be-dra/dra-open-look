/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */

#include <X11/Xlib.h>
#include <xview/xview.h>
#include <xview/dragdrop.h>
#include <xview_private/portable.h>
#include <xview_private/win_info.h>
#ifdef SVR4
#include <stdlib.h>
#endif  /* SVR4 */

#define DD_FAILED  -1

static int HandleNewDrop(Event *ev, XClientMessageEvent *cM, Xv_window window,
			char *buffer, unsigned int bsize);

int xv_decode_drop(Event *ev, char *buffer, unsigned int bsize)
{
	unsigned long nitems, bytes_after;
	XClientMessageEvent *cM;
	Atom actual_type;
	int actual_format, data_length, return_value, NoDelete = False;
	char *data;
	Xv_window window;
	Xv_server server;


	if ((event_action(ev) != ACTION_DRAG_COPY) &&
			(event_action(ev) != ACTION_DRAG_MOVE) &&
			(event_action(ev) != ACTION_DRAG_LOAD))
		return (DD_FAILED);
	/* Dig out the ClientMessage event. */
	cM = (XClientMessageEvent *) event_xevent(ev);

	if ((window = win_data(cM->display, cM->window)) == XV_NULL)
		return (DD_FAILED);

	server = XV_SERVER_FROM_WINDOW(window);

	if (cM->message_type == xv_get(server,SERVER_ATOM,"_SUN_DRAGDROP_TRIGGER"))
		return HandleNewDrop(ev, cM, window, buffer, bsize);


	if (XGetSelectionOwner(cM->display, XA_PRIMARY) == None) return DD_FAILED;

	/* Use client set property. */
	if ((cM->data.l[4]) && (XGetWindowProperty(cM->display,
							(Window)cM->data.l[3],
							(Atom)cM->data.l[4], 0L, (long)((bsize + 3) / 4),
							True, AnyPropertyType, &actual_type,
							&actual_format, &nitems, &bytes_after,
							(unsigned char **)&data) == Success)) {
		data_length = strlen(data);
		return_value = data_length + bytes_after;
		if (data_length >= bsize) {
			data_length = bsize - 1;
			NoDelete = True;
		}
		XV_BCOPY(data, buffer, (size_t)data_length);
		buffer[data_length] = '\0';
		XFree(data);
	}
	else {
		Selection_requestor  sel;
		char 		*buf;
		long length;
		int format;

		sel = xv_create(window, SELECTION_REQUESTOR,
					SEL_RANK,	XA_PRIMARY,
					SEL_TYPE,	XA_STRING,
					SEL_TIME,	&event_time(ev),
					NULL);

		buf = (char *)xv_get(sel, SEL_DATA, &length, &format);
		if (length == SEL_ERROR) return DD_FAILED;

		return_value = length;

		if (length >= bsize) {
			bsize -= 1;
			NoDelete = True;
		}
		XV_BCOPY(buf, buffer, (size_t)bsize);
		buffer[bsize] = '\0';
		free(buf);
		/* If this is a move operation, we must ask
		 * the source to delete the selection object.
		 * We should only do this if the transfer of
		 * data was successful.
		 */
		if (event_action(ev) == ACTION_DRAG_MOVE && !NoDelete) {
			int length, format;
			xv_set(sel, SEL_TYPE_NAME, "DELETE", NULL);
			(void)xv_get(sel, SEL_DATA, &length, &format);
		}
		dnd_done(sel);
	}
	return return_value;
}

static int HandleNewDrop(Event *ev, XClientMessageEvent *cM, Xv_window window,
			char *buffer, unsigned int bsize)
{
    Selection_requestor  sel;
    char 		*buf;
    long length;
	int format, NoDelete = False;

    sel = xv_create(window, SELECTION_REQUESTOR,
				SEL_TYPE,	XA_STRING,
				SEL_TIME,	&event_time(ev),
				NULL);

    (void)dnd_decode_drop(sel, ev);

    buf = (char *)xv_get(sel, SEL_DATA, &length, &format);
    if (length == SEL_ERROR) return(DD_FAILED);

    if (length >= bsize) {
	bsize -= 1;
	NoDelete = True;
    }
    XV_BCOPY(buf, buffer, (size_t)bsize);
    buffer[bsize] = '\0';
    free(buf);
                                /* If this is a move operation, we must ask
                                 * the source to delete the selection object.
                                 * We should only do this if the transfer of
                                 * data was successful.
                                 */
    if (event_action(ev) == ACTION_DRAG_MOVE && !NoDelete) {
        int length, format;
        xv_set(sel, SEL_TYPE_NAME, "DELETE", NULL);
        (void)xv_get(sel, SEL_DATA, &length, &format);
    }
    dnd_done(sel);

	return length;
}
