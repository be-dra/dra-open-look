#ifndef lint
#ifdef SCCS
static char     sccsid[] = "@(#)sel_req.c 1.17 90/12/14 DRA: $Id: sel_req.c,v 4.49 2026/02/17 17:24:35 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1990 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE
 *	file for terms of the license.
 */

#include <xview_private/sel_impl.h>
#include <xview_private/svr_impl.h>
#include <xview_private/win_info.h>
#include <xview/server.h>
#include <xview/window.h>
#include <xview/notify.h>
#include <sys/time.h>
#if defined(SVR4) || defined(__linux)
#include <stdlib.h>
#endif  /* SVR4 */

#define ITIMER_NULL  ((struct itimerval *)0)

#ifndef NULL
#define NULL 0
#endif

extern char *xv_app_name;

Pkg_private char *xv_sel_atom_to_str(Display *, Atom, Window);
Pkg_private int xv_sel_add_prop_notify_mask(Display *,Atom,XWindowAttributes *);

/* this function works as (temporary) reply_proc during an incremental
 * XA_STRING request
 */
static void collect_incr_string(Selection_requestor self, Atom target,
				Atom type, Xv_opaque value, unsigned long length, int format)
{
	Xv_window win = xv_get(self, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(win);

	if (target == XA_STRING
#ifdef NO_XDND
#else /* NO_XDND */
		|| target == (Atom)xv_get(srv, SERVER_ATOM, "text/plain")
#endif /* NO_XDND */
	) {
		Sel_req_info *selReq = SEL_REQUESTOR_PRIVATE(self);

		if (type == (Atom) xv_get(srv, SERVER_ATOM, "INCR")) {
			selReq->is_incremental = TRUE;
		}
		else if (! selReq->is_incremental) {
			selReq->incr_collector = (char *)value;
			selReq->incr_size = 0L;
			selReq->reply_proc = selReq->saved_reply_proc;
		}
		else if (length) {
			char *string;
			unsigned long len = selReq->incr_size;
			if (! len)
				string = (char *)xv_malloc((size_t)length);
			else {
				char *str = selReq->incr_collector;

				string = (char *)xv_realloc(str, (size_t)(len + length));
			}
			strncpy(string + len, (char *)value, (size_t)length);
			selReq->incr_collector	= string;
			selReq->incr_size = len + length;
		}
		else {
			selReq->is_incremental = FALSE;
			selReq->incr_size = 0L;
			selReq->reply_proc = selReq->saved_reply_proc;
		}
	}
}


static int sel_req_init(Xv_Window parent, Selection_requestor sel_req_public,
								Attr_avlist avlist, int *u)
{
    Sel_req_info     *sel_req;
    Xv_sel_requestor *sel_req_object = (Xv_sel_requestor *) sel_req_public;

    /* Allocate and clear private data */
    sel_req = xv_alloc(Sel_req_info);

    /* Link private and public data */
    sel_req_object->private_data = (Xv_opaque) sel_req;
    sel_req->public_self = sel_req_public;

    /* Initialize private data */
    sel_req->nbr_types = 1;
    sel_req->typeIndex = 0;
    sel_req->typeTbl = xv_alloc( Sel_type_tbl );
    sel_req->typeTbl->type = XA_STRING;
    sel_req->typeTbl->status = 0;
    sel_req->typeTbl->type_name = "STRING";

    return XV_OK;
}

static int selreq_reply_key = 0;

static void cleanup_replyinfo(Xv_object obj, int key, char *data)
{
	/* I never saw this with data != NULL */
	if (key == selreq_reply_key && data != NULL) {
		Sel_reply_info *replyInfo = (Sel_reply_info *)data;

		fprintf(stderr, "in cleanup_replyinfo, data=%ld)\n", replyInfo->sri_propdata);
    	if (replyInfo->sri_propdata) xv_free(replyInfo->sri_propdata);
		xv_free(data);
	}
}

static Xv_opaque sel_req_set_avlist(Selection_requestor sel_req_public,
									Attr_avlist avlist)
{
	Sel_req_info *sel_req = SEL_REQUESTOR_PRIVATE(sel_req_public);
	int type_set = FALSE;
	int types_set = FALSE;
	int type_name_set = FALSE;
	int apndTypeSet = FALSE;
	int apndTypeNameSet = FALSE;
	int type_names_set = FALSE;
	Sel_info *sel_info;
	Sel_prop_info *propInfo;
	Attr_avlist attrs;
	int i;
	int numApndTypes, numTypes = 0;
	Xv_window win;
	XID xid = 0;
	Xv_opaque propdata[100]; /* who requests more than 100 targets??? */
	int propdata_set = FALSE;

	win = (Xv_window) xv_get(sel_req_public, XV_OWNER);
	xid = (XID) xv_get(win, XV_XID);

	sel_info = SEL_PRIVATE(sel_req->public_self);

	for (attrs = avlist; *attrs; attrs = attr_next(attrs)) switch (attrs[0]) {
		case SEL_REPLY_PROC:
			sel_req->reply_proc = (selection_reply_proc_t) attrs[1];
			break;
		case SEL_TYPE:
			sel_req->nbr_types = 1;
			sel_req->typeTbl[0].type = (Atom) attrs[1];
			sel_req->typeTbl[0].status = 0;
			type_set = TRUE;
			break;
		case SEL_TYPES:
			free(sel_req->typeTbl);
			for (i = 1; attrs[i]; i++);
			sel_req->nbr_types = i - 1;	/* don't count NULL terminator */

			sel_req->typeTbl =
					(Sel_type_tbl *) xv_calloc((unsigned)sel_req->nbr_types,
					(unsigned)sizeof(Sel_type_tbl));

			for (i = 1; i <= sel_req->nbr_types; i++) {
				sel_req->typeTbl[i - 1].type = (Atom) attrs[i];
				sel_req->typeTbl[i - 1].status = 0;
			}
			types_set = TRUE;
			break;
		case SEL_APPEND_TYPES:
			for (i = 1; attrs[i]; i++);
			numApndTypes = i - 1;	/* don't count NULL terminator */
			numTypes = sel_req->nbr_types;

			sel_req->typeTbl =
					(Sel_type_tbl *) xv_realloc((char *)sel_req->typeTbl,
					(size_t)((numTypes +
									numApndTypes) * sizeof(Sel_type_tbl)));

			for (i = 1; i <= numApndTypes; i++) {
				sel_req->typeTbl[numTypes - 1 + i].type = (Atom) attrs[i];
				sel_req->typeTbl[numTypes - 1 + i].status = 0;
			}
			sel_req->nbr_types += numApndTypes;
			apndTypeSet = TRUE;
			break;
		case SEL_APPEND_TYPE_NAMES:
			for (i = 1; attrs[i]; i++);
			numApndTypes = i - 1;	/* don't count NULL terminator */
			numTypes = sel_req->nbr_types;

			sel_req->typeTbl =
					(Sel_type_tbl *) xv_realloc((char *)sel_req->typeTbl,
					(size_t)((numTypes +
									numApndTypes) * sizeof(Sel_type_tbl)));

			for (i = 1; i <= numApndTypes; i++) {
				sel_req->typeTbl[numTypes - 1 + i].type_name =
						(char *)attrs[i];
				sel_req->typeTbl[numTypes - 1 + i].status = 0;
			}
			sel_req->nbr_types += numApndTypes;
			apndTypeNameSet = TRUE;
			break;
		case SEL_TYPE_NAME:
			sel_req->nbr_types = 1;
			sel_req->typeTbl->type_name = (char *)attrs[1];
			sel_req->typeTbl->status = 0;
			type_name_set = TRUE;
			break;
		case SEL_TYPE_NAMES:
			free(sel_req->typeTbl);
			for (i = 1; attrs[i]; i++);
			sel_req->nbr_types = i - 1;	/* don't count NULL terminator */

			sel_req->typeTbl =
					(Sel_type_tbl *) xv_calloc((unsigned)sel_req->nbr_types,
					(unsigned)sizeof(Sel_type_tbl));

			for (i = 1; i <= sel_req->nbr_types; i++) {
				sel_req->typeTbl[i - 1].type_name = (char *)attrs[i];
				sel_req->typeTbl[i - 1].status = 0;
			}
			type_names_set = TRUE;
			break;
		case SEL_TYPE_INDEX:
			sel_req->typeIndex = (int)attrs[1];
			if (!sel_req->typeTbl[sel_req->typeIndex].propInfo) {
				sel_req->typeTbl[sel_req->typeIndex].propInfo =
						xv_alloc(Sel_prop_info);
			}
			propInfo = sel_req->typeTbl[sel_req->typeIndex].propInfo;
			if (propInfo->data) xv_free(propInfo->data);
			propInfo->data = XV_NULL;
			propInfo->format = 8;
			propInfo->length = 0;
			propInfo->type = XA_STRING;
			propInfo->typeName = (char *)NULL;
			break;
		case SEL_PROP_DATA:
			i = sel_req->typeIndex;
			propdata[i] = (Xv_opaque) attrs[1];
			propdata_set = TRUE;
			sel_req->typeTbl[i].status |= SEL_PROPERTY_DATA;
			break;
		case SEL_PROP_LENGTH:
			i = sel_req->typeIndex;
			sel_req->typeTbl[i].propInfo->length = (long)attrs[1];
			break;
		case SEL_PROP_FORMAT:
			i = sel_req->typeIndex;
			sel_req->typeTbl[i].propInfo->format = (int)attrs[1];
			break;
		case SEL_PROP_TYPE:
			i = sel_req->typeIndex;
			sel_req->typeTbl[i].propInfo->type = (Atom) attrs[1];
			break;
		case SEL_PROP_TYPE_NAME:
			i = sel_req->typeIndex;
			sel_req->typeTbl[i].propInfo->typeName = (char *)attrs[1];
			break;
		case SEL_AUTO_COLLECT_INCR:
			sel_req->auto_collect_incr = (int)attrs[1];
			if (sel_req->auto_collect_incr) {
				sel_req->saved_reply_proc = sel_req->reply_proc;
				sel_req->reply_proc = collect_incr_string;
			}
			break;
		case XV_END_CREATE:
			if (! selreq_reply_key) selreq_reply_key = xv_unique_key();
			xv_set(sel_req_public,
				XV_KEY_DATA, selreq_reply_key, XV_NULL,
				XV_KEY_DATA_REMOVE_PROC, selreq_reply_key, cleanup_replyinfo,
				NULL);
			break;
	}

	if (type_set && !type_name_set)
		sel_req->typeTbl[0].type_name = xv_sel_atom_to_str(sel_info->dpy,
				sel_req->typeTbl[0].type, xid);
	else if (type_name_set && !type_set)
		sel_req->typeTbl[0].type = xv_sel_str_to_atom(sel_info->dpy,
				sel_req->typeTbl[0].type_name, xid);
	else if (types_set && !type_names_set) {
		for (i = 1; i <= sel_req->nbr_types; i++) {
			sel_req->typeTbl[i - 1].type_name =
					xv_sel_atom_to_str(sel_info->dpy,
					sel_req->typeTbl[i - 1].type, xid);
		}
	}
	else if (type_names_set && !types_set) {
		for (i = 1; i <= sel_req->nbr_types; i++)
			sel_req->typeTbl[i - 1].type = xv_sel_str_to_atom(sel_info->dpy,
					sel_req->typeTbl[i - 1].type_name, xid);
	}

	if (propdata_set) {
    	Sel_prop_info  *pi;
		for (i = numTypes; i < sel_req->nbr_types; i++) {
			Sel_type_tbl *tbl = sel_req->typeTbl + i;
			pi = tbl->propInfo;
			if (pi->data) xv_free(pi->data);
			pi->data = XV_NULL;

			if (propdata[i]) {
				size_t bytelen = BYTE_SIZE(pi->length, pi->format);
				pi->data = (Xv_opaque)xv_malloc(bytelen + 1);

				memcpy((void *)pi->data, (void *)propdata[i], bytelen);
				propdata[i] = XV_NULL;
			}
		}
	}

	if (apndTypeSet && !apndTypeNameSet) {
		for (i = numTypes; i < sel_req->nbr_types; i++) {
			sel_req->typeTbl[i].type_name = xv_sel_atom_to_str(sel_info->dpy,
					sel_req->typeTbl[i].type, xid);
		}
	}
	else if (apndTypeNameSet && !apndTypeSet) {
		for (i = numTypes; i < sel_req->nbr_types; i++)
			sel_req->typeTbl[i].type = xv_sel_str_to_atom(sel_info->dpy,
					sel_req->typeTbl[i].type_name, xid);
	}

	for (i = 0; i < sel_req->nbr_types; i++) {
		if (sel_req->typeTbl[i].status & SEL_PROPERTY_DATA) {
			propInfo = sel_req->typeTbl[i].propInfo;
			if (propInfo->typeName == NULL)
				propInfo->typeName =
						xv_sel_atom_to_str(sel_info->dpy, propInfo->type, xid);
			else
				propInfo->type =
						xv_sel_str_to_atom(sel_info->dpy, propInfo->typeName,
						xid);
		}
	}
	return XV_OK;
}

static int selection_is_word(Selection_requestor self)
{
	long length, *wdp;
	int format, is_word;

	xv_set(self, SEL_TYPE_NAME, "_OL_SELECTION_IS_WORD", NULL);
	wdp = (long *)xv_get(self, SEL_DATA, &length, &format);
	if (length == SEL_ERROR) is_word = FALSE;
	else is_word = *wdp;
	if (wdp) xv_free(wdp);
	return is_word;
}

static XID SelGetOwnerXID(Sel_req_info  *selReq)
{
    Sel_info        *sel_info;

    sel_info = SEL_PRIVATE( selReq->public_self );
    return XGetSelectionOwner( sel_info->dpy, sel_info->rank );
}

static void SetExtendedData(Sel_reply_info *reply, Atom property, int typeIndex)
{
	Sel_prop_info *exType;

	if (reply->sri_req_info->typeTbl[typeIndex].status & SEL_PROPERTY_DATA) {
		exType = reply->sri_req_info->typeTbl[typeIndex].propInfo;

		XChangeProperty(reply->sri_dpy, reply->sri_requestor, property,
				exType->type, exType->format, PropModeReplace,
				(unsigned char *)exType->data, (int)exType->length);
	}

}

/*
* Create a reply struct.
*/
static Sel_reply_info *NewReplyInfo(Selection_requestor req, Window win,
					Sel_owner_info *selection, Time time, Sel_req_info *selReq)
{
	int numTarget;
	Sel_reply_info *replyInfo;
	Xv_window xvw = xv_get(req, XV_OWNER);
	Xv_server srv = XV_SERVER_FROM_WINDOW(xvw);

	replyInfo = xv_alloc(Sel_reply_info);

	if (selection) selection->req_info = SEL_REQUESTOR_PRIVATE(req);

	numTarget = selReq->nbr_types;
	replyInfo->sri_local_owner = selection;
	replyInfo->sri_selection = xv_get(req, SEL_RANK);
	replyInfo->sri_srv = srv;
	replyInfo->sri_dpy = (Display *)xv_get(srv, XV_DISPLAY);
	replyInfo->sri_incr = xv_get(srv, SERVER_ATOM, "INCR");
	replyInfo->sri_multiple = xv_get(srv, SERVER_ATOM, "MULTIPLE");

	/* MLK */
	replyInfo->sri_target = (Atom *) xv_calloc((unsigned)numTarget + 1,
			(unsigned)sizeof(Atom));

	if (numTarget > 1) {
		Atom *target;

		target = (Atom *) xv_get(req, SEL_TYPES);
		*replyInfo->sri_target = replyInfo->sri_multiple;
		XV_BCOPY((char *)target, (char *)replyInfo->sri_target + sizeof(Atom),
				numTarget * sizeof(Atom));
	}
	else {
		Atom target;

		target = (Atom) xv_get(req, SEL_TYPE);
		XV_BCOPY((char *)&target, replyInfo->sri_target, numTarget * sizeof(Atom));
	}

	replyInfo->sri_property = xv_sel_get_property(replyInfo->sri_srv, replyInfo->sri_dpy);
	replyInfo->sri_requestor = win;
	if (time == 0)
		replyInfo->sri_time = xv_sel_get_last_event_time(srv, replyInfo->sri_dpy, win);
	else
		replyInfo->sri_time = time;

	replyInfo->sri_timeout = xv_get(req, SEL_TIMEOUT_VALUE);
	replyInfo->sri_multiple_count = 0;
	replyInfo->sri_propdata = (Xv_opaque) NULL;
	replyInfo->sri_format = 0;
	replyInfo->sri_during_incr = 0;
	replyInfo->sri_status = 0;
	replyInfo->sri_length = (long)NULL;
	replyInfo->sri_req_info = selReq;

	if (numTarget == 1)
		SetExtendedData(replyInfo, replyInfo->sri_property, 0);

	return (replyInfo);
}

/*
* This function is called by the selection requestor. It sets the MULTIPLE
* atom pairs on the requestor window.
*/
static void SetupMultipleRequest(Sel_reply_info *reply, int numTarget)
{
	atom_pair *pairPtr;
	int i, count;

	reply->sri_atomPair = (atom_pair *) xv_calloc((unsigned)numTarget + 1,
			(unsigned)sizeof(atom_pair));

	count = numTarget;

	for (i = 1, pairPtr = reply->sri_atomPair; count > 0;
			pairPtr++, count--, i++) {
		pairPtr->target = *(reply->sri_target + i);
		pairPtr->property = xv_sel_get_property(reply->sri_srv, reply->sri_dpy);
		SetExtendedData(reply, pairPtr->property, i - 1);
	}

	XChangeProperty(reply->sri_dpy, reply->sri_requestor,
			reply->sri_property, reply->sri_property, 32, PropModeReplace,
			(unsigned char *)reply->sri_atomPair, numTarget * 2);

	reply->sri_multiple_count = numTarget;
}

static void SelSaveData(char *buffer, Sel_reply_info *reply, int size)
{
    char  *strPtr;


    /*
     * One extra byte is malloced than is needed. This last byte is null
     * terminated for returning string properties, so the client does't then
     * have to recopy the string to make it null terminated.
     * This is done to make local process transactions to be exactly like
     * non-local transactions. XGetWindowProperty null terminates the last
     * extra byte.
     */
    reply->sri_propdata = (Xv_opaque) xv_malloc((size_t)size+1 );

    strPtr = (char *) reply->sri_propdata;
    strPtr[size] = '\0';

    if ( buffer != (char *) NULL )
        XV_BCOPY( (char *)buffer, (char *)reply->sri_propdata, (size_t)size );
}

/*
 * This routine transfers the local process data in increments.
*/
static int HandleLocalIncr(Sel_req_info *selReq, char *replyBuf, Sel_reply_info *reply, Atom target, Atom retType)
{
	Atom replyType;
	int byteLength;
	int offset = 0;
	unsigned long size;
	unsigned long svr_max_req_size;

	replyType = target;
	if (selReq->reply_proc == NULL)
		return FALSE;

	/*
	 * We are setting the size to a lower bound on the number of bytes of
	 * data in the selection.
	 * Clients who wish to send the selection data incrementally by setting
	 * the type to INCR, should set the reply buffer to a lower bound on the
	 * number of bytes of
	 */
	if (retType != reply->sri_incr) {
		size = BYTE_SIZE(reply->sri_length, reply->sri_format);
		SelSaveData((char *)&size, reply, (unsigned)sizeof(int));
	}

	/*
	 * Call the user defined reply_proc with replyType set to INCR and
	 * reply->data set to the a lower bound of the number of bytes of data
	 * in the selection.
	 */
	(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target,
			reply->sri_incr, reply->sri_propdata, 1L,32);

	/*
	 * If this is INCR from a multiple request; we need to call the
	 * user defined convert_proc with format set to SEL_MULTIPLE.
	 */
	if (reply->sri_req_info->nbr_types > 1)
		reply->sri_format = SEL_MULTIPLE;


	svr_max_req_size = (MAX_SEL_BUFF_SIZE(reply->sri_dpy) << 2) - 100;

	/*
	 * If the user has decided to send the data in increments,  call it's
	 * convert_proc the second time to get the actual data (the first time
	 * it told us that the data is being transferred in increments.
	 */
	if (retType == reply->sri_incr) {
		reply->sri_length = svr_max_req_size;
		if (!sel_wrap_convert_proc(reply->sri_local_owner->public_self,
						&replyType, (Xv_opaque *) & replyBuf,
						&reply->sri_length, &reply->sri_format))
		{
			xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply, target,TRUE);
			reply->sri_format = 0;
			reply->sri_length = 0;
			XFree((char *)(reply->sri_propdata));
			reply->sri_propdata = (Xv_opaque) NULL;
			return FALSE;
		}
	}

	byteLength = BYTE_SIZE(reply->sri_length, reply->sri_format);
	offset = 0;

	do {
		size = ((byteLength - offset) > svr_max_req_size) ?
				svr_max_req_size : (byteLength - offset);

		SelSaveData(replyBuf + offset, reply, (int)size);

		/* We have the first batch of data */
		(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target,
				replyType, reply->sri_propdata, size, reply->sri_format);

		offset += size;

		/*
		 * If the selection owner has explicitly asked for incremental data
		 * transfer; execute the passed-in convert routine again.
		 */
		if (retType == reply->sri_incr) {
			/*
			 * Set the length to max buffer size and Run the convert proc.
			 */
			reply->sri_length = svr_max_req_size;

			/*
			 * If this is INCR from a multiple request; we need to call the
			 * user defined convert_proc with format set to SEL_MULTIPLE.
			 */
			if (reply->sri_req_info->nbr_types > 1)
				reply->sri_format = SEL_MULTIPLE;

			/* Get the next batch */
			replyType = target;
			if (!sel_wrap_convert_proc(reply->sri_local_owner->public_self,
							&replyType, (Xv_opaque *) & replyBuf,
							&reply->sri_length, &reply->sri_format))
			{
				xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply,
														target, TRUE);
				reply->sri_format = 0;
				reply->sri_length = 0;
				XFree((char *)(reply->sri_propdata));
				reply->sri_propdata = (Xv_opaque) NULL;
				return FALSE;
			}

			byteLength = BYTE_SIZE(reply->sri_length, reply->sri_format);
			offset = 0;
		}
		if (!(byteLength - offset))
			break;
	} while (1);

	/*
	 * The owner calls us with a zero length data to indicate the end of data
	 * transfer. We call the user defined reply_proc with
	 * the zero length data  to indicate to the client the end of
	 * incremental data transfer.
	 */
	(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target, replyType,
			reply->sri_propdata, 0L, reply->sri_format);

	if (reply->sri_local_owner->done_proc) {
		(*reply->sri_local_owner->done_proc) (reply->sri_local_owner->public_self,
				(Xv_opaque) replyBuf, target);
	}
	else {
		/* does dim done_proc yma - felly, rhyddheuwn ni ein hunain */
		if (replyBuf) xv_free(replyBuf);
	}

	return TRUE;
}

static int TransferLocalData(Sel_req_info *selReq, Sel_reply_info *reply,
						Atom target, int blocking, int multipleIndex)
{
	Atom replyType;
	char *replyBuff;
	unsigned long svr_max_req_size;

	if (target == reply->sri_local_owner->atomList->timestamp) {
		reply->sri_propdata = (Xv_opaque) & reply->sri_local_owner->time;
		reply->sri_length = 1;
		reply->sri_format = 32;
		if (selReq->reply_proc) {
			/* let's hope they dont free reply->sri_propdata... */
			(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target,
					reply->sri_local_owner->atomList->integer,
					reply->sri_propdata, reply->sri_length, reply->sri_format);
		}
		xv_sel_free_property(reply->sri_srv,reply->sri_dpy,reply->sri_property);
		return TRUE;
	}

	replyType = target;
	/*
	 * set the length to the max_server-buffer size; set the format to
	 * indicate the beginning or end of a multiple conversion.
	 */

	svr_max_req_size = (MAX_SEL_BUFF_SIZE(reply->sri_dpy) << 2) - 100;
	reply->sri_length = svr_max_req_size;
	reply->sri_format = multipleIndex;

	if (!sel_wrap_convert_proc(reply->sri_local_owner->public_self,
					&replyType, (Xv_opaque *)&replyBuff, &reply->sri_length,
					&reply->sri_format)) {

		if (selReq->reply_proc != NULL)
			xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply, target,TRUE);

		xv_sel_free_property(reply->sri_srv,reply->sri_dpy,reply->sri_property);
		return FALSE;
	}

	/*
	 * Note: The user done_proc should free "replyBuff".
	 *       The user reply_proc should free "reply->data".
	 */
	SelSaveData(replyBuff, reply, (int)BYTE_SIZE(reply->sri_length, reply->sri_format));

	if (((Atom) replyType == reply->sri_incr)
		|| (BYTE_SIZE(reply->sri_length, reply->sri_format) >= svr_max_req_size)) {

		reply->sri_during_incr = 1;
		SERVERTRACE((355, "%s: sri_during_incr = 1\n", __FUNCTION__));

		/*
		 * If we are processing a blocking request and the selection owner is
		 * transferring the data in increments or the size of selection data
		 * is larger than the max selection size and the requestor hasn't
		 * registered a reply_proc, reject the data transfer.
		 */
		if (blocking && (selReq->reply_proc == NULL)) {
			xv_sel_free_property(reply->sri_srv,reply->sri_dpy,reply->sri_property);
			return FALSE;
		}
		return HandleLocalIncr(selReq, replyBuff, reply, target, replyType);
	}

	if (selReq->reply_proc)
		(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target, replyType,
				reply->sri_propdata, reply->sri_length, reply->sri_format);

	if (reply->sri_local_owner->done_proc) {
		(*reply->sri_local_owner->done_proc) (reply->sri_local_owner->public_self, (Xv_opaque)replyBuff, target);
	}
	else {
		/* does dim done_proc yma - felly, rhyddheuwn ni ein hunain */
		/* nage, cawson ni 'invalid free': if (replyBuff) xv_free(replyBuff); */
	}

	xv_sel_free_property(reply->sri_srv,reply->sri_dpy,reply->sri_property);
	return TRUE;
}

static int LocalMultipleTrans(Sel_reply_info *reply, Sel_req_info *selReq,
								int blocking)
{
	unsigned long length;
	unsigned long bytesafter;
	atom_pair *atomPair;
	unsigned char *prop;
	int format;
	Atom target;
	int byteLen, i;
	int multipleIndex = SEL_MULTIPLE, firstTarget = 1;

	/*
	 * The contents of the property named in the request is a list
	 * of atom pairs, the first atom naming a target, and the second
	 * naming a property. Do not delete the data. The requestor needs to
	 * access this data.
	 */
	if (XGetWindowProperty(reply->sri_dpy, reply->sri_requestor,
					reply->sri_property, 0L, 1000000L,
					False, (Atom) AnyPropertyType, &target, &format,
					&length, &bytesafter, &prop) != Success) {
		xv_error(selReq->public_self,
				ERROR_STRING, XV_MSG("XGetWindowProperty Failed"),
				ERROR_PKG, SELECTION,
				NULL);
		xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply,
				reply->sri_local_owner->atomList->multiple, TRUE);
	}

	byteLen = BYTE_SIZE(length, format) / sizeof(atom_pair);

	for (i = 0, atomPair = (atom_pair *) prop; byteLen;
			i++, atomPair++, byteLen--) {

		reply->sri_req_info->typeIndex = i;


		if (firstTarget) {
			multipleIndex = SEL_BEGIN_MULTIPLE;
			firstTarget = 0;
		}

		if (!(byteLen - 1))
			multipleIndex = SEL_END_MULTIPLE;

		TransferLocalData(selReq, reply, atomPair->target, blocking, multipleIndex);
		xv_sel_free_property(reply->sri_srv, reply->sri_dpy, atomPair->property);
		multipleIndex = SEL_MULTIPLE;
	}

	xv_sel_free_property(reply->sri_srv, reply->sri_dpy, reply->sri_property);
	XFree((char *)prop);
	return TRUE;
}


/*
 * If this process is both the selection owner and the selection requestor,
 * don't transfer the data through X.
 */
static int HandleLocalProcess(Sel_req_info   *selReq, Sel_reply_info *reply, int blocking)
{
	if (selReq->nbr_types > 1) {
		if (blocking && (selReq->reply_proc == NULL)) return FALSE;
		return LocalMultipleTrans(reply, selReq, blocking);
	}

	return TransferLocalData(selReq, reply, *reply->sri_target, blocking, 0);
}

static int CheckSelectionNotify(Sel_req_info *selReq, Sel_reply_info *replyInfo,
								XSelectionEvent *ev, int blocking )
{
	if (ev->time != replyInfo->sri_time) {
		xv_sel_handle_error(SEL_BAD_TIME, selReq, replyInfo,
				*replyInfo->sri_target, ev->send_event);
		return (FALSE);
	}
	if (ev->requestor != replyInfo->sri_requestor) {
		xv_sel_handle_error(SEL_BAD_WIN_ID, selReq, replyInfo,
				*replyInfo->sri_target, ev->send_event);
		return (FALSE);
	}
	/*
	 * If the "property" field is None, the conversion has been refused.
	 * This can mean that there is no owner for the selection, that the owner
	 * does not support the conversion impiled by "target", or that the
	 * server did not have sufficient space.
	 */
	if (ev->property == None) {
		xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, replyInfo,
				*replyInfo->sri_target, ev->send_event);
		if (!blocking && !replyInfo->sri_multiple_count)
			xv_sel_end_request(replyInfo);
		return (FALSE);
	}

	return (TRUE);
}

/*
 * Predicate function for XCheckIfEvent
 *
 * This is used during an incremental transfer
 * See also xv_sel_block_for_event in sel_util.c
 */
static int check_incr_prop_newval(Display *display, XEvent *xevent,
								XPointer args)
{
	Sel_reply_info *reply = (Sel_reply_info *)args;

	if ((xevent->type & 0177) == Expose) {
		if (0 == xevent->xexpose.count) reply->checkedEventType = Expose;
		else reply->checkedEventType = 0;
		return TRUE;
	}

	/*
	 * If some other process wants to become the selection owner, let
	 * it do so but continue handling the current transaction.
	 */
	if ((xevent->type & 0177) == SelectionClear) {
		reply->checkedEventType = SelectionClear;
		return TRUE;
	}

	if ((xevent->type & 0177) == PropertyNotify) {
		XPropertyEvent *ev = (XPropertyEvent *) xevent;

		if (ev->atom == reply->sri_property) { /* the interesting property ! */
			if (ev->state == PropertyNewValue && ev->time > reply->sri_time) {
				reply->checkedEventType = PropertyNotify;
				return TRUE;
			}

			reply->checkedEventType = 0;
			/* we want to get rid of the PropertyDelete events -
			 * they are interesting only for the selection owner.
			 * Let's remove them from the input queue:
			 */
			return TRUE;
		}
	}

	if (xevent->type == FocusOut) {
		reply->checkedEventType = FocusOut;
		return TRUE;
	}

	/* during lengthy incremental transfers I saw the following event types
	 * (filling the input queue), that we might remove from the queue:
	 */
	if (xevent->type == EnterNotify || xevent->type == LeaveNotify) {
		reply->checkedEventType = 0;
		return TRUE;
	}
	return FALSE;
}


static int ProcessIncr(Sel_req_info *selReq, Sel_reply_info *reply, Atom target,
								XSelectionEvent *ev)
{
	int format;
	Atom type;
	unsigned long length;
	unsigned long bytesafter;
	unsigned char *propValue;
	XEvent event;
	XPropertyEvent *propEv;
	XWindowAttributes winAttr;
	int status = xv_sel_add_prop_notify_mask(ev->display, reply->sri_requestor,
											&winAttr);

	/*
	 * We should start the transfer process by deleting the (type==INCR)
	 * property and by calling the requestor reply_proc with "type" set to
	 * INCR and replyBuf set to a lower bound on the number of bytes of
	 * data in the selection.
	 */
	if (XGetWindowProperty(ev->display, reply->sri_requestor,
					reply->sri_property, 0L, 1000000L, True, AnyPropertyType,
					&type, &format, &length, &bytesafter,
					(unsigned char **)&propValue) != Success)
	{
		xv_error(selReq->public_self,
				ERROR_STRING, XV_MSG("XGetWindowProperty Failed"),
				ERROR_PKG, SELECTION,
				NULL);
		xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply, target,
									ev->send_event);
		return FALSE;
	}

	/*
	 * Note: The user reply_proc should free "propValue".
	 */
	SERVERTRACE((355, "%s: init incr\n", __FUNCTION__));
	(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target, type,
			(Xv_opaque) propValue, length, format);

	do {
		/* Wait for PropertyNotify with stat==NewValue */
		if (!xv_sel_block_for_event(ev->display, &event, reply->sri_timeout,
				check_incr_prop_newval, (char *)reply, &reply->checkedEventType))
		{
			if (status)
				XSelectInput(ev->display, reply->sri_requestor,
						winAttr.your_event_mask);

			SERVERTRACE((355, "%s: incr timeout\n", __FUNCTION__));
			xv_sel_handle_error(SEL_TIMEDOUT, selReq, reply, target,
										ev->send_event);
			return FALSE;
		}

		propEv = (XPropertyEvent *)&event;

		/* retrieve the data and delete */
		if (XGetWindowProperty(propEv->display, propEv->window,
						propEv->atom, 0L, 10000000L, True, AnyPropertyType,
						&type, &format, &length, &bytesafter,
						(unsigned char **)&propValue) != Success)
		{
			xv_error(selReq->public_self,
					ERROR_STRING, XV_MSG("XGetWindowProperty Failed"),
					ERROR_PKG, SELECTION,
					NULL);
			xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply, target,
									ev->send_event);

			if (status)
				XSelectInput(ev->display, reply->sri_requestor,
						winAttr.your_event_mask);

			return FALSE;
		}

		if (!type /* != *(reply->target) */ ) {
			/* reset length */
			length = 1;
			continue;
		}

		if (length == 0) {
			if (propValue) xv_free(propValue);
			propValue = (unsigned char *)NULL;
		}

		/*
		 * The owner calls us with a zero length data to indicate the end of
		 * data transfer. We call the user defined reply_proc with
		 * the zero length data  to indicate to the client the end of
		 * incremental data transfer.
		 */
		SERVERTRACE((355, "%s: in incr, length=%ld\n", __FUNCTION__, length));
		(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target, type,
				(Xv_opaque) propValue, length, format);
	} while (length);

	/*
	 * If we have added PropertyChangeMask to the win, reset the mask to
	 * it's original state.
	 */
	if (status)
		XSelectInput(ev->display, reply->sri_requestor, winAttr.your_event_mask);

	XDeleteProperty(ev->display, reply->sri_requestor, reply->sri_property);
	return TRUE;
}

static int internalProcessIncr(Sel_req_info *selReq, Sel_reply_info *reply,
							Atom target, XSelectionEvent *ev)
{
	int retval;

	xv_set(reply->sri_srv, SERVER_APPL_BUSY, TRUE, XV_NULL, NULL);
	retval = ProcessIncr(selReq, reply, target, ev);
	xv_set(reply->sri_srv, SERVER_APPL_BUSY, FALSE, XV_NULL, NULL);

	return retval;
}

static int XvGetRequestedValue(Sel_req_info *selReq, XSelectionEvent *ev,
			Sel_reply_info *replyInfo, Atom property, Atom target,
			int unused_blocking)
{
	int format;
	Atom type;
	unsigned long length;
	unsigned long bytesafter;
	unsigned char *propValue;

	if (XGetWindowProperty(ev->display, ev->requestor,
					property, 0L, 10000000L, False,
					AnyPropertyType, &type, &format, &length,
					&bytesafter, &propValue) != Success) {
		xv_error(selReq->public_self,
				ERROR_STRING, XV_MSG("XGetWindowProperty Failed"),
				ERROR_PKG, SELECTION,
				NULL);

		xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, replyInfo, target,
										ev->send_event);
		return FALSE;
	}

	if (type == 0 && format == 0) {
		XFree(propValue);
        xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, replyInfo, target,
										ev->send_event);
		return FALSE;
	}

	/*
	 * If we are processing a blocking request and there is no reply_proc
	 * registered with package and the selection owner is
	 * transferring the data in increments, reject the data transfer.
	 */
	if ((selReq->reply_proc == NULL)
			&& (type == replyInfo->sri_incr))
	{
		XFree(propValue);
		return FALSE;
	}

	if (type == replyInfo->sri_incr) {
		replyInfo->sri_property = property;


		/*
		 * The following lines of code were used to process the INCR target
		 * without blocking the thread of execution. It could be used again
		 * if we decide to go back to a non-blocking schem.

		 if ( !blocking ) {
		 replyInfo->sri_during_incr++;
		 ProcessNonBlkIncr( selReq, replyInfo, ev, target );
		 return TRUE;

		 }
		 */


		internalProcessIncr(selReq, replyInfo, target, ev);
		XFree(propValue);
		return (SEL_INCREMENT);
	}
	replyInfo->sri_propdata = (Xv_opaque) propValue;
	replyInfo->sri_length = length;
	replyInfo->sri_format = format;

	/*
	 * Note: The user reply_proc should free "propValue".
	 */
	if (selReq->reply_proc) {
		(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target, type,
				(Xv_opaque) propValue, length, format);
		/* INCOMPLETE - what if the caller accesses replyInfo->data */
		/* darauf ist offenbar keiner vorbereitet: replyInfo->data =XV_NULL; */
	}

	XDeleteProperty(ev->display, replyInfo->sri_requestor, property);
	xv_sel_free_property(replyInfo->sri_srv, ev->display, property);
	return (TRUE);
}



/*
* This procedure is called by the requestor, after the owner has set
* the selection data on the requestor window. The target is MULTIPLE so
* the passed in user proc is call "numTarget" of times.
*/
static int ProcessMultiple(Sel_req_info *selReq, Sel_reply_info *reply,
								XSelectionEvent *ev, int blocking)
{
	atom_pair *pPtr;
	int format;
	Atom type;
	unsigned char *pair;
	unsigned long length;
	unsigned long bytesafter;
	int byteLen;

	/*
	 * Get the atom pair data that we set on our window. We can delete
	 * now since the owners has already read this.
	 */

	if (XGetWindowProperty(ev->display, ev->requestor,
					reply->sri_property, 0L, 1000000L, False,
					(Atom) AnyPropertyType, &type, &format,
					&length, &bytesafter, (unsigned char **)&pair) != Success) {
		xv_error(selReq->public_self,
				ERROR_STRING, XV_MSG("XGetWindowProperty Failed"),
				ERROR_PKG, SELECTION,
				NULL);
		xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply,
				reply->sri_local_owner->atomList->multiple, ev->send_event);
		return FALSE;
	}

	byteLen = BYTE_SIZE(length, format) / sizeof(atom_pair);

	for (pPtr = (atom_pair *) pair; byteLen; byteLen--, pPtr++) {
		/*
		 * The owner replace the property to None, if it failes to convert .
		 */
		if (pPtr->property == None)
			xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply,
					pPtr->target, ev->send_event);
		else
			XvGetRequestedValue(selReq, ev, reply, pPtr->property,
					pPtr->target, blocking);

		/*
		 * The properties used for multiple request are freed in
		 * xv_sel_end_request proc.
		 */
	}  /* for loop */
	XFree((char *)pair);
	return TRUE;
}


/*
* This function initiates and processes request. It sends a SelectionRequest
* event to the selection owner and blocks (waits) for the owner
* SelectionNotify event. If the "numTarget" is greater than one it sends a
* SelectionRequest to the owner with the target set to MULTIPLE.
*/
static int GetSelection(Display *dpy, XID xid, Sel_req_info *selReq,
									Sel_reply_info **reply, Time time)
{
	Selection_requestor requestor;
	Sel_reply_info *replyInfo;
	Atom selection;
	Sel_owner_info *sel;
	XEvent event;
	XSelectionEvent *ev;

	/* get the requestor's xid, event time, selection and the target */
	requestor = SEL_REQUESTOR_PUBLIC(selReq);
	selection = (Atom) xv_get(requestor, SEL_RANK);

	/*
	 * Have we saved any data on this window?
	 */
	/* I have not understood why we attach something that looks like
	 * the private selection OWNER data - while we are performing
	 * a REQUEST for this selection.
	 * Now I know that this is used in the case of a 'local transfer'.
	 */
	sel = xv_sel_find_selection_data(dpy, selection, xid);

	replyInfo = NewReplyInfo(requestor, xid, sel, time, selReq);
	xv_set(requestor, XV_KEY_DATA, selreq_reply_key, replyInfo, NULL);
	*reply = replyInfo;


	if (selReq->nbr_types > 1)	/* target is MULTIPLE */
		SetupMultipleRequest(replyInfo, selReq->nbr_types);

	if (sel && (sel->own == TRUE)) {
		/* Do local process data transfer */
		sel->status |= SEL_LOCAL_PROCESS;
		return HandleLocalProcess(selReq, replyInfo, 1);
	}

	/*
	 * Send the SelectionRequest message to the selection owner.
	 */
	XConvertSelection(dpy, selection, *replyInfo->sri_target,
			replyInfo->sri_property, xid, replyInfo->sri_time);

	/*
	 * Wait for the SelectionNotify event.
	 */
	if (!xv_sel_block_for_event(dpy, &event, replyInfo->sri_timeout,
					xv_sel_check_selnotify, (char *)replyInfo, NULL)) {
		xv_sel_handle_error(SEL_TIMEDOUT, selReq, replyInfo,
				*replyInfo->sri_target, FALSE);
		xv_sel_free_property(replyInfo->sri_srv, dpy, replyInfo->sri_property);
		return FALSE;
	}
	ev = (XSelectionEvent *)&event;

	if (!CheckSelectionNotify(selReq, replyInfo, ev, 1))
	{
		xv_sel_free_property(replyInfo->sri_srv, dpy, replyInfo->sri_property);
		return FALSE;
	}

	if (ev->target == replyInfo->sri_multiple || replyInfo->sri_multiple_count)
	{

		/*
		 * If we are processing a blocking request and the requestor
		 * has requested for MULTIPLE targets with no reply_proc
		 * registered, reject the data transfer.
		 */
		if (selReq->reply_proc == NULL) {
			xv_sel_free_property(replyInfo->sri_srv, dpy, replyInfo->sri_property);
			return FALSE;
		}

		if (ProcessMultiple(selReq, replyInfo, ev, 1))
			return TRUE;

	}

	/* Is this condition EVER true???
	 * The event's target is probably never INCR
	 */
	if (ev->target == replyInfo->sri_incr) {
		char buf[500];

		sprintf(buf, "UNEXPECTED: %s-%d: target %ld == INCR ????", __FUNCTION__,
							__LINE__, ev->target);
		xv_error(XV_NULL,
				ERROR_STRING, buf,
				ERROR_PKG, SELECTION,
				NULL);
		/*
		 * If we are processing a blocking request and the selection
		 * owner is transferring the data in increments with no reply_proc
		 * registered, reject the data transfer.
		 */
		if (selReq->reply_proc == NULL) {
			xv_sel_free_property(replyInfo->sri_srv, dpy, replyInfo->sri_property);
			return FALSE;
		}

		if (internalProcessIncr(selReq,replyInfo,*replyInfo->sri_target, ev)) {
			return TRUE;
		}
	}
	return XvGetRequestedValue(selReq, ev, replyInfo, ev->property,
			*replyInfo->sri_target, 1);
}

static Xv_opaque SelBlockReq(Sel_req_info *selReq, unsigned long *length,
								int *format)
{
	Selection_requestor requestor;
	Sel_reply_info *reply = NULL;
	struct timeval *time;
	Xv_window win;
	Display *dpy;
	Time xt;
	XID xid;

	/* get the requestor's xid, event time, selection and the target */
	requestor = SEL_REQUESTOR_PUBLIC(selReq);
	win = (Xv_window) xv_get(requestor, XV_OWNER);
	xid = (XID) xv_get(win, XV_XID);
	dpy = XV_DISPLAY_FROM_WINDOW(win);

	/* get the time;if it hasn't been set, set it */
	time = (struct timeval *)xv_get(requestor, SEL_TIME);

	xt = xv_sel_cvt_timeval_to_xtime(time);

	if (xt == 0) {
		Xv_server srv = XV_SERVER_FROM_WINDOW(win);
        struct timeval tv;

		xt = xv_sel_get_last_event_time(srv, dpy, xid);
		xv_sel_cvt_xtime_to_timeval(xt, &tv);
		xv_set(requestor, SEL_TIME, &tv, NULL);
	}
	time->tv_sec = 0;
	time->tv_usec = 0;
	if (GetSelection(dpy, xid, selReq, &reply, xt)) {
		Xv_opaque data;

		if (reply->sri_during_incr || reply->sri_multiple_count) {
			*length = XV_OK;
			*format = reply->sri_format;
			xv_set(requestor, SEL_TIME, time, NULL);
			xv_free(reply->sri_target);
			xv_free(reply);
			return XV_NULL;
		}
		*length = reply->sri_length;
		*format = reply->sri_format;
		xv_set(requestor, SEL_TIME, time, NULL);
		if (selReq->auto_collect_incr && selReq->incr_collector) {
			data = (Xv_opaque)selReq->incr_collector;
			selReq->incr_collector = NULL;
			selReq->auto_collect_incr = FALSE; /* only for one request */
		}
		else {
			data = reply->sri_propdata;
		}
		xv_free(reply->sri_target);
		xv_free(reply);
		return data;
	}
	xv_set(requestor, SEL_TIME, time, NULL);
	*length = SEL_ERROR;
	*format = 0;
	if (reply->sri_target) xv_free(reply->sri_target);
	if (reply) xv_free(reply);
	return XV_NULL;
}

static Xv_opaque sel_req_get_attr(Selection_requestor self,
					int *status, Attr_attribute attr, va_list valist)
{
	Sel_req_info *sel_req = SEL_REQUESTOR_PRIVATE(self);
	unsigned long *arg;
	int i;
	static Atom *types = NULL;
	static char **typeNames = NULL;

	switch (attr) {
		case SEL_DATA:
			/* Initiate a blocking request */
			sel_req->incr_collector	= NULL;
			arg = va_arg(valist, unsigned long *);
			return (Xv_opaque) SelBlockReq(sel_req, arg, va_arg(valist, int *));

		case SEL_REPLY_PROC:
			return (Xv_opaque) sel_req->reply_proc;
		case XV_XID:
			return (Xv_opaque) SelGetOwnerXID(sel_req);
		case SEL_TYPE:
			return (Xv_opaque) sel_req->typeTbl[0].type;
		case SEL_TYPES:
			if (types != NULL)
				XFree((char *)types);
			types = (Atom *) xv_calloc((unsigned)sel_req->nbr_types + 1,
					(unsigned)sizeof(Atom));
			for (i = 0; i < sel_req->nbr_types; i++)
				types[i] = sel_req->typeTbl[i].type;
			types[i] = 0;	/* NULL terminate the list */
			return (Xv_opaque) types;
		case SEL_TYPE_NAME:
			return (Xv_opaque) sel_req->typeTbl[0].type_name;
		case SEL_TYPE_NAMES:
			if (typeNames != NULL)
				XFree((char *)typeNames);
			typeNames =
					(char **)xv_malloc((sel_req->nbr_types +
							1) * sizeof(char *));
			for (i = 0; i < sel_req->nbr_types; i++)
				typeNames[i] = (char *)sel_req->typeTbl[i].type_name;
			typeNames[i] = 0;	/* Null terminate the list */
			return (Xv_opaque) typeNames;
		case SEL_AUTO_COLLECT_INCR:
			return (Xv_opaque)sel_req->auto_collect_incr;
		case SEL_IS_WORD_SELECTION:
			return (Xv_opaque)selection_is_word(self);
		default:
			*status = XV_ERROR;
			return (Xv_opaque) 0;
	}
}


static int sel_req_destroy(Selection_requestor sel_req_public, Destroy_status status)
{
    Sel_req_info   *selReq = SEL_REQUESTOR_PRIVATE(sel_req_public);
	int i;

    if (status == DESTROY_CHECKING
		|| status == DESTROY_SAVE_YOURSELF
        || status == DESTROY_PROCESS_DEATH) return XV_OK;

    /* Free up malloc'ed storage */
	for (i = 0; i < selReq->nbr_types; i++) {
		Sel_type_tbl *tbl = selReq->typeTbl + i;
    	Sel_prop_info  *pi = tbl->propInfo;

	    if (pi) {
			if (pi->data) xv_free(pi->data);
	    	xv_free(pi);
		}
	}
    XFree( (char *) selReq->typeTbl );
    XFree( (char *) selReq );

    return XV_OK;
}

static int ProcessNonBlkIncr(Sel_req_info *selReq, Sel_reply_info *reply,
								XSelectionEvent *ev, Atom target)
{
	unsigned long length;
	unsigned long bytesafter;
	unsigned char *propValue;
	int format;
	Atom type;

	/*
	 * We should start the transfer process by deleting the (type==INCR)
	 * property and by calling the requestor reply_proc with "type" set to
	 * INCR and replyBuf set to a lower bound on the number of bytes of
	 * data in the selection.
	 */
	if (XGetWindowProperty(ev->display, reply->sri_requestor,
					reply->sri_property, 0L, 10000000L, True, AnyPropertyType,
					&type, &format, &length, &bytesafter,
					(unsigned char **)&propValue) != Success) {
		xv_error(selReq->public_self,
				ERROR_STRING, XV_MSG("XGetWindowProperty Failed"),
				ERROR_PKG, SELECTION,
				NULL);
		xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply,
				*reply->sri_target, ev->send_event);
		return FALSE;
	}

	(*selReq->reply_proc) (SEL_REQUESTOR_PUBLIC(selReq), target,
			type, (Xv_opaque) propValue, length, format);


	return TRUE;
}




/*
* This function initiates and processes request. It sends a SelectionRequest
* event to the selection owner.
* If the number of target is greater than one it sends a SelectionRequest
* to the owner with the target set to MULTIPLE.
*/
static void XvGetSeln(Display *dpy, XID xid, Sel_req_info *selReq, Time time)
{
	Selection_requestor requestor;
	Sel_reply_info *replyInfo;
	Sel_owner_info *sel;
	Atom selection;

	/* get the requestor's xid, event time, selection and the target */
	requestor = SEL_REQUESTOR_PUBLIC(selReq);
	selection = (Atom) xv_get(requestor, SEL_RANK);

	/*
	 * Have we saved any data on this window?
	 */
	sel = xv_sel_find_selection_data(dpy, selection, xid);

	replyInfo = NewReplyInfo(requestor, xid, sel, time, selReq);
	xv_set(requestor, XV_KEY_DATA, selreq_reply_key, replyInfo, NULL);

	if (selReq->nbr_types > 1)	/* target is MULTIPLE */
		SetupMultipleRequest(replyInfo, selReq->nbr_types);


	if (sel && (sel->own == TRUE)) {
		sel->status |= SEL_LOCAL_PROCESS;
		HandleLocalProcess(selReq, replyInfo, FALSE);
		if (replyInfo->sri_target) xv_free(replyInfo->sri_target);
		xv_free(replyInfo);
	}
	else {
		struct itimerval timeout;

		xv_sel_set_reply(replyInfo);

		timeout.it_interval.tv_usec = 99999;	/* linux-bug */
		timeout.it_interval.tv_sec = 0;

		timeout.it_value.tv_usec = 0;
		timeout.it_value.tv_sec = replyInfo->sri_timeout;

		notify_set_itimer_func((Notify_client) replyInfo,
				xv_sel_handle_sel_timeout, ITIMER_REAL, &timeout, ITIMER_NULL);

		/*
		 * Send the SelectionRequest message to the selection owner.
		 */
		XConvertSelection(dpy, selection, *replyInfo->sri_target,
				replyInfo->sri_property, xid, replyInfo->sri_time);
	}
}


Xv_private void xv_sel_handle_selection_notify(XSelectionEvent *ev)
{
	Sel_reply_info *reply;
	Sel_req_info *selReq;
	XWindowAttributes winAttr;

	reply = (Sel_reply_info *) xv_sel_get_reply((XEvent *)ev);

	if (reply == NULL) return;

	if (!CheckSelectionNotify(reply->sri_req_info, reply, ev, 0))
		return;

	selReq = reply->sri_req_info;

	/* Is this condition EVER true???
	 * The event's target is probably never INCR
	 */
	if (ev->target == reply->sri_incr) {
		char buf[500];

		sprintf(buf, "UNEXPECTED: %s-%d: target %ld == INCR ????", __FUNCTION__,
							__LINE__, ev->target);
		xv_error(XV_NULL,
				ERROR_STRING, buf,
				ERROR_PKG, SELECTION,
				NULL);
		reply->sri_during_incr = 1;

		reply->sri_status =
				xv_sel_add_prop_notify_mask(ev->display, reply->sri_requestor,
				&winAttr);

		if (ProcessNonBlkIncr(selReq, reply, ev, *reply->sri_target))
			return;

	}
	if (ev->target==reply->sri_multiple||reply->sri_multiple_count) {
		reply->sri_status =
				xv_sel_add_prop_notify_mask(ev->display, reply->sri_requestor,
				&winAttr);

		if (ProcessMultiple(selReq, reply, ev, 0)) {
			if (!reply->sri_during_incr)
				xv_sel_end_request(reply);
			return;
		}
	}
	reply->sri_during_incr = 0;
	if (XvGetRequestedValue(selReq,ev,reply,ev->property,*reply->sri_target,0))
	{
		if (!reply->sri_during_incr) {
			xv_sel_end_request(reply);
		}
	}
}




static int
CheckPropertyNotify(XPropertyEvent *ev, Sel_reply_info *reply)
{
    int  i;

    if ( !reply->sri_during_incr )
        return FALSE;

    if ( ev->window != reply->sri_requestor )
        return FALSE;

    if ( ev->state != PropertyNewValue )
        return FALSE;

    if ( reply->sri_multiple_count )
        for ( i=0; i < reply->sri_multiple_count; i++ )    {
	    if ( ev->atom == reply->sri_atomPair[i].property )
	        return TRUE;
	}

    if ( ev->atom != reply->sri_property )
        return FALSE;

    return TRUE;
}



static int ProcessReq(Requestor  *req, XPropertyEvent  *ev)
{
	Selection_owner sp;
	Xv_window win;
	Xv_server srv;

	if (ev->window != req->rq_owner->xid || ev->atom != req->rq_property ||
			ev->state != PropertyDelete || ev->time < req->rq_time)
		return FALSE;

	sp = SEL_OWNER_PUBLIC(req->rq_owner);
	win = xv_get(sp, XV_OWNER);
	srv = XV_SERVER_FROM_WINDOW(win);

	xv_set(srv, SERVER_APPL_BUSY, TRUE, XV_NULL, NULL);
	xv_sel_handle_incr(req->rq_owner);
	xv_set(srv, SERVER_APPL_BUSY, FALSE, XV_NULL, NULL);
	return TRUE;
}

static Requestor * SelGetReq(XPropertyEvent *ev)
{
    Requestor  *req;

    if ( reqCtx == 0 )
        reqCtx = XUniqueContext();


    if ( XFindContext( ev->display, ev->atom, reqCtx, (caddr_t *)&req ))
	return (Requestor *) NULL;
    return  req;
}

static int ProcessReply(Sel_reply_info  *reply, XPropertyEvent  *ev)
{
	struct itimerval timeout;
	unsigned long length;
	unsigned long bytesafter;
	unsigned char *propValue;
	Sel_req_info *selReq;
	int format;
	Atom type;
	Atom target;
	int i;

	if (!CheckPropertyNotify(ev, reply))
		return FALSE;

	selReq = reply->sri_req_info;

	/* retrieve the data and delete */
	if (XGetWindowProperty(ev->display, ev->window,
					ev->atom, 0L, 10000000L, True, AnyPropertyType,
					&type, &format, &length, &bytesafter,
					(unsigned char **)&propValue) != Success) {
		xv_error(selReq->public_self,
				ERROR_STRING, XV_MSG("XGetWindowProperty Failed"),
				ERROR_PKG, SELECTION,
				NULL);
		xv_sel_handle_error(SEL_BAD_CONVERSION, selReq, reply,
									*reply->sri_target, TRUE);
		return FALSE;
	}

	if (!type)
		return TRUE;

	target = *reply->sri_target;
	if (reply->sri_multiple_count) {
		for (i = 0; i < reply->sri_multiple_count; i++) {
			if (ev->atom == reply->sri_atomPair[i].property)
				target = reply->sri_atomPair[i].target;
		}
	}

	if (reply->sri_during_incr) {
		(*selReq->reply_proc)(SEL_REQUESTOR_PUBLIC(selReq), target, type,
							(Xv_opaque) propValue, length, format);
	}


	/*
	 * For INCR the time out should be the time between each reply.
	 * We just got a reply reset the timer.
	 */

	timeout.it_interval.tv_usec = 9999999;	/* linux-bug */
	timeout.it_interval.tv_sec = 0;
	timeout.it_value.tv_usec = 0;
	timeout.it_value.tv_sec = reply->sri_timeout;

	(void)notify_set_itimer_func((Notify_client) reply,
			xv_sel_handle_sel_timeout, ITIMER_REAL, &timeout,
			(struct itimerval *)ITIMER_NULL);

	/*
	 * If length is set to zero, we have completed an incr transfer.
	 */
	if (length == 0)
		reply->sri_during_incr--;

	/*
	 * We have completed all the incr replies. Finish this request.
	 */
	if (reply->sri_during_incr == 0) {
		xv_sel_end_request(reply);
	}

	return TRUE;
}

Xv_private int xv_sel_handle_property_notify(XPropertyEvent *ev)
{
    Sel_reply_info   *reply;
    Requestor        *req;

    /*
     * If this event is for the selection requestor, process the owner's
     * reply and return.
     * If it is for the selection owner, process the request.
     */
    reply = (Sel_reply_info *) xv_sel_get_reply((XEvent *)ev );
    if ( reply != NULL )
        return ProcessReply( reply, ev );

    req = (Requestor *) SelGetReq( ev );
    if ( req != NULL )
        return ProcessReq( req, ev );
    return FALSE;
}

int sel_post_req(Selection_requestor sel)
{
    Sel_req_info   *selReq;
    struct timeval *time;
    Display        *dpy;
    Xv_window      win;
    Time           XTime;
    XID            xid;

    /* get the requestor's xid, event time, selection and the target */
    selReq = SEL_REQUESTOR_PRIVATE( sel );
    win = (Xv_window) xv_get( sel, XV_OWNER );
    xid = (XID) xv_get( win, XV_XID );
    dpy = XV_DISPLAY_FROM_WINDOW( win );

    /*
     * If no reply proc is defined, return XV_ERROR.
     */
    if ( selReq->reply_proc == NULL )
        return XV_ERROR;

    /* get the time;if it hasn't been set, set it */
    time = (struct timeval *) xv_get( sel, SEL_TIME );

    XTime = xv_sel_cvt_timeval_to_xtime( time );

    if ( XTime == 0 )  {
		Xv_server srv = XV_SERVER_FROM_WINDOW(win);
        struct timeval tv;

		XTime = xv_sel_get_last_event_time(srv, dpy, xid );
		xv_sel_cvt_xtime_to_timeval(XTime, &tv);
		xv_set(sel, SEL_TIME, &tv, NULL);
    }
    XvGetSeln( dpy, xid, selReq, XTime);
    time->tv_sec = 0;
    time->tv_usec = 0;
    xv_set( sel, SEL_TIME, time, NULL );
    return XV_OK;
}

const Xv_pkg xv_sel_requestor_pkg = {
    "Selection Requestor",
    ATTR_PKG_SELECTION,
    sizeof(Xv_sel_requestor),
    SELECTION,
    sel_req_init,
    sel_req_set_avlist,
    sel_req_get_attr,
    sel_req_destroy,
    NULL			/* no find proc */
};
