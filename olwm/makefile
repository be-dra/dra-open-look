DO_LINK_LIBS=false
NO_COPY_LIB_CMD=true
DO_COPY_LIBS=false
LIBSTARGET=$(HOME)/lib
SHELL=/bin/sh
REMOTE_MACHINES=sun2 rm40 hp1 indy0 alpha1
REMOTE_SHELL=rsh

LDFLAGS=-L$(OPENWINHOME)/lib $(SYSLDFLAGS)
OLLDFLAGS=
XLDFLAGS=
LOLWM=libolwm_$(HOSTMTYPE).a
DEPEND=makedepend

PRECTARGETS=$(LOLWM)

include default_$(HOSTMTYPE)

T=date +%y%m%d%H%M%S >
MV=mv
RM=rm -f

CPPFLAGS=$(SYSDEFINES) -DOW_I18N_L3 -DSHAPE -I../build/include
LDLIBS=$(OLLDFLAGS) -lolgx $(XLDFLAGS) -lXext -lX11 $(SYSLIBS)

COMPILE.c=$(CC) $(CFLAGS) $(CPPFLAGS) -c
LINK.c=$(CC_FOR_LINK) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)


PRINTSRC=owplaces.c toolwait.c

.PRECIOUS: $(PRECTARGETS)

$(KEEP_STATE_TARGET)

.SUFFIXES: .o .i .c .mo_$(HOSTMTYPE) .po

.c.o:
	$(COMPILE.c) $<

.c.a:
	$(COMPILE.c) $<
	$(AR) $(ARFLAGS) $@ $%
	$(RM) $%

.po.mo_$(HOSTMTYPE):
	msgfmt $<
	cp $*.mo $(OPENWINHOME)/lib/locale/de/LC_MESSAGES
	mv $*.mo $@

.c.i:
	$(CC) $(CPPFLAGS) $(C_I_OPT) $< $(C_I_TAIL)

HEADERS=cmdstream.h debug.h defaults.h dsdm.h environ.h   \
	  error.h events.h globals.h group.h helpcmd.h i18n.h iconimage.h \
	  iconmask.h kbdfuncs.h list.h mem.h menu.h notice.h olcursor.h \
	  ollocale.h olwm.h properties.h resources.h atom.h \
	  screen.h selection.h services.h slots.h st.h win.h gettext.h

SRCS=atom.c client.c cmdstream.c debug.c dra_signal.c dra_sel.c \
	  defaults.c dra_color.c dsdm.c environ.c error.c evbind.c events.c \
	  fontset.c group.c helpsend.c i18n.c info.c kbdfuncs.c \
	  list.c mem.c menu.c moveresize.c notice.c olwm.c properties.c\
	  resources.c screen.c selection.c services.c slave.c slots.c \
	  st.c states.c usermenu.c usleep.c win.c winbusy.c \
	  winbutton.c wincolor.c winframe.c wingframe.c \
	  winicon.c winipane.c winmenu.c winnofoc.c \
	  winpane.c winpinmenu.c winpush.c winresize.c \
	  winroot.c gettext.c dra_quick.c

all: olwm.$(HOSTMTYPE) $(MOFILES) $(ADD_ONS)
	$T $@

olwm.$(HOSTMTYPE): $(HEADERS) $(LOLWM)
	$(LINK.c) $(LOLWM) $(LDLIBS) -o $(CMDS)/olwm
	$T $@

$(LOLWM):$(LOLWM)(atom.o) \
         $(LOLWM)(client.o) \
         $(LOLWM)(cmdstream.o) \
         $(LOLWM)(debug.o) \
         $(LOLWM)(defaults.o) \
         $(LOLWM)(dra_color.o) \
         $(LOLWM)(dra_signal.o) \
         $(LOLWM)(dra_sel.o) \
         $(LOLWM)(dra_quick.o) \
         $(LOLWM)(dsdm.o) \
         $(LOLWM)(environ.o) \
         $(LOLWM)(error.o) \
         $(LOLWM)(evbind.o) \
         $(LOLWM)(events.o) \
         $(LOLWM)(fontset.o) \
         $(LOLWM)(gettext.o) \
         $(LOLWM)(group.o) \
         $(LOLWM)(helpsend.o) \
         $(LOLWM)(i18n.o) \
         $(LOLWM)(info.o) \
         $(LOLWM)(kbdfuncs.o) \
         $(LOLWM)(list.o) \
         $(LOLWM)(mem.o) \
         $(LOLWM)(menu.o) \
         $(LOLWM)(moveresize.o) \
         $(LOLWM)(notice.o) \
         $(LOLWM)(olwm.o) \
         $(LOLWM)(properties.o) \
         $(LOLWM)(resources.o) \
         $(LOLWM)(screen.o) \
         $(LOLWM)(selection.o) \
         $(LOLWM)(services.o) \
         $(LOLWM)(slave.o) \
         $(LOLWM)(slots.o) \
         $(LOLWM)(st.o) \
         $(LOLWM)(states.o) \
         $(LOLWM)(usermenu.o) \
         $(LOLWM)(usleep.o) \
         $(LOLWM)(win.o) \
         $(LOLWM)(winbusy.o) \
         $(LOLWM)(winbutton.o) \
         $(LOLWM)(wincolor.o) \
         $(LOLWM)(winframe.o) \
         $(LOLWM)(wingframe.o) \
         $(LOLWM)(winicon.o) \
         $(LOLWM)(winipane.o) \
         $(LOLWM)(winmenu.o) \
         $(LOLWM)(winnofoc.o) \
         $(LOLWM)(winpane.o) \
         $(LOLWM)(winpinmenu.o) \
         $(LOLWM)(winpush.o) \
         $(LOLWM)(winresize.o) \
         $(LOLWM)(winroot.o)
	$(RANLIB) $@

mofiles: olwm_messages.mo_$(HOSTMTYPE)

owplaces.$(HOSTMTYPE): owplaces.c
	$(LINK.c) owplaces.c $(LDLIBS) -o $(CMDS)/owplaces
	$T $@

toolwait.$(HOSTMTYPE): toolwait.c
	$(LINK.c) toolwait.c $(LDLIBS) -o $(CMDS)/toolwait
	$T $@

install: install_$(HOSTMTYPE)

install_$(HOSTMTYPE): install_here $(INSTALL_REMOTE)

install_here:
	$(RM) olwm.$(HOSTMTYPE) $(ADD_ONS)
	$(MAKE) CMDS=$(INSTALL_BIN) all

install_remote:
	cd $(INSTALL_BIN) ; rcp olwm sun0:$(INSTALL_BIN)
	cd $(INSTALL_BIN) ; rcp olwm sun4:$(INSTALL_BIN)

install_world: install
	for host in $(REMOTE_MACHINES) ; do \
		$(REMOTE_SHELL) -l $(USER) $$host make_from_somewhere olwm install ;\
	done

clean:
	$(RM) $(LOLWM)

ctags: $(SRCS) $(HEADERS)
	ctags -tw $(SRCS) $(HEADERS) > $@

deliver: $(DELIVER_TARGET)

deliver_solaris:
	cd $(INSTALL_BIN) ; cp olwm $(DELIVER_DIR)/bin/dolwm

world: all $(REMOTE_MACHINES)

$(REMOTE_MACHINES): FORCE
	$(REMOTE_SHELL) -l $(USER) $@ make_from_somewhere olwm

FORCE:

later: FORCE
	at 2359 atjob

print: $(PRINTSRC)
	print_big_changes -l26 $?
	$T $@

to_iso: $(HEADERS) $(SRCS) $(PRINTSRC)
	ssh -x sun4 'cd private/aco/x/olwm ; co -l $?'
	scp $? sun4:private/aco/x/olwm
	$T $@

depend: FORCE
	cat /dev/null > deptmp
	$(DEPEND) -f deptmp $(DEPENDFLAGS) $(SRCS)
	if [ -n "$(IGNORE_INCLUDE)" ]; then \
	( for PATTERN in `echo "$(IGNORE_INCLUDE)"`; do \
		echo "g:$$PATTERN[^ ]*:s:::g"; \
	done; echo x ) | ex deptmp > /dev/null; fi
	( echo '/^# DO NOT DELETE/,$$d'; echo x ) | ex makefile > /dev/null
	sed	-e '/:  *$$/d' -e 's/   */ /g' deptmp >> makefile
	$(RM) deptmp deptmp.bak


##############################################################
# DO NOT DELETE

libolwm_$(HOSTMTYPE).a(atom.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h 
libolwm_$(HOSTMTYPE).a(atom.o): olwm.h atom.h
libolwm_$(HOSTMTYPE).a(client.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(client.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(client.o): events.h mem.h olwm.h win.h list.h screen.h group.h globals.h dsdm.h resources.h atom.h
libolwm_$(HOSTMTYPE).a(cmdstream.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(cmdstream.o): cmdstream.h mem.h
libolwm_$(HOSTMTYPE).a(debug.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(debug.o): i18n.h olwm.h win.h list.h events.h screen.h debug.h
libolwm_$(HOSTMTYPE).a(dra_signal.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h 
libolwm_$(HOSTMTYPE).a(dra_signal.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h
libolwm_$(HOSTMTYPE).a(dra_signal.o): i18n.h ollocale.h events.h olwm.h globals.h list.h
libolwm_$(HOSTMTYPE).a(dra_sel.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(dra_sel.o): events.h properties.h selection.h atom.h
libolwm_$(HOSTMTYPE).a(defaults.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h
libolwm_$(HOSTMTYPE).a(defaults.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(defaults.o): i18n.h ollocale.h olwm.h defaults.h globals.h list.h resources.h
libolwm_$(HOSTMTYPE).a(dra_color.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(dra_color.o): i18n.h ollocale.h olwm.h globals.h list.h resources.h events.h mem.h win.h screen.h group.h
libolwm_$(HOSTMTYPE).a(dsdm.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(dsdm.o): i18n.h ollocale.h mem.h olwm.h properties.h selection.h win.h list.h events.h screen.h
libolwm_$(HOSTMTYPE).a(dsdm.o): error.h dsdm.h atom.h
libolwm_$(HOSTMTYPE).a(environ.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(environ.o): mem.h atom.h
libolwm_$(HOSTMTYPE).a(error.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(error.o): i18n.h ollocale.h error.h olwm.h globals.h list.h
libolwm_$(HOSTMTYPE).a(evbind.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(evbind.o): i18n.h ollocale.h olwm.h win.h list.h events.h screen.h 
libolwm_$(HOSTMTYPE).a(evbind.o): globals.h mem.h kbdfuncs.h resources.h error.h
libolwm_$(HOSTMTYPE).a(events.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(events.o): i18n.h
libolwm_$(HOSTMTYPE).a(events.o): ollocale.h olwm.h win.h list.h events.h screen.h debug.h globals.h mem.h error.h atom.h
libolwm_$(HOSTMTYPE).a(group.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(group.o): i18n.h mem.h st.h olwm.h win.h list.h events.h screen.h group.h
libolwm_$(HOSTMTYPE).a(helpsend.o): helpcmd.h cmdstream.h
libolwm_$(HOSTMTYPE).a(i18n.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(i18n.o): i18n.h 
libolwm_$(HOSTMTYPE).a(i18n.o): mem.h ollocale.h olwm.h globals.h list.h
libolwm_$(HOSTMTYPE).a(info.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(info.o): i18n.h olwm.h win.h list.h
libolwm_$(HOSTMTYPE).a(info.o): events.h screen.h st.h mem.h error.h
libolwm_$(HOSTMTYPE).a(kbdfuncs.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(kbdfuncs.o): i18n.h ollocale.h olwm.h win.h list.h events.h screen.h menu.h globals.h group.h resources.h
libolwm_$(HOSTMTYPE).a(list.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(list.o): mem.h list.h
libolwm_$(HOSTMTYPE).a(mem.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(mem.o): i18n.h mem.h error.h 
libolwm_$(HOSTMTYPE).a(menu.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(menu.o): i18n.h 
libolwm_$(HOSTMTYPE).a(menu.o): ollocale.h mem.h events.h olwm.h win.h list.h screen.h menu.h globals.h error.h atom.h
libolwm_$(HOSTMTYPE).a(moveresize.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(moveresize.o): i18n.h ollocale.h mem.h olwm.h
libolwm_$(HOSTMTYPE).a(moveresize.o): win.h list.h events.h screen.h globals.h error.h group.h
libolwm_$(HOSTMTYPE).a(notice.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(notice.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(notice.o): olwm.h screen.h notice.h globals.h list.h mem.h events.h error.h
libolwm_$(HOSTMTYPE).a(olwm.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h 
libolwm_$(HOSTMTYPE).a(olwm.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(olwm.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h 
libolwm_$(HOSTMTYPE).a(olwm.o): i18n.h ollocale.h events.h mem.h olwm.h win.h list.h screen.h menu.h defaults.h resources.h globals.h group.h selection.h slots.h error.h dsdm.h
libolwm_$(HOSTMTYPE).a(properties.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(properties.o): i18n.h olwm.h win.h list.h
libolwm_$(HOSTMTYPE).a(properties.o): events.h screen.h mem.h properties.h atom.h
libolwm_$(HOSTMTYPE).a(resources.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(resources.o): i18n.h mem.h olwm.h ollocale.h defaults.h globals.h list.h resources.h win.h events.h screen.h olcursor.h error.h
libolwm_$(HOSTMTYPE).a(screen.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(screen.o): i18n.h ollocale.h mem.h olwm.h defaults.h globals.h list.h resources.h selection.h environ.h win.h events.h screen.h menu.h slots.h atom.h iconimage.h iconmask.h
libolwm_$(HOSTMTYPE).a(selection.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(selection.o): i18n.h olwm.h win.h list.h events.h screen.h
libolwm_$(HOSTMTYPE).a(selection.o): mem.h atom.h selection.h
libolwm_$(HOSTMTYPE).a(services.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(services.o): services.h win.h list.h events.h screen.h
libolwm_$(HOSTMTYPE).a(services.o): olwm.h i18n.h menu.h ollocale.h notice.h globals.h group.h mem.h resources.h error.h dsdm.h atom.h
libolwm_$(HOSTMTYPE).a(slave.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h 
libolwm_$(HOSTMTYPE).a(slave.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(slave.o): cmdstream.h error.h
libolwm_$(HOSTMTYPE).a(slots.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(slots.o): i18n.h ollocale.h mem.h st.h olwm.h win.h list.h events.h screen.h globals.h
libolwm_$(HOSTMTYPE).a(slots.o): slots.h
libolwm_$(HOSTMTYPE).a(st.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(st.o): st.h mem.h
libolwm_$(HOSTMTYPE).a(states.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(states.o): i18n.h ollocale.h mem.h olwm.h win.h
libolwm_$(HOSTMTYPE).a(states.o): list.h events.h screen.h group.h globals.h properties.h atom.h
libolwm_$(HOSTMTYPE).a(usermenu.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(usermenu.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h 
libolwm_$(HOSTMTYPE).a(usermenu.o): i18n.h ollocale.h olwm.h globals.h list.h mem.h win.h 
libolwm_$(HOSTMTYPE).a(usermenu.o): events.h screen.h menu.h services.h
libolwm_$(HOSTMTYPE).a(usleep.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h 
libolwm_$(HOSTMTYPE).a(win.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(win.o): i18n.h olwm.h win.h list.h events.h screen.h mem.h st.h
libolwm_$(HOSTMTYPE).a(win.o): notice.h
libolwm_$(HOSTMTYPE).a(winbusy.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winbusy.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winbusy.o): mem.h olwm.h win.h list.h events.h screen.h globals.h
libolwm_$(HOSTMTYPE).a(winbutton.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winbutton.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winbutton.o): mem.h olwm.h win.h list.h events.h screen.h globals.h menu.h atom.h
libolwm_$(HOSTMTYPE).a(wincolor.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(wincolor.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(wincolor.o): mem.h olwm.h win.h list.h events.h screen.h globals.h atom.h
libolwm_$(HOSTMTYPE).a(winframe.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winframe.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winframe.o): mem.h olwm.h win.h list.h events.h screen.h menu.h group.h globals.h error.h atom.h
libolwm_$(HOSTMTYPE).a(wingframe.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(wingframe.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(wingframe.o): mem.h olwm.h win.h list.h events.h screen.h globals.h
libolwm_$(HOSTMTYPE).a(winicon.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winicon.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winicon.o): mem.h olwm.h win.h list.h events.h screen.h menu.h globals.h slots.h group.h
libolwm_$(HOSTMTYPE).a(winipane.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winipane.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winipane.o): mem.h olwm.h win.h list.h events.h screen.h menu.h globals.h error.h atom.h
libolwm_$(HOSTMTYPE).a(winmenu.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winmenu.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winmenu.o): mem.h olwm.h win.h list.h events.h screen.h menu.h globals.h
libolwm_$(HOSTMTYPE).a(winnofoc.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winnofoc.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winnofoc.o): mem.h olwm.h win.h list.h events.h screen.h globals.h selection.h atom.h
libolwm_$(HOSTMTYPE).a(winpane.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winpane.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winpane.o): mem.h olwm.h win.h list.h events.h screen.h globals.h atom.h
libolwm_$(HOSTMTYPE).a(winpinmenu.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winpinmenu.o): i18n.h ollocale.h
libolwm_$(HOSTMTYPE).a(winpinmenu.o): mem.h olwm.h win.h list.h events.h screen.h menu.h globals.h atom.h
libolwm_$(HOSTMTYPE).a(winpush.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winpush.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winpush.o): mem.h olwm.h win.h list.h events.h screen.h globals.h atom.h
libolwm_$(HOSTMTYPE).a(winresize.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winresize.o): ollocale.h i18n.h
libolwm_$(HOSTMTYPE).a(winresize.o): mem.h olwm.h win.h list.h events.h screen.h globals.h
libolwm_$(HOSTMTYPE).a(winroot.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(winroot.o): i18n.h ollocale.h 
libolwm_$(HOSTMTYPE).a(winroot.o): mem.h olwm.h win.h list.h events.h screen.h menu.h globals.h group.h error.h atom.h selection.h
libolwm_$(HOSTMTYPE).a(gettext.o): gettext.h 
libolwm_$(HOSTMTYPE).a(gettext.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(gettext.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h 
libolwm_$(HOSTMTYPE).a(dra_quick.o): /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h 
libolwm_$(HOSTMTYPE).a(dra_quick.o): win.h 
libolwm_$(HOSTMTYPE).a(dra_quick.o): list.h events.h screen.h olwm.h i18n.h 
