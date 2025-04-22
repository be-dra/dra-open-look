XView4.0
========

Introduction
------------

This project contains what I call XView4.0.

It is based on the sources that have been released by Sun Microsystems some thirty years ago.


Features:

* This is a 64-bit version of XView

* It implements (probably) all the features of a Level 2 Open Look toolkit.



Contents
--------

It consists of (directories) with the sources for

* libolgx

* libxview

* olwm


Apart from the sources there are also files ow_noarch_\*.tar.xz, ow_aarch64_\*.tar.xz and ow_x86_64_\*.tar.xz.

They contain pretty much everything from my /usr/openwin: especially the libraries and quite a few applications like

* fileman
* calman
* mailtool
* olws_props
* texted
* vkbd

etc.

Depending on your machine type, you need 
ow_noarch_\*.tar.xz and either ow_x86_64_\*.tar.xz or ow_aarch64\_*.tar.xz.

If you download these files and unpack them (they contain relative path names like "openwin/lib64/libxview.so.4.0", so, it is best to unpack them under /usr so that you have a default installation under /usr/openwin), you may want to do the following:

cd /usr/share/xsessions  
ln -s /usr/openwin/etc/openwin.desktop .

Create files under /etc/profile.d:

echo 'export OPENWINHOME=/usr/openwin' > ow.sh  
echo 'setenv OPENWINHOME /usr/openwin' > ow.csh  

Create a file under /etc/ld.so.conf.d:

echo /usr/openwin/lib64 > ow.conf  
echo /usr/openwin/lib >> ow.conf  

If you do not want to use /usr/openwin, then, of course, you'll have to modify those commands accordingly.


libxview
--------

Main changes since the XView sources were released:


attr:
* attempts to handle the NULL-termination, fighting against C calling conventions (at least on aarch64)

cursor:
* creation of a text drag cursor based on the ol cursor font

dnd:
* Integration of Xdnd
* Preview-Events modified - they contain the drag source's window id in order to establish a communication

frame:
* Implementation of FRAME_PROPS with Settings menu, change bars etc
* Quick duplicate on footers
* function xv_ol_default_background: can be set as PANEL_BACKGROUND_PROC and is used on frame footers. Implements that control areas behave as specified in the OLSpec: mouse events fall through to the window manager's decoration. Mouse events are sent as ClientMessage to the window manager
* New attribute FRAME_FOOTER_PROC so that applications can create their nonstandard footers (multiline or similar)

help:
* Help window layout closer to OL Spec

menu:
* Menu shadow (even transparent)
* new class MENU_MIXED_MENU, menu items in a MIXED_MENU must specifiy their MENU_CLASS.
* MENU_PREVIEW_PROC (to provide 'Join Views' preview)

notice:
* emanation shadow even for non-screen-locking notices 
* new NOTICE_LOCK_SCREEN_LOOKING: default TRUE
* no need for NOTICE_BUSY_FRAME: they are determined automatically

notifier:
	this was REALLY the main problem - preventing the notifier to work
	properly: there was a 'u_int an_u_int' - I changed this to 
	'u_long an_u_int;'

openwin:
* selectable and resizable views

panel:
* edit menu for all text fields
* new Panel_item classes PANEL_SUBWINDOW and PANEL_ABBREV_WINDOW_BUTTON
* attribute PANEL_ITEM_LAYOUT_ROLE
* Quick duplicate on PANEL_LABEL_STRING
* Quick duplicate in PANEL_LISTs

scrollbar:
* page oriented
* new: SCROLLBAR_SHOW_PAGE and SCROLLBAR_PAGE

server:
* SERVER_UI_REGISTRATION_PROC
* SERVER_EVENT_IS_DUPLICATE
* SERVER_EVENT_IS_CONSTRAIN
* SERVER_EVENT_IS_PAN
* SERVER_EVENT_IS_SET_DEFAULT
* SERVER_SHAPE_AVAILABLE

screen:
* SCREEN_OLWM_MANAGED

textsw:
* 32/64bit bug fixes especially when the old selection is involved
* trying to get rid of that Textsw/Texsw_view mixture
* getting rid of the good old selection service (I hate it)

ttysw:
	if I only understood the whole tty things
* eliminate the selection service

win:
* fixed a bug concerning the SERVER_EXTERNAL_XEVENT_* attributes
* fixed a bug concerning the SERVER_EXTENSION_PROC
* support for mouse wheel (Button4 and Button5), new actions ACTION_WHEEL_FORWARD and ACTION_WHEEL_BACKWARD
* fixed a bug: _OL_TRANSLATE_KEY client message should use format 32 instead of 16
