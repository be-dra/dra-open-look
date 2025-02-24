/*	@(#)text.h 20.13 93/06/28 SMI  DRA: $Id: text.h,v 4.1 2024/03/28 19:06:00 dra Exp $	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * The entire contents of this file are for
 * SunView 1 compatibility only -- THIS IS GOING AWAY 
 */

#ifndef xview_text_DEFINED
#define xview_text_DEFINED

/*
 ***********************************************************************
 *			Include Files
 ***********************************************************************
 */

#include <xview/textsw.h>

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */

#define TEXT_TYPE 	ATTR_PKG_TEXTSW
#define TEXT 		textsw_window_object, WIN_COMPATIBILITY

/*
 ***********************************************************************
 *			Typedefs
 ***********************************************************************
 */

typedef Textsw Text;

#endif /* ~xview_text_DEFINED  */
