/*	@(#)cms_mono.h 20.12 93/06/28 SMI	DRA: RCS: $Id: cms_mono.h,v 4.1 2024/03/28 18:05:27 dra Exp $ */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Definition of the colormap segment CMS_MONOCHROME,
 * a black and white color map segment.
 */

#ifndef xview_cms_mono_DEFINED
#define xview_cms_mono_DEFINED

/*
 ***********************************************************************
 *			Definitions and Macros
 ***********************************************************************
 */

/*
 * PUBLIC #defines 
 */
#define	CMS_MONOCHROME		"monochrome"
#define	CMS_MONOCHROMESIZE	2

#define	WHITE			0
#define	BLACK			1

#define	cms_monochromeload(r,g,b) \
	(r)[WHITE] = -1;(g)[WHITE] = -1;(b)[WHITE] = -1; \
	(r)[BLACK] = 0;(g)[BLACK] = 0;(b)[BLACK] = 0; 


#endif /* ~xview_cms_mono_DEFINED */
