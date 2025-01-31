#include <xview/xview.h>
#include <xview/attr.h>
#include <xview/colortext.h>
#include <xview/group.h>
#include <stdint.h>

char *testattr_c_rcsid = "@(#) $Id: testattr.c,v 1.4 2024/11/05 09:11:37 dra Exp $ ";

static void show(Attr_avlist av, int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		printf("%d: %lx\n", i, av[i]);
	}
}

int main(int argc, char **argv)
{
	Attr_avlist result1, result2;

	show(attr_create_list(XV_X, 32L,
					XV_WIDTH, 33L,
					XV_HEIGHT, 34L,
					PANEL_INACTIVE, 35L,
					NULL), 9);

	show(attr_create_list(XV_X, 32,
					XV_WIDTH, 33,
					XV_HEIGHT, 34,
					PANEL_INACTIVE, 35,
					NULL), 9);

	show(attr_create_list(XV_X, 21,
					XV_WIDTH, 500,
					XV_LABEL, "unknown",
					NULL), 7);

	show(attr_create_list(XV_X, 21,
					XV_LABEL, "unknown",
					XV_WIDTH, 500,
					PANEL_COLORTEXT_WINDOW_ATTRS,
						PANEL_CHOICE_STRINGS, "a", "bb", "ccc", NULL,
						PANEL_COLORTEXT_HEADER, "huhu",
						NULL,
					NULL), 16);

	show(attr_create_list(XV_X, 21,
					PANEL_COLORTEXT_WINDOW_ATTRS,
						PANEL_CHOICE_STRINGS, "a", "bb", "ccc", NULL,
						PANEL_COLORTEXT_HEADER, "huhu",
						NULL,
					FRAME_PROPS_CREATE_ITEM,
						FRAME_PROPS_ITEM_SPEC, NULL, FRAME_PROPS_MOVE, PANEL_CHOICE,
						PANEL_LABEL_STRING, "Display:",
						PANEL_CHOICE_STRINGS,
							"All",
							"Long",
							"Interesting",
							NULL,
						XV_HELP_DATA, "XvPlayer:display",
						NULL,
					NULL), 28);

	show(attr_create_list(XV_X, 32,
					XV_WIDTH, 33,
					XV_HEIGHT, 34,
					PANEL_INACTIVE, 35,
					FRAME_SHOW_FOOTER, 36,
					CANVAS_VIEW_MARGIN, 37,
					CANVAS_WIDTH, 38,
					GROUP_ROWS, 39,
					GROUP_COLUMNS, 40,
					GROUP_HORIZONTAL_OFFSET, 41,
					NULL), 21);

	show(attr_create_list(XV_X, 32L,
					XV_WIDTH, 33L,
					XV_HEIGHT, 34L,
					PANEL_INACTIVE, 35L,
					FRAME_SHOW_FOOTER, 36L,
					CANVAS_VIEW_MARGIN, 37L,
					CANVAS_WIDTH, 38L,
					GROUP_ROWS, 39L,
					GROUP_COLUMNS, 40L,
					GROUP_HORIZONTAL_OFFSET, 41L,
					NULL), 21);
}

void variadisch(int lala, ...)
{
	va_list args;
	long l;
	int i;
	char *cp;

	va_start(args, lala);
	l = va_arg(args, long);
	i = va_arg(args, int);
	cp = va_arg(args, char*);
	va_end(args);
}
