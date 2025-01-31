#ifndef lint
#ifdef sccs
static char     sccsid[] = "@(#)win_env.c 20.16 93/06/28 DRA: $Id: win_env.c,v 4.1 2024/03/28 19:28:19 dra Exp $";
#endif
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license.
 */

/*
 * Implement the win_environ.h & win_sig.h interfaces. (see win_ttyenv.c for
 * other functions)
 */

#include <xview/rect.h>
#define _OTHER_WIN_ENV_FUNCTIONS 1
#include <xview/win_env.h>
#include <xview/win_struct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int _we_setstrfromenvironment(char *tag, char *target);

/*
 * Public routines
 */
void we_setparentwindow(char *windevname)
{
    char *str;

    str = (char *)malloc( strlen( WE_PARENT ) + strlen( windevname ) + 3 );
    strcpy( str, WE_PARENT ); strcat( str, "=" ); strcat( str, windevname );
    (void) putenv(str);
}

int we_getparentwindow(char *windevname)
{
    return (_we_setstrfromenvironment(WE_PARENT, windevname));
}
 
void we_setgfxwindow(char *windevname)
{
    char *str; 
 
    str = (char *)malloc( strlen( WE_GFX ) + strlen( windevname ) + 3 ); 
    strcpy( str, WE_GFX ); strcat( str, "=" ); strcat( str, windevname );
    (void) putenv(str);
}

int we_getgfxwindow(char *windevname)
{
    return (_we_setstrfromenvironment(WE_GFX, windevname));
}

void we_setinitdata(struct rect *initialrect, struct rect *initialsavedrect, int iconic) ;

void we_setinitdata(struct rect *initialrect, struct rect *initialsavedrect, int iconic) 
{ 
    static char            rectstr[100]; 
  
    rectstr[0] = '\0'; 
    strcpy( rectstr, WE_INITIALDATA );
    strcat( rectstr, "=" );
    (void) sprintf(rectstr + strlen( rectstr ),
                   "%04d,%04d,%04d,%04d,%04d,%04d,%04d,%04d,%04d", 
                   initialrect->r_left, initialrect->r_top, 
                   initialrect->r_width, initialrect->r_height, 
                   initialsavedrect->r_left, initialsavedrect->r_top, 
                   initialsavedrect->r_width, initialsavedrect->r_height, 
                   iconic); 
    (void) putenv(rectstr); 
}       
  
int we_getinitdata(struct rect *initialrect, struct rect *initialsavedrect, int *iconic);

int we_getinitdata(struct rect *initialrect, struct rect *initialsavedrect, int *iconic) 
{ 
    char            rectstr[60]; 
  
    if (_we_setstrfromenvironment(WE_INITIALDATA, rectstr)) 
        return (-1); 
    else { 
        if (sscanf(rectstr, "%d,%d,%hd,%hd,%d,%d,%hd,%hd,%d", 
                   &initialrect->r_left, &initialrect->r_top, 
                   &initialrect->r_width, &initialrect->r_height, 
                   &initialsavedrect->r_left, &initialsavedrect->r_top, 
                   &initialsavedrect->r_width, &initialsavedrect->r_height, 
                   iconic) != 9) 
            return (-1); 
        return (0); 
    }    
}
 
int _we_setstrfromenvironment(char *tag, char *target)
{
	char *en_str;

	*target = 0;
	if ((en_str = getenv(tag))) {
		(void)strcat(target, en_str);
		return (0);
	}
	else {
		return (-1);
	}
}
