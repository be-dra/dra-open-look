#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)win_keymap.c 20.20 93/06/28 DRA: $Id: win_keymap.c,v 4.1 2024/03/28 19:28:19 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/******************************************************************************
 *	win_keymap.c -- keymapping system for SunView event mapping
 *
 *    Gobeithio nid defnyddiwyd
 *
 ******************************************************************************/

#define win_keymap_c

#include <stdio.h>
#include <ctype.h>

#include <sys/param.h>		/* max files/process (NOFILE) from here */

#include <sys/types.h>
#include <sys/time.h>

#include <xview/base.h>
#include <xview/generic.h>
#include <xview/defaults.h>
#include <xview/win_input.h>
/* #include <xview_private/bitmask.h> */
#include <xview_private/win_keymap.h>

/*
 * Semantic class definitions
 */
void win_keymap_set_smask_class(Xv_object win, Event_class class)
{

	switch (class) {
		case KEYMAP_FUNCT_KEYS:
			break;
		case KEYMAP_EDIT_KEYS:
			break;
		case KEYMAP_MOTION_KEYS:
			break;
		case KEYMAP_TEXT_KEYS:
			break;
	}
}

#ifdef NOT_USED

static caddr_t Malloc(unsigned s)
{
	caddr_t p;
	char buf[64];

	s = (s) ? s : 1;

	if ((p = (caddr_t) xv_malloc(s)) == 0) {
		(void)sprintf(buf, "win_keymap: Malloc(%d)", s);
		perror(buf);
		exit(1);
	}
	else {
		return p;
	}
}


#define isctrl(c)	(0 <= (c) && (c) <= 0x1f)
#define isprintable(c)	(33 <= (c) && (c) <= 126)
#define isalphabetic(c)	((65<=(c) && (c)<=90)||(97<=(c) && (c)<=122))

static int win_metanormalize(register int c, register unsigned mask)
{
	register int d = c % 128;

	if (d >= 64) {
		if (mask & CTRLMASK)
			return d % 32 + 128;
		else if (mask & SHIFTMASK)
			return d % 32 + 192;
		else
			return d + 128;
	}
	else {
		return d + 128;
	}
}


int             keymap_enable;	/* Public enable flag */
int             keymap_errno;	/* Public errno value */
int             keymap_initialized;	/* Public initialization flag */


static int      keymap_quoted;	/* Keymap quote in-progress flag */



static void win_keymap_set_imask_from_std_bind(Inputmask *mask, unsigned short action)
{
}

static void win_keymap_unset_imask_from_std_bind(Inputmask *mask, unsigned short action)
{
}




static void win_keymap_fault_resolve( Xv_object win)
{
}

static void win_keymap_copy_on_write(Xv_object win)
{
}


/*
 * Semantic bit mask manipulation
 */
static void win_keymap_set_smask(Xv_object win, unsigned short code)
{
}

static void win_keymap_unset_smask(Xv_object win, unsigned short code)
{
}


static void win_keymap_get_smask(Xv_object win, unsigned short code)
{
}

static void win_keymap_set_imask_class(Inputmask *mask, Event_class class)
{
	switch (class) {
		case KEYMAP_FUNCT_KEYS:
			break;
		case KEYMAP_EDIT_KEYS:
			break;
		case KEYMAP_MOTION_KEYS:
			break;
		case KEYMAP_TEXT_KEYS:
			break;
	}
}



/******************************************************************************
 *	win_keymap_enable() -- enable xview keymapping
 ******************************************************************************/

static void win_keymap_enable(void)
{
}

/******************************************************************************
 *	win_keymap_disable() -- disable xview keymapping
 ******************************************************************************/

static void win_keymap_disable(void)
{
}

/******************************************************************************
 *	win_keymap_map(win, event) -- attempt to translate the event into
 *		a keymap mapping; return 1 if translation occurs, 0 if not
 ******************************************************************************/

static Xv_object mapped_win;
static unsigned short mapped_event, mapped_action;

static void win_keymap_map(Xv_object win, Event *event)
{
}





static void win_keymap_show_inputmask(Inputmask *m)
{
}

static void win_keymap_show_keymap( Xv_object win)
{
}


/******************************************************************************
 *	win_keymap_code_and_masks(newevent, masklist, mapping, win) -- take the
 *		event and masklist and map those events into the corresponding mask
 *		and mapping events.  Masklists are -1 terminated.
 ******************************************************************************/
static void win_keymap_map_code_and_masks(int newevent, int masklist[], int mapping, Xv_object win)
{

}

static void win_keymap_unmap_code_and_masks(int newevent, int masklist[], Xv_object win)
{

}
#endif /* NOT_USED */
