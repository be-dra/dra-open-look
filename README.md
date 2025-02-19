This project contains what I call XView4.0.

It is based on the sources that have been released by Sun Microsystems some thirty years ago.


Features:

· This is a 64-bit version of XView

· It implements all the features of a Level 2 Open Look toolkit.



==============================================

This It consists of (directories)

+++ libolgx

+++ libxview

+++ olwm


==============================================

Main changes since the XView sources were released:


attr:
	attempts to handle the NULL-termination, fighting against C calling
	conventions

cursor:
	creation of a text drag cursor based on the ol cursor font

dnd:
	+ Integration of that Xdnd misfit
	+ Preview-Events modified - they contain the drag source's window id
	  in order to establish a communication

frame:
	+ Implementation of FRAME_PROPS with Settings menu, change bars etc
	+ Quick duplicate on footers
	+ function xv_ol_default_background: can be set as PANEL_BACKGROUND_PROC
	  and is used on frame footers. Implements that control areas behave as
	  specified in the OLSpec: mouse events fall through to the
	  window manager's decoration. Mouse events are sent as ClientMessage
	  to the window manager

help:
	+ Help window closer to OL Spec

menu:
	+ Menu shadow (even transparent)
	+ new package MENU_MIXED_MENU, menu items in a MIXED_MENU must specifiy
	  their MENU_CLASS.
	+ MENU_PREVIEW_PROC (to provide 'Join Views' preview)

notice:
	+ emanation shadow even for non-screen-locking notices 
	+ new NOTICE_LOCK_SCREEN_LOOKING: default TRUE
	+ no need for NOTICE_BUSY_FRAME: they are determined automatically

notifier:
	this was REALLY the main problem - preventing the notifier to work
	properly: there was a 'u_int an_u_int' - I changed this to 
	'u_long an_u_int;'

openwin:
	+ selectable and resizable views

panel:
	+ edit menu for all text fields
	+ new Panel_item classes PANEL_SUBWINDOW and PANEL_ABBREV_WINDOW_BUTTON
	+ attribute PANEL_ITEM_LAYOUT_ROLE

scrollbar:
	+ page oriented
	+ new: SCROLLBAR_SHOW_PAGE and SCROLLBAR_PAGE

server:
	+ SERVER_UI_REGISTRATION_PROC
	+ SERVER_EVENT_IS_DUPLICATE
	+ SERVER_EVENT_IS_CONSTRAIN
	+ SERVER_EVENT_IS_PAN
	+ SERVER_EVENT_IS_SET_DEFAULT
	+ SERVER_SHAPE_AVAILABLE

screen:
	+ SCREEN_OLWM_MANAGED

textsw:
	+ 32/64bit bug fixes especially when the old selection is involved
	+ trying to get rid of that Textsw/Texsw_view mixture
	+ getting rid of the good old selection service (I hate it)

ttysw:
	if I only understood the whole tty things
	+ eliminate the selection service

win:
	+ fixed a bug concerning the SERVER_EXTERNAL_XEVENT_* attributes
	+ fixed a bug concerning the SERVER_EXTENSION_PROC
	+ support for mouse wheel (Button4 and Button5), new actions
	  ACTION_WHEEL_FORWARD and ACTION_WHEEL_BACKWARD
	+ fixed a bug: _OL_TRANSLATE_KEY should use format 32 instead of 16
