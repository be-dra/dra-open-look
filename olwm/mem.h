/* @(#) %M% V%I% %E% %U% $Id: mem.h,v 1.3 2025/06/20 20:37:08 dra Exp $ */
/* #ident	"@(#)mem.h	26.15	93/06/28 SMI" */

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc.
 */

/*
 *      Sun design patents pending in the U.S. and foreign countries. See
 *      LEGAL_NOTICE file for terms of the license.
 */

#ifndef _OLWM_MEM_H
#define _OLWM_MEM_H

extern void *MemAlloc(unsigned int sz);	/* malloc frontend */
extern void * MemCalloc(unsigned int num, unsigned int sz); /*calloc frontend */
extern void MemFree(void   *p); /* free frontend */
extern void *MemRealloc(void *p, unsigned int sz); /* realloc frontend */

#ifdef MEMDEBUG
extern void *d_MemAlloc();
extern void d_MemFree();
extern void *d_MemRealloc();
extern void *d_MemCalloc();

#define MemAlloc(s)	d_MemAlloc((s), __FILE__, __LINE__, NULL)
#define MemCalloc(n,s)	d_MemCalloc((n),(s), __FILE__, __LINE__)
#define MemFree(p)	d_MemFree(p)
#define MemRealloc(p,s)	d_MemRealloc((p),(s))

#define MemNew(X) 	d_MemAlloc(sizeof(X), __FILE__, __LINE__, #X)
#define MemNewString(s) (strcpy(d_MemAlloc(strlen(s)+1,__FILE__,__LINE__,"(string)"),s))

extern int MemAcct;
extern int AcctTag;
#else
#define MemNew(t) ((t *)MemAlloc((unsigned int)sizeof(t)))
#define MemNewString(s) (strcpy((char *)MemAlloc(strlen(s)+1),s))
#endif /* MEMDEBUG */

#ifdef OW_I18N_L4

#define MemNewText(s)	wscpy((wchar_t *)MemAlloc((wslen(s)+1)*sizeof(wchar_t)),s)

#else

#define MemNewText(s)	MemNewString((char *)s)

#endif /* OW_I18N_L4 */

#endif /* _OLWM_MEM_H */
