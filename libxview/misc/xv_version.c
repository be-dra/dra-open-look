#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)xv_version.c 1.11 93/06/28 DRA: RCS $Id: xv_version.c,v 4.1 2024/03/28 18:05:27 dra Exp $ ";
#endif
#endif

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *      file for terms of the license.
 */

 /*
  *  XView version number:
  *
  *  thousands digit signifies major release number
  *  hundreds digit signifies minor release number
  *  tens digit signifies patch release number
  *  ones digit signifies release number for specials
  *
  */

unsigned short xview_version;
char *xv_version;
