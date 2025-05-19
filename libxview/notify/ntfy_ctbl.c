#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)ntfy_ctbl.c 1.22 93/06/28  DRA: $Id: ntfy_ctbl.c,v 4.4 2025/05/19 08:01:57 dra Exp $ ";
#endif
#endif

#include <xview_private/ntfy.h>
#include <xview_private/ndet.h>
#include <stdio.h>
#include <signal.h>

NTFY_CNDTBL *ntfy_cndtbl[NTFY_LAST_CND];

/*
 * Add a client into the condition table (ntfy_cndtbl) for the condition it
 * has an interest in.
 */
/* #define DRA_FIND_TIMER_BUG 1 */
/* #define DUMP_notdef 1 */

#ifdef DRA_FIND_TIMER_BUG
#include <xview/pkg_public.h>
static NTFY_CLIENT *last_same_client = NULL;
static NTFY_CONDITION *last_same_condition = NULL;
#endif /* DRA_FIND_TIMER_BUG */


pkg_private void ntfy_add_to_table(NTFY_CLIENT *client,
							NTFY_CONDITION *condition, int type)
{
	NTFY_CNDTBL *cnd_list = ntfy_cndtbl[type];

#ifdef DRA_FIND_TIMER_BUG
	if (type == NTFY_REAL_ITIMER) {
		if (cnd_list) {
			fprintf(stderr, "%s-%d: add_to_table(%p, %p): head=%p, next=%p\n",
				__FILE__, __LINE__, client, condition,
				cnd_list, cnd_list->next);
		}
		else {
			fprintf(stderr, "%s-%d: add_to_table(%p, %p): head=%p\n",
				__FILE__, __LINE__, client, condition, cnd_list);
		}
	}
#endif /* DRA_FIND_TIMER_BUG */

	NTFY_BEGIN_CRITICAL;
	if (!cnd_list) {
		/* Create the head, which is never used */
		cnd_list = (NTFY_CNDTBL *) xv_malloc(sizeof(NTFY_CNDTBL));
		cnd_list->client = (NTFY_CLIENT *) NULL;
		cnd_list->condition = (NTFY_CONDITION *) NULL;
		cnd_list->next = (NTFY_CNDTBL *) NULL;
		ntfy_cndtbl[type] = cnd_list;

		/*
		 * Create the first clt/cnd in the list, along with ptrs back to the
		 * actual clt and condition
		 */
		cnd_list = (NTFY_CNDTBL *) xv_malloc(sizeof(NTFY_CNDTBL));
		cnd_list->client = client;
		cnd_list->condition = condition;
		cnd_list->next = ntfy_cndtbl[type]->next;
		ntfy_cndtbl[type]->next = cnd_list;
		NTFY_END_CRITICAL;

#ifdef DRA_FIND_TIMER_BUG
		if (type == NTFY_REAL_ITIMER) {
			fprintf(stderr, "first new cond of type %d\n", type);
			fprintf(stderr, "%s-%d: added       : head=%p, next=%p\n",
					__FILE__, __LINE__,
					ntfy_cndtbl[(int)NTFY_REAL_ITIMER],
					ntfy_cndtbl[(int)NTFY_REAL_ITIMER]->next);
		}
#endif /* DRA_FIND_TIMER_BUG */

		return;
	}
	/* See if a particular client already has registered this condition. */
	cnd_list = cnd_list->next;
	while (cnd_list) {
		ntfy_assert(cnd_list->condition->type == condition->type, 25
				/* Found wrong condition type in condition table */ );
		if ((cnd_list->client == client) && (cnd_list->condition == condition)) {
#ifdef DRA_FIND_TIMER_BUG
			if (type == NTFY_REAL_ITIMER) {
				fprintf(stderr, "%s-%d: already here: cl=%p, cond=%p ================\n",
						__FILE__, __LINE__, client, condition);

				/* who is adding the same thing twice? */
				/* answer:
						ntfy_add_to_table (ntfy_ctbl.c:84)
						notify_set_itimer_func (ndetsitimr.c:52)
						ndet_itimer_expired (ndetitimer.c:176)
						ndet_poll_send (ndet_loop.c:967)
						ndet_poll_send (ndet_loop.c:957)
						ntfy_enum_conditions (ntfy_cond.c:221)
						notify_start (ndet_loop.c:412)
				*/

				/* remember: I have not really added anything */
				last_same_client = client;
				last_same_condition = condition;
				{
					Notify_client cl = client->nclient;
					Xv_base *obj = (Xv_base *)cl;

					fprintf(stderr, "%s-%d: %ld = 0x%lx\n", __FILE__, __LINE__, obj->seal, obj->seal);
				}
			}
#endif /* DRA_FIND_TIMER_BUG */
			NTFY_END_CRITICAL;
			return;
		}
#ifdef DRA_FIND_TIMER_BUG
		last_same_client = NULL;
		last_same_condition = NULL;
#endif /* DRA_FIND_TIMER_BUG */
		cnd_list = cnd_list->next;
	}

	cnd_list = (NTFY_CNDTBL *) xv_malloc(sizeof(NTFY_CNDTBL));
	cnd_list->client = client;
	cnd_list->condition = condition;
	cnd_list->next = ntfy_cndtbl[type]->next;
	ntfy_cndtbl[type]->next = cnd_list;

#ifdef DRA_FIND_TIMER_BUG
	if (ntfy_cndtbl[(int)NTFY_REAL_ITIMER] && type == NTFY_REAL_ITIMER) {
		fprintf(stderr, "new cond of type %d\n", type);
		fprintf(stderr, "%s-%d: added       : head=%p, next=%p\n",
				__FILE__, __LINE__,
				ntfy_cndtbl[(int)NTFY_REAL_ITIMER],
				ntfy_cndtbl[(int)NTFY_REAL_ITIMER]->next);
	}
#endif /* DRA_FIND_TIMER_BUG */

	NTFY_END_CRITICAL;
	return;
}

/*
 * Remove a condition interest for a particular client from the condition
 * table.
 */

pkg_private void ntfy_remove_from_table(NTFY_CLIENT *client, NTFY_CONDITION *condition)
{
	NTFY_CNDTBL *cnd_list, *last_cnd;

	if ((int)condition->type >= NTFY_LAST_CND) return;

#ifdef DRA_FIND_TIMER_BUG
	/* remember: I have not really added anything */
	if (last_same_client == client && last_same_condition == condition) {
		fprintf(stderr,
				"%s-%d: remove_from_table(%p, %p): NO!!!\n",
				__FILE__, __LINE__, client, condition);
		last_same_client = NULL;
		last_same_condition = NULL;
		return;
	}

	if (condition->type == NTFY_REAL_ITIMER)
		fprintf(stderr,
				"%s-%d: remove_from_table(%p, %p): head=%p, next=%p\n",
				__FILE__, __LINE__, client, condition,
				ntfy_cndtbl[(int)NTFY_REAL_ITIMER],
				ntfy_cndtbl[(int)NTFY_REAL_ITIMER]->next);
#endif /* DRA_FIND_TIMER_BUG */

	NTFY_BEGIN_CRITICAL;
	cnd_list = last_cnd = ntfy_cndtbl[(int)condition->type];

	ntfy_assert(cnd_list, 26 /* Condition list has a NULL head */ );

	cnd_list = cnd_list->next;
	while (cnd_list) {
		ntfy_assert(cnd_list->condition->type == condition->type, 27
				/* Found wrong condition type in condition table */ );
		if ((cnd_list->client == client) && (cnd_list->condition == condition)) {
			last_cnd->next = cnd_list->next;
			free(cnd_list);
			NTFY_END_CRITICAL;

#ifdef DRA_FIND_TIMER_BUG
			if (condition->type == NTFY_REAL_ITIMER) {
				fprintf(stderr, "%s-%d: remove         : head=%p, next=%p\n",
						__FILE__, __LINE__,
						ntfy_cndtbl[(int)NTFY_REAL_ITIMER],
						ntfy_cndtbl[(int)NTFY_REAL_ITIMER]->next);
			}
#endif /* DRA_FIND_TIMER_BUG */

			return;
		}
		last_cnd = cnd_list;
		cnd_list = cnd_list->next;
	}
	NTFY_END_CRITICAL;
}

pkg_private NTFY_ENUM ntfy_new_enum_conditions(NTFY_CNDTBL *cnd_list,
						NTFY_ENUM_FUNC enum_func, NTFY_ENUM_DATA context)
{
	if (!cnd_list)
		return (NTFY_ENUM_NEXT);

	cnd_list = cnd_list->next;

#ifdef DRA_FIND_TIMER_BUG
	fprintf(stderr, "%s-%d: new_enum_cond: head=%p\n",
			__FILE__, __LINE__, ntfy_cndtbl[(int)NTFY_REAL_ITIMER]);

/* 						ntfy_cndtbl[(int) NTFY_REAL_ITIMER]->next); */
#endif /* DRA_FIND_TIMER_BUG */

#ifdef DUMP_notdef
	if (cnd_list)
		dump_table(cnd_list->condition->type);
#endif

	while (cnd_list) {
#ifdef DRA_FIND_TIMER_BUG
		fprintf(stderr, "%s-%d: --------------  cnd_list=%p\n", __FILE__,
				__LINE__, cnd_list);
#endif /* DRA_FIND_TIMER_BUG */

		switch (enum_func(cnd_list->client, cnd_list->condition, context)) {
			case NTFY_ENUM_SKIP:
#ifdef DRA_FIND_TIMER_BUG
				fprintf(stderr, "%s-%d: NTFY_ENUM_SKIP: head=%p, next=%p\n",
						__FILE__, __LINE__,
						ntfy_cndtbl[(int)NTFY_REAL_ITIMER],
						ntfy_cndtbl[(int)NTFY_REAL_ITIMER]->next);
#endif /* DRA_FIND_TIMER_BUG */
				break;
			case NTFY_ENUM_TERM:
				return (NTFY_ENUM_TERM);
			default:
				break;
		}

#ifdef DRA_FIND_TIMER_BUG
		if ((long)cnd_list->next == 0x10) {
			fprintf(stderr, "\n                       2 gleich krachts\n\n");
			fprintf(stderr, "cnd_list before = %p\n", cnd_list);
		}
		cnd_list = cnd_list->next;
#else /* DRA_FIND_TIMER_BUG */
		/* the problem seems to be that somebody calls ntfy_add_to_table
		 * (maybe from within the 'enum_func' ?!?) for a client/condition pair
		 * that is already there (so no real adding takes place) and immediately
		 * afterwards calls  ntfy_remove_from_table, which really does the
		 * removing - so the current cnd_list is being freed ....
		 * don't know where the 0x10 comes from, but we never saw anything else
		 */
		if ((long)cnd_list->next == 0x10) {  /* HERE !!!! valgrind detects
									* (sometimes) "Invalid read of size 8".
									* For now, I have suppressed it....
									*/
			/* sorry, but this only prevents us from dying here - 
			 * we die in ntfy_new_paranoid_enum_conditions
			 */
			cnd_list = NULL;
		}
		else {
			cnd_list = cnd_list->next;
		}
#endif /* DRA_FIND_TIMER_BUG */
	}
	return (NTFY_ENUM_NEXT);
}

#define NTFY_BEGIN_PARANOID     ntfy_paranoid_count++
#define NTFY_IN_PARANOID        (ntfy_paranoid_count > 0)
#define NTFY_END_PARANOID       ntfy_paranoid_count--;

/* Variables used in paranoid enumerator */
static NTFY_CONDITION *ntfy_enum_condition;
static NTFY_CONDITION *ntfy_enum_condition_next;
static int ntfy_paranoid_count;

pkg_private NTFY_ENUM ntfy_new_paranoid_enum_conditions(NTFY_CNDTBL *cnd_list,
						NTFY_ENUM_FUNC  enum_func, NTFY_ENUM_DATA  context)
{
	extern NTFY_CLIENT *ntfy_enum_client;
	extern NTFY_CLIENT *ntfy_enum_client_next;
	NTFY_ENUM return_code = NTFY_ENUM_NEXT;
	extern sigset_t ndet_sigs_managing;
	sigset_t oldmask, newmask;

	newmask = ndet_sigs_managing;	/* assume interesting < 32 */
	sigprocmask(SIG_BLOCK, &newmask, &oldmask);

	/*
	 * Blocking signals because async signal sender uses this paranoid
	 * enumerator.
	 */

	ntfy_assert(!NTFY_IN_PARANOID, 28
			/* More then 1 paranoid using enumerator */ );
	NTFY_BEGIN_PARANOID;

	if (!cnd_list)
		goto Done;

	cnd_list = cnd_list->next;

#ifdef DUMP_notdef
	if (cnd_list)
		dump_table(cnd_list->condition->type);
#endif

	while (cnd_list) {
		ntfy_enum_client = cnd_list->client;
		ntfy_enum_condition = cnd_list->condition;
		switch (enum_func(ntfy_enum_client, ntfy_enum_condition, context)) {
			case NTFY_ENUM_SKIP:
				break;
			case NTFY_ENUM_TERM:
				return_code = NTFY_ENUM_TERM;
				goto Done;
			default:
				if (ntfy_enum_client == NTFY_CLIENT_NULL)
					goto BreakOut;
		}
		cnd_list = cnd_list->next;
	}
  BreakOut:
	{
	}
  Done:
	/* Reset global state */
	ntfy_enum_client = ntfy_enum_client_next = NTFY_CLIENT_NULL;
	ntfy_enum_condition = ntfy_enum_condition_next = NTFY_CONDITION_NULL;
	NTFY_END_PARANOID;
	sigprocmask(SIG_SETMASK, &oldmask, (sigset_t *) 0);
	return (return_code);
}

#ifdef DUMP_notdef
dump_table(type)
    int             type;
{
    NTFY_CNDTBL    *cnd_list;
    int             i;

    fprintf(stderr, "\n\n");
    fprintf(stderr, "Searching for type %d\n", type);
    for (i = 0; i < NTFY_LAST_CND; i++) {
	if (ntfy_cndtbl[i]) {
	    cnd_list = ntfy_cndtbl[i]->next;
	    fprintf(stderr, "%d: ", i);
	    while (cnd_list) {
		/*
		 * fprintf (stderr, "%d, ", cnd_list->condition->type);
		 */
		fprintf(stderr, "%d (0x%x [0x%x] 0x%x), ", cnd_list->condition->type, cnd_list->client, cnd_list->client->nclient, cnd_list->condition);
		cnd_list = cnd_list->next;
	    }
	    fprintf(stderr, "\n");
	}
    }
    fprintf(stderr, "\n");
}

#endif
