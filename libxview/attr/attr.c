#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)attr.c 20.24 90/12/04  DRA: $Id: attr.c,v 4.22 2025/03/12 19:52:18 dra Exp $ ";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

#include <stdio.h>
#include <stdint.h>
#include <libintl.h>
#include <xview_private/i18n_impl.h>
#include <xview/pkg_public.h>
#include <xview/xv_error.h>
#include <xview/server.h>
#include <xview_private/attr_impl.h>
#ifdef SVR4
#include <stdlib.h>
#endif  /* SVR4 */
#include <stdarg.h>
#include <sys/types.h>

Xv_private FILE *xv_help_find_file(Xv_server srv, const char *);

#ifdef UNUSED
static Attr_avlist attr_make_count(Attr_avlist listhead, int listlen, va_list valist, int *countptr)
{
    return copy_va_to_av(valist, listhead, 0);
}
#endif

char *(*attr_converter)(Attr_attribute) = NULL;

/*
 * attr_name - return a pointer to a string containing the name of
 * the specified attribute, or its hex value if not found.
 */
char *attr_name(Attr_attribute attr)
{
	static char attr_name_buf[200];	/* to hold name of attribute */
	static FILE *file_ptr = NULL;
	int attr_value;
	int found = FALSE;

	attr_name_buf[0] = 0;
	if (! file_ptr) {
		file_ptr = xv_help_find_file(xv_default_server, "attr_names");
	}
	else {
		rewind(file_ptr);
	}
	if (file_ptr) {
		while (fscanf(file_ptr, "%x %s\n", &attr_value, attr_name_buf) != EOF) {
			if (attr_value == attr) {
/* 				fclose(file_ptr); */
				return attr_name_buf;
			}
		}
/* 		fclose(file_ptr); */
	}
	if (!found) {
		if (attr_converter) {
			char *f;
			f = (*attr_converter)(attr);
			if (f) {
				strcpy(attr_name_buf, f);
				found = TRUE;
			}
		}
		if (! found) sprintf(attr_name_buf, "attr # 0x%08lx", attr);
	}
	return attr_name_buf;
}

static Attr_attribute trunc_attr(Attr_attribute attr)
{
#ifdef __LP64__
#  ifdef __aarch64__
	/* the whole idiot business is only needed here */

	/* das ist auch unter ARM definiert */
	uint32_t rhwng;
	/* attr wurde von va_arg 'vom Stack' als Attr_attribute geholt, und das
	 * ist ein 64-Bit-Typ, naemlich unsigned long.
	 * Aber die tatsaechlichen Attribute sind enum-Typen, und die sind
	 * 32-Bit-Typen. Also wird sie der Aufrufer auch nur als 32-Bit-Groesse
	 * uebergeben, d.h. die 'oberen' 32 Bits sind irgendwelches Zeug.
	 * Da die Definition dieser Typen aus der 32-Bit-Welt stammt, wird es
	 * auch nichts ausmachen, wenn wir das durch die 32-Bit-Muehle quetschen.
	 */
	/* eigentlich ist dieses ifdef __LP64__ ueberfluessig */
	/* das ganze hier machen wir nur, um den eine oder anderen Aufruf,
	 * der nur mit 0 statt mit NULL terminiert ist, trotzdem zu
	 * ueberleben....
	 * ... die echten Terminatoren erkenne wir ja mittlerweile durch
	 * das _X_SENTINEL, aber da gibt es ja auch noch die Geschichten
	 * mit ATTR_RECURSIVE etc....
	 */
	/* attr was fetched "from the stack" by va_arg as an Attr_attribute which
	 * is a 64bit unsigned long.
	 * But the actual attributes are 32bit enum types. So the caller will
	 * give them as 32bit entities, so the upper 32bit are some garbage.
	 */
	rhwng = (uint32_t)attr;
	return (Attr_attribute)rhwng;
#  else /* __aarch64__ */
	return attr;
#  endif /* __aarch64__ */
#else /* __LP64__ */
	return attr;
#endif /* __LP64__ */
}

#ifdef __LP64__
#ifdef __aarch64__
static char *bastypnam(Attr_attribute attr)
{
	Attr_base_type basetype = ATTR_BASE_TYPE(attr);

	switch (basetype) {
		case ATTR_BASE_AV: return "AV";
		case ATTR_BASE_BOOLEAN: return "BOOLEAN";
		case ATTR_BASE_CHAR: return "CHAR";
		case ATTR_BASE_CURSOR_PTR: return "CURSOR_PTR";
		case ATTR_BASE_ENUM: return "ENUM";
		case ATTR_BASE_FUNCTION_PTR: return "FUNCTION_PTR";
		case ATTR_BASE_ICON_PTR: return "ICON_PTR";
		case ATTR_BASE_INDEX_X: return "INDEX_X";
		case ATTR_BASE_INDEX_XY: return "INDEX_XY";
		case ATTR_BASE_INDEX_Y: return "INDEX_Y";
		case ATTR_BASE_INT: return "INT";
		case ATTR_BASE_LONG: return "LONG";
		case ATTR_BASE_NO_VALUE: return "NO_VALUE";
		case ATTR_BASE_OPAQUE: return "OPAQUE";
		case ATTR_BASE_PIXFONT_PTR: return "PIXFONT_PTR";
		case ATTR_BASE_PIXRECT_PTR: return "PIXRECT_PTR";
		case ATTR_BASE_RECT_PTR: return "RECT_PTR";
		case ATTR_BASE_SHORT: return "SHORT";
		case ATTR_BASE_SINGLE_COLOR_PTR: return "SINGLE_COLOR_PTR";
		case ATTR_BASE_STRING: return "STRING";
		case ATTR_BASE_X: return "X";
		case ATTR_BASE_XY: return "XY";
		case ATTR_BASE_Y: return "Y";
		default: break;
	}
	return "??";
}
#endif /* __aarch64__ */
#endif /* __LP64__ */

static Attr_attribute check_arg(Attr_attribute arg, Attr_attribute attr,
								int expect_nullterm, int idx)
{
#ifdef __LP64__
#  ifdef __aarch64__
	static int verbosity = -1; /* 0 still, 1 leise, 2 laut, 3 laut und blast */
	Attr_base_type basetype = ATTR_BASE_TYPE(attr);
	uint32_t bach, mawr;
	uint16_t bachs, mawrs;

	if (verbosity < 0) {
		char *envverb = getenv("XVIEW_VERBOSITY");

		if (envverb) verbosity = atoi(envverb);
		else verbosity = 0;
	}

	bach = (arg & 0xffffffff);
	mawr = (arg >> 32);
	bachs = (bach & 0xffff);
	mawrs = (bach >> 16);

	/* a few base types should be quite clear: */
	switch (ATTR_BASE_TYPE(attr)) {
		case ATTR_BASE_BOOLEAN: return (Attr_attribute)bach;
		case ATTR_BASE_ENUM: return (Attr_attribute)bach;
		case ATTR_BASE_FUNCTION_PTR: return arg;
		case ATTR_BASE_STRING: return arg;
		case ATTR_BASE_OPAQUE: return arg;
		case ATTR_BASE_NO_VALUE: return arg;
		case ATTR_BASE_PIXRECT_PTR: return arg;
		case ATTR_BASE_PIXFONT_PTR: return arg;
		case ATTR_BASE_RECT_PTR: return arg;
		default:
			break;
	}

	switch (attr) {
		case XV_KEY_DATA: 
			if (idx == 0) return (Attr_attribute)bach;  /* this is the key */
			else return arg;                            /* this is the value */
		case XV_KEY_DATA_REMOVE_PROC: 
			if (idx == 0) return (Attr_attribute)bach;  /* this is the key */
			else return arg;                            /* this is the proc */
		case XV_REF_COUNT: return (Attr_attribute)bach;
#ifdef NOT_NOW
		case 0x49ed0801: /* WIN_BIT_GRAVITY */
		case 0x491c8921: /* WIN_CONSUME_EVENTS */
		case 0x49e20901: /* WIN_SAVE_UNDER */
		case 0x527d0901: /* FRAME_SHOW_FOOTER */
		case 0x4d050921: /* CMS_TYPE */
		case 0x4d0a0801: /* CMS_SIZE */
		case 0x4d460901: /* CMS_DEFAULT_CMS */
		case 0x4d4b0901: /* CMS_FRAME_CMS */
		case 0x61080921: /* SCROLLBAR_DIRECTION */
		case 0x61070901: /* SCROLLBAR_SPLITTABLE */
		case 0x61010801: /* SCROLLBAR_OBJECT_LENGTH */
		case 0x55360901: /* PANEL_INACTIVE */
		case 0x553f0841: /* PANEL_ITEM_X */
			return arg;
#endif /* NOT_NOW */

		default:
			if (verbosity >= 2) {
				fprintf(stderr, "%s-%d: check_arg(arg=%lx, attr=%lx (%s bt %s))\n",
								__FILE__, __LINE__, arg, attr, attr_name(attr),
								bastypnam(attr));
			}
			break;
	}

	if (expect_nullterm) {
		if (mawr != 0) {
			if (bach == 0) {
				fprintf(stderr, "suspicious null-term-value 0x%lx for 0x%lx (%s)\n",
										arg, attr, attr_name(attr));
				if (verbosity >= 3) abort();
				arg = (Attr_attribute)0;
			}
		}
	}

	if (mawr != 0) {
		if (bach == 0) {
			/* das ist aber ein komischer Zufall, dass sich die Nichtnullbits
			 * alle 'oben' versammeln. Wir schauen mal an, was das fuer 
			 * basetypes sind:
			 */
			switch (basetype) {
				case ATTR_BASE_CHAR:
				case ATTR_BASE_BOOLEAN:
				case ATTR_BASE_SHORT:
					/* das kann nur eine 0 sein */
					return (Attr_attribute)0;

				case ATTR_BASE_INT:
					/* das kann nur eine 0 sein */
					return (Attr_attribute)0;
				case ATTR_BASE_ENUM:
					/* das kann nur eine 0 sein */
					return (Attr_attribute)0;

				case ATTR_BASE_X:
				case ATTR_BASE_INDEX_X:
				case ATTR_BASE_Y:
				case ATTR_BASE_INDEX_Y:
				case ATTR_BASE_XY:
				case ATTR_BASE_INDEX_XY:
					/* sieht nach 32 bits aus, wir hoffen,
					 * dass es ungefaehrlich ist
					 */
					if (verbosity) {
						fprintf(stderr,
								"suspicious(1) value 0x%lx for 0x%lx (%s)\n",
											arg, attr, attr_name(attr));
					}
					break;
				default:
					/* der wird wahrscheinlich 64-bit-maessig hingrapschen... */
					if (verbosity) {
						fprintf(stderr,
								"suspicious(2) value 0x%lx for 0x%lx (%s)\n",
											arg, attr, attr_name(attr));
						fprintf(stderr, "basetype = %d, index=%d\n",
										basetype, idx);
					}
					arg = (Attr_attribute)0;
					break;
			}

			if (verbosity >= 3) abort();

		}
		else if (mawrs == 0) {
			char *typ;

			/* d.h. z.B.   7f34 2345 0000 0ea7 */
			switch (basetype) {
				case ATTR_BASE_INT:
					typ = "INT";
					return (Attr_attribute)bachs;
				case ATTR_BASE_ENUM:
					typ = "ENUM";
					return (Attr_attribute)bachs;
					break;
				case ATTR_BASE_X:
					typ = "X";
					return (Attr_attribute)bachs;
					break;
				case ATTR_BASE_INDEX_X:
					typ = "INDEX_X";
					break;
				case ATTR_BASE_Y:
					typ = "Y";
					return (Attr_attribute)bachs;
				case ATTR_BASE_INDEX_Y:
					typ = "INDEX_Y";
					break;
				case ATTR_BASE_XY:
					typ = "XY";
					break;
				case ATTR_BASE_INDEX_XY:
					typ = "INDEX_XY";
					break;
					/* sieht nach 32 bits aus, wir hoffen,
					 * dass es ungefaehrlich ist
					 */
					break;
				default:
					/* der wird wahrscheinlich 64-bit-maessig hingrapschen... */
					return (Attr_attribute)bachs;
					break;
			}

			if (verbosity) {
				fprintf(stderr, "suspicious value 0x%lx (%s) for 0x%lx (%s)\n",
									arg, typ, attr, attr_name(attr));
			}
			if (verbosity >= 3) abort();
		}
	}
#  else /* __aarch64__ */
	return arg;
#  endif /* __aarch64__ */
#endif /* __LP64__ */
	return arg;
}

static void too_long(int maxcnt)
{
	char errbuf[200];

	sprintf(errbuf, 
		XV_MSG("A/V list more than %d elements long, extra elements ignored"),
		maxcnt);
	xv_error(XV_NULL, ERROR_STRING, errbuf, NULL);
}

static int hope_32bits(Attr_attribute attr)
{
#ifdef UNWANTED_SEGFAULT__aarch64__
	/* here the compiler creates different code between (e.g.)
	 * xv_set(obj,
	 *	XV_WIDTH, 100,
	 *	XV_HEIGHT, 200,
	 *	NULL);
	 *
	 * and
	 *
	 * xv_set(obj,
	 *	XV_WIDTH, 100L,
	 *	XV_HEIGHT, 200L,
	 *	NULL);
	 */

	switch (ATTR_BASE_TYPE(attr)) {
		/* these are the types we hope to be 32 bits: */
		case ATTR_BASE_INT:
		case ATTR_BASE_SHORT:
		case ATTR_BASE_ENUM:
		case ATTR_BASE_CHAR:
		case ATTR_BASE_BOOLEAN:
		case ATTR_BASE_X:
		case ATTR_BASE_INDEX_X:
		case ATTR_BASE_Y:
		case ATTR_BASE_INDEX_Y:
		case ATTR_BASE_XY:
		case ATTR_BASE_INDEX_XY:
			return TRUE;
		default:
			break;
	}
#else /* __aarch64__ */
#endif /* __aarch64__ */
	return FALSE;
}

/* copy_va_to_av copies a varargs parameter list into an avlist. If the
   avlist parameter is NULL a new avlist is malloced and returned after
   the varargs list is copied into it. Attr1 exists because ANSI C requires
   that all varargs be preceded by a normal parameter, but some xview
   functions have only a varargs list as their parameters. So those
   functions declare the first vararg parameter as a normal parameter, and
   pass it in to copy_va_to_av as attr1.
*/

Attr_attribute avlist_tmp[ATTR_STANDARD_SIZE];

Xv_private Attr_avlist copy_va_to_av(va_list valist1, Attr_avlist avlist1,
										Attr_attribute attr1)
{
	Attr_attribute attr;
	unsigned cardinality;
	int i;
	static va_list valist;
	static Attr_avlist avlist;
	static int arg_count = 0, arg_count_max = ATTR_STANDARD_SIZE,
			recursion_count = 0;

	recursion_count++;

	/* These two variables are used instead of the paramters so that the
	   position in the lists is maintained after a recursive call.
	 */

#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1)
	__va_copy(valist, valist1);
#else
	valist = valist1;
#endif

	avlist = avlist1;

	if (!avlist)
		avlist = avlist_tmp;

	if (attr1)
		attr = attr1;
	else
		attr = va_arg(valist, Attr_attribute);

	attr = trunc_attr(attr);

	while (attr) {
		if (++arg_count > arg_count_max) {
			too_long(arg_count_max);
			return (avlist1);
		}
		cardinality = ATTR_CARDINALITY(attr);

		switch (ATTR_LIST_TYPE(attr)) {
			case ATTR_NONE:	/* not a list */
				*avlist++ = attr;

				if ((arg_count += cardinality) > arg_count_max) {
					too_long(arg_count_max);
					return (avlist1);
				}
				for (i = 0; i < cardinality; i++) {
					if (hope_32bits(attr))
						*avlist++ =check_arg((Attr_attribute)va_arg(valist,int),
											attr, FALSE, i);
					else *avlist++ = check_arg(va_arg(valist, Attr_attribute),
											attr, FALSE, i);
				}
				break;

			case ATTR_NULL:	/* null terminated list */
				*avlist++ = attr;
				switch (ATTR_LIST_PTR_TYPE(attr)) {
					case ATTR_LIST_IS_INLINE:
						/*
						 * Note that this only checks the first four bytes
						 * for the null termination. Copy each value element
						 * until we have copied the null termination.
						 */
						do {
							if ((arg_count += cardinality) > arg_count_max) {
								too_long(arg_count_max);
								return (avlist1);
							}
							for (i = 0; i < cardinality; i++) {
								if (hope_32bits(attr))
									*avlist++ = check_arg((Attr_attribute)va_arg(valist, int),
														attr, TRUE, i);
								else *avlist++ = check_arg(va_arg(valist,
															Attr_attribute),
														attr, TRUE, i);
							}
						} while (*(avlist - 1));
						break;

					case ATTR_LIST_IS_PTR:
						if (++arg_count > arg_count_max) {
							too_long(arg_count_max);
							return (avlist1);
						}
						if (hope_32bits(attr))
							*avlist++ = check_arg((Attr_attribute)va_arg(valist, int),
											attr, TRUE, 0);
						else *avlist++ =check_arg(va_arg(valist,Attr_attribute),
											attr, TRUE, 0);
						break;
				}
				break;

			case ATTR_COUNTED:	/* counted list */
				*avlist++ = attr;
				switch (ATTR_LIST_PTR_TYPE(attr)) {
					case ATTR_LIST_IS_INLINE:
						{
							register unsigned count;

							if (++arg_count > arg_count_max) {
								too_long(arg_count_max);
								return (avlist1);
							}
							*avlist = check_arg(va_arg(valist, Attr_attribute),
											attr, FALSE, 0); /*copy the count */
							count = ((unsigned)*avlist++) * cardinality;

							if ((arg_count += count) > arg_count_max) {
								too_long(arg_count_max);
								return (avlist1);
							}
							for (i = 0; i < count; i++) {
								*avlist++ = check_arg(va_arg(valist, Attr_attribute),
												attr, FALSE, i);
							}
						}
						break;

					case ATTR_LIST_IS_PTR:
						if (++arg_count > arg_count_max) {
							too_long(arg_count_max);
							return (avlist1);
						}
						*avlist++ = check_arg(va_arg(valist, Attr_attribute),
										attr, FALSE, 0);
						break;
				}
				break;

			case ATTR_RECURSIVE:	/* recursive attribute-value list */
				if (cardinality != 0)	/* don't strip it */
					*avlist++ = attr;

				switch (ATTR_LIST_PTR_TYPE(attr)) {
					case ATTR_LIST_IS_INLINE:
						(void)copy_va_to_av(valist, avlist, (Attr_attribute)0);
						if (cardinality != 0)	/* don't strip it */
							avlist++;	/* move past the null terminator */
						break;

					case ATTR_LIST_IS_PTR:
						if (cardinality != 0) {	/* don't collapse inline */
							if (++arg_count > arg_count_max) {
								too_long(arg_count_max);
								return (avlist1);
							}
							*avlist++ = check_arg(va_arg(valist, Attr_attribute),
											attr, FALSE, 0);
						}
						else {
							attr = check_arg(va_arg(valist, Attr_attribute),
											attr, FALSE, 0);
							if (attr)
								/*
								 * Copy the list inline -- don't move past the null
								 * termintor. Here both the attribute and null
								 * terminator will be stripped away.
								 */
								avlist = attr_copy_avlist(avlist,
										(Attr_avlist) attr);
						}
						break;
				}
				break;
		}

		attr = trunc_attr(va_arg(valist, Attr_attribute));
	}
	va_end(valist);

	*avlist = 0;
	if (!avlist1) {
		unsigned long avlist_size;

		avlist_size = ((avlist - avlist_tmp) + 1) * sizeof(Attr_attribute);
		avlist1 = xv_malloc(avlist_size);
		XV_BCOPY(avlist_tmp, avlist1, avlist_size);
	}

	if (--recursion_count == 0) {
		arg_count = 0;
		arg_count_max = ATTR_STANDARD_SIZE;
	}

	return (avlist1);
}



/*
 * attr_copy_avlist copies the attribute-value list from avlist to dest.
 * Recursive lists are collapsed into dest.
 */

Attr_avlist attr_copy_avlist(Attr_avlist dest, Attr_avlist avlist)
{
    register Attr_attribute attr;
    register unsigned cardinality;

    while ((attr = (Attr_attribute) avlist_get(avlist))) {
	cardinality = ATTR_CARDINALITY(attr);
	switch (ATTR_LIST_TYPE(attr)) {
	  case ATTR_NONE:	/* not a list */
	    *dest++ = attr;
	    avlist_copy_values(avlist, dest, cardinality);
	    break;

	  case ATTR_NULL:	/* null terminated list */
	    *dest++ = attr;
	    switch (ATTR_LIST_PTR_TYPE(attr)) {
	      case ATTR_LIST_IS_INLINE:
		/*
		 * Note that this only checks the first four bytes for the
		 * null termination. Copy each value element until we have
		 * copied the null termination.
		 */
		do {
		    avlist_copy_values(avlist, dest, cardinality);
		} while (*(dest - 1));
		break;

	      case ATTR_LIST_IS_PTR:
		*dest++ = avlist_get(avlist);
		break;
	    }
	    break;

	  case ATTR_COUNTED:	/* counted list */
	    *dest++ = attr;
	    switch (ATTR_LIST_PTR_TYPE(attr)) {
	      case ATTR_LIST_IS_INLINE:{
		    register unsigned count;

		    *dest = avlist_get(avlist);	/* copy the count */
		    count = ((unsigned) *dest++) * cardinality;
		    avlist_copy_values(avlist, dest, count);
		}
		break;

	      case ATTR_LIST_IS_PTR:
		*dest++ = avlist_get(avlist);
		break;
	    }
	    break;

	  case ATTR_RECURSIVE:	/* recursive attribute-value list */
	    if (cardinality != 0)	/* don't strip it */
		*dest++ = attr;

	    switch (ATTR_LIST_PTR_TYPE(attr)) {
	      case ATTR_LIST_IS_INLINE:
		dest = attr_copy_avlist(dest, avlist);
		if (cardinality != 0)	/* don't strip it */
		    dest++;	/* move past the null terminator */
		avlist = attr_skip(attr, avlist);
		break;

	      case ATTR_LIST_IS_PTR:
		if (cardinality != 0)	/* don't collapse inline */
		    *dest++ = avlist_get(avlist);
		else {
		    Attr_avlist     new_avlist = (Attr_avlist)
		    avlist_get(avlist);
		    if (new_avlist)
			/*
			 * Copy the list inline -- don't move past the null
			 * termintor. Here both the attribute and null
			 * terminator will be stripped away.
			 */
			dest = attr_copy_avlist(dest, new_avlist);
		}
		break;
	    }
	    break;
	}
    }
    *dest = 0;
    return (dest);
}


/*
 * attr_count counts the number of slots in the av-list avlist. Recursive
 * lists are counted as being collapsed inline.
 */
int attr_count(Attr_avlist avlist)
{
    /* count the null termination */
    return (attr_count_avlist(avlist, (Attr_attribute) NULL) + 1);
}


int attr_count_avlist(Attr_avlist avlist, Attr_attribute last_attr)
{
    register Attr_attribute attr;
    register unsigned count = 0;
    register unsigned num;
    register unsigned cardinality;

    while ((attr = (Attr_attribute) * avlist++)) {
	count++;		/* count the attribute */
	cardinality = ATTR_CARDINALITY(attr);
	last_attr = attr;
	switch (ATTR_LIST_TYPE(attr)) {
	  case ATTR_NONE:	/* not a list */
	    count += cardinality;
	    avlist += cardinality;
	    break;

	  case ATTR_NULL:	/* null terminated list */
	    switch (ATTR_LIST_PTR_TYPE(attr)) {
	      case ATTR_LIST_IS_INLINE:
		/*
		 * Note that this only checks the first four bytes for the
		 * null termination.
		 */
		while (*avlist++)
		    count++;
		count++;	/* count the null terminator */
		break;

	      case ATTR_LIST_IS_PTR:
		count++;
		avlist++;
		break;
	    }
	    break;

	  case ATTR_COUNTED:	/* counted list */
	    switch (ATTR_LIST_PTR_TYPE(attr)) {
	      case ATTR_LIST_IS_INLINE:
		num = ((unsigned) (*avlist)) * cardinality + 1;
		count += num;
		avlist += num;
		break;
	      case ATTR_LIST_IS_PTR:
		count++;
		avlist++;
		break;
	    }
	    break;

	  case ATTR_RECURSIVE:	/* recursive attribute-value list */
	    if (cardinality == 0)	/* don't include the attribute */
		count--;

	    switch (ATTR_LIST_PTR_TYPE(attr)) {
	      case ATTR_LIST_IS_INLINE:
		count += attr_count_avlist(avlist, attr);
		if (cardinality != 0)	/* count the null terminator */
		    count++;
		avlist = attr_skip(attr, avlist);
		break;

	      case ATTR_LIST_IS_PTR:
		if (cardinality != 0) {	/* don't collapse inline */
		    count++;
		    avlist++;
		} else if (*avlist)
		    /*
		     * Here we count the elements of the recursive list as
		     * being inline. Don't count the null terminator.
		     */
		    count += attr_count_avlist((Attr_avlist) * avlist++,
					       attr);
		else
		    avlist++;
		break;
	    }
	    break;
	}
    }
    return count;
}


/*
 * attr_skip_value returns a pointer to the attribute after the value pointed
 * to by avlist.  attr should be the attribute which describes the value at
 * avlist.
 */
Attr_avlist
attr_skip_value(attr, avlist)
    register Attr_attribute attr;
    register Attr_avlist avlist;
{
    switch (ATTR_LIST_TYPE(attr)) {
      case ATTR_NULL:
	if (ATTR_LIST_PTR_TYPE(attr) == ATTR_LIST_IS_PTR)
	    avlist++;
	else
	    while (*avlist++);
	break;

      case ATTR_RECURSIVE:
	if (ATTR_LIST_PTR_TYPE(attr) == ATTR_LIST_IS_PTR)
	    avlist++;
	else
	    while ((attr = (Attr_attribute) * avlist++))
		avlist = attr_skip_value(attr, avlist);
	break;

      case ATTR_COUNTED:
	if (ATTR_LIST_PTR_TYPE(attr) == ATTR_LIST_IS_PTR)
	    avlist++;
	else
	    avlist += ((int) *avlist) * ATTR_CARDINALITY(attr) + 1;
	break;

      case ATTR_NONE:
	avlist += ATTR_CARDINALITY(attr);
	break;
    }
    return avlist;
}
