#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)ntfy_dump.c 20.12 93/06/28 Copyr 1985 Sun Micro  DRA: $Id: ntfy_dump.c,v 4.2 2025/03/29 21:01:30 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Ntfy_dump.c - Calls to dump notifier state.
 */

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <xview_private/ndis.h>
#include <stdio.h>		/* For output */

typedef struct ntfy_dump_data {
    Notify_dump_type type;
    Notify_client   nclient;
    FILE           *file;
    NTFY_CLIENT    *client_latest;
}               Ntfy_dump_data;

static NTFY_ENUM ntfy_dump(NTFY_CLIENT *client, NTFY_CONDITION *cond,
						NTFY_ENUM_DATA context);

extern void
notify_dump(nclient, type, file)
    Notify_client   nclient;
    Notify_dump_type type;
    FILE           *file;
{
    Ntfy_dump_data  data;

    /* Set up enumeration record */
    data.nclient = nclient;
    if (file == (FILE *) 1)
	file = stdout;
    if (file == (FILE *) 2)
	file = stderr;
    data.file = file;

    if (type == NOTIFY_ALL || type == NOTIFY_DETECT) {
	(void) fprintf(file, "DETECTOR CONDITIONS:\n");
	data.type = NOTIFY_DETECT;
	data.client_latest = NTFY_CLIENT_NULL;
	(void) ntfy_enum_conditions(ndet_clients, ntfy_dump,
				    (NTFY_ENUM_DATA) & data);
    }
    if (type == NOTIFY_ALL || type == NOTIFY_DISPATCH) {
	(void) fprintf(file, "DISPATCH CONDITIONS:\n");
	data.type = NOTIFY_DISPATCH;
	data.client_latest = NTFY_CLIENT_NULL;
	(void) ntfy_enum_conditions(ndis_clients, ntfy_dump,
				    (NTFY_ENUM_DATA) & data);
    }
    return;
}

static NTFY_ENUM ntfy_dump(NTFY_CLIENT *client, NTFY_CONDITION *cond,
						NTFY_ENUM_DATA context)
{
	register Ntfy_dump_data *data = (Ntfy_dump_data *) context;

	if (data->nclient && client->nclient != data->nclient)
		return (NTFY_ENUM_NEXT);
	if (data->client_latest != client) {
		(void)fprintf(data->file, "Client handle %lx, prioritizer %p",
				client->nclient, client->prioritizer);
		if (data->type == NOTIFY_DISPATCH &&
				client->flags & NCLT_EVENT_PROCESSING)
			(void)fprintf(data->file, " (in middle of dispatch):\n");
		else
			(void)fprintf(data->file, ":\n");
		data->client_latest = client;
	}
	(void)fprintf(data->file, "\t");
	switch (cond->type) {
		case NTFY_INPUT:
			fprintf(data->file, "input pending on fd %d", cond->data.fd);
			break;
		case NTFY_OUTPUT:
			fprintf(data->file, "output completed on fd %d", cond->data.fd);
			break;
		case NTFY_EXCEPTION:
			fprintf(data->file, "exception occured on fd %d", cond->data.fd);
			break;
		case NTFY_SYNC_SIGNAL:
			fprintf(data->file, "signal (synchronous) %d", cond->data.signal);
			break;
		case NTFY_ASYNC_SIGNAL:
			fprintf(data->file, "signal (asynchronous) %d", cond->data.signal);
			break;
		case NTFY_REAL_ITIMER:
			fprintf(data->file, "interval timer (real time) ");
			if (data->type == NOTIFY_DETECT) {
				fprintf(data->file, "waiting (%p)",
						cond->data.ntfy_itimer);
			}
			else
				fprintf(data->file, "expired");
			break;
		case NTFY_VIRTUAL_ITIMER:
			fprintf(data->file, "interval timer (virtual time) ");
			if (data->type == NOTIFY_DETECT) {
				fprintf(data->file, "waiting (%p)",
						cond->data.ntfy_itimer);
			}
			else
				fprintf(data->file, "expired");
			break;
		case NTFY_WAIT3:
			if (data->type == NOTIFY_DETECT)
				fprintf(data->file, "wait3 pid %d", cond->data.pid);
			else
				fprintf(data->file, "wait3 pid %d", cond->data.wait3->pid);
			break;
		case NTFY_SAFE_EVENT:
			fprintf(data->file, "event (safe) %lx", cond->data.event);
			break;
		case NTFY_IMMEDIATE_EVENT:
			fprintf(data->file, "event (immediate) %lx",
					cond->data.event);
			break;
		case NTFY_DESTROY:
			fprintf(data->file, "destroy status 0x%x", cond->data.status);
			break;
		default:
			fprintf(data->file, "UNKNOWN %lx", cond->data.an_u_long);
			break;
	}
	/* Copy function list, if appropriate */
	if (cond->func_count > 1) {
		fprintf(data->file, "\n\t\tfunctions: %lx %lx %lx %lx",
				cond->callout.functions[0],
				cond->callout.functions[1],
				cond->callout.functions[2],
				cond->callout.functions[3], cond->callout.functions[4]);
		fprintf(data->file, "\n\t\tfunc count %ld, func next %ld\n",
				cond->func_count, cond->func_next);
	}
	else
		(void)fprintf(data->file, ", func: %lx\n", cond->callout.function);
	if (data->type == NOTIFY_DISPATCH) {
		if (cond->arg && cond->release)
			(void)fprintf(data->file,
					"\targ: %lx, release func: %lx\n",
					cond->arg, cond->release);
		else if (cond->arg)
			(void)fprintf(data->file, "\targ: %lx\n", cond->arg);
		else if (cond->release)
			(void)fprintf(data->file, "\trelease func: %lx\n", cond->release);
	}
	return (NTFY_ENUM_NEXT);
}
