#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)xv_version.h 1.22 93/06/28DRA: RCS: $Id: xv_version.h,v 4.1 2024/03/28 18:05:27 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *      file for terms of the license.
 */

#ifndef xview_xv_version_DEFINED
#define xview_xv_version_DEFINED

 /*
  *  XView version number:
  *
  *  thousands digit signifies major release number
  *  hundreds digit signifies minor release number
  *  tens digit signifies dotdot release number
  *  ones digit signifies patch or specials release number
  *
  */

  /* For compatibility with old version variables */

#define xv_version_number xview_version
#define xv_version_string xv_version

  /* REMIND: this will need to be bumped with each release of XView */

#define XV_VERSION_NUMBER 4000
#define XV_VERSION_STRING "XView Version 4.0"

extern unsigned short xview_version;
extern char *xv_version;

#endif /* ~xview_xv_version_DEFINED */
