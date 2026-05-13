#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)fm_rodata.c 1.6 93/06/28 DRA: $Id: fm_rodata.c,v 4.2 2026/04/18 08:12:36 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/* This file is compiled with the -R flag.
 * This ensures that  all data in this file is moved to the text segment
 * of the shared library. So,this data is strictly read-only.
 */

#include <xview_private/portable.h>

