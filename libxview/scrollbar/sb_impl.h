/*      @(#)sb_impl.h 1.49 93/06/28 RCS: $Id: sb_impl.h,v 4.7 2026/07/21 06:22:51 dra Exp $ */

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#ifndef	__scrollbar_impl_h
#define	__scrollbar_impl_h

/* $Id: sb_impl.h,v 4.7 2026/07/21 06:22:51 dra Exp $ */
/*
 * Module:	scrollbar_impl.h
 *
 * Level:	private
 *
 * Description: Declarations for data structures internal to the scrollbar
 *
 */

/*
 * Include files: <only those required by THIS file>
 */
#include <xview_private/i18n_impl.h>
#include <xview/scrollbar.h>


/*
 * Package-private Function Declarations:
 */
Xv_private int scrollbar_minimum_size(Scrollbar sb_public);
Xv_private int scrollbar_width_for_owner(Xv_window owner);
Xv_private int scrollbar_width(Scrollbar sb);

#endif	 /* __scrollbar_impl_h */
