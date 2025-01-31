# @(#)Imakefile	1.12 28 Jun 1993 SMI

NON_EXECS = .text_extras_menu .textswrc


CFILES.textsw.4        = txt_cb.c convpos.c
CFILES.textsw.3        =

OFILES.textsw.3	= ${CFILES.textsw.3:%.c=%.o}
OFILES.textsw.4	= ${CFILES.textsw.4:%.c=%.o}

HFILES.textsw.4        = convpos.h
HFILES.textsw.3        =

HDRSPRIVATE=    ei.h es.h ev.h ev_impl.h primal.h ps_impl.h		finger_tbl.h txt_impl.h txt_18impl.h ${HFILES.textsw.3}

HDRSPUBLIC=textsw.h text.h

SRCS=es_file.c es_mem.c es_util.c es_attr.c ps_impl.c ev_display.c ev_op_bdry.c ev_edit.c ev_once.c ev_attr.c ev_field.c ev_update.c es_cp_file.c ei_text.c ei_attr.c finger_tbl.c txt_once.c txt_input.c txt_edit.c txt_menu.c txt_disp.c txt_event.c txt_find.c txt_file.c txt_sel.c txt_selsvc.c txt_again.c txt_getkey.c txt_filter.c text.c txt_attr.c txt_dbx.c txt_tool.c txt_scroll.c txt_field.c txt_caret.c txt_search.c txt_e_menu.c txt_move.c txt_view.c txt_xsel.c txt_popup.c txt_match.c txt_load.c txt_line.c txt_incl.c ${CFILES.textsw.3}

OBJS=$(SRCS:.c=.o)

LIBTARGET = xview

TARGETDIR = ../../lib${LIBTARGET}/
SHARED_DIR = $(TARGETDIR)xshared
SOXVIEWREV = 3.2
SOREV = $(SOXVIEWREV)
SODIR = shared

###########################################################################
# @(#)XView.obj	1.6 28 Jun 1993 SMI
# XView.obj for XView release 3.0.  Common commands for all library objects.

STD_DEFINES = -Dlinux -D_POSIX_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE -DX_LOCALE -DBSD

include ../default.linux
CDEBUGFLAGS=-g -pipe -Wall -Wmissing-prototypes -Wtraditional-conversion -Wstrict-prototypes -D_FILE_OFFSET_BITS=64

RM = rm -f
CC=cc
MV=mv
CP=cp
AR=ar r
PIC=-fPIC
DEPEND=gccmakedep
DEPEND_DEFINES=
CHECKOUT,v=$(CO) $(COFLAGS) $<

HEADERS= ${HDRSPUBLIC} ${HDRSPRIVATE}
BUILDINCDIR = ../../../build
INCLUDES= -I$(BUILDINCDIR)/include
INCROOT = /usr/X11R6/include
TOP_INCLUDES = -I$(INCROOT)
PROTO_DEFINES = -DFUNCPROTO=15 -DNARROWPROTO
LOCALE_DEFINES = -DOS_HAS_LOCALE
MMAP_DEFINES = -DOS_HAS_MMAP
DEFINES = $(LOCALE_DEFINES) $(MMAP_DEFINES)
ALLDEFINES = $(INCLUDES) $(TOP_INCLUDES) $(STD_DEFINES) $(PROTO_DEFINES) $(DEFINES)
CFLAGS = $(CDEBUGFLAGS) -ansi $(ALLDEFINES)

XVDESTDIR = $$OPENWINHOME
HEADER_DEST = $(XVDESTDIR)/include
INSTALL = install
INSTALLFLAGS = -c
INSTDATFLAGS = -m 0444
XVPUBINC = xview
XVPUB_HEADER_DEST = $(HEADER_DEST)/$(XVPUBINC)
MKDIRHIER = mkdirhier

ALLFILES = $(SRCS) $(HEADERS)

all: preparedirs retrieve lib${LIBTARGET}.so.${SOREV} lib${LIBTARGET}.a

.c.o:
	$(RM) $@ $(SODIR)32/$@ $(SODIR)64/$@
	$(CC) -m32 $(PIC) $(CFLAGS) -c $*.c
	$(MV) $@ $(SODIR)32/$*.o
	$(CC) $(PIC) $(CFLAGS) -c $*.c
	$(MV) $@ $(SODIR)64/$*.o

.c.s:
	$(CC) $(PIC) $(CFLAGS) -S $<
	
clean::
	-@if [ -d $(SODIR) ]; then echo "	$(RM) $(SODIR)/?*.o"; \
	$(RM) $(SODIR)/?*.o; else exit 0; fi

lib${LIBTARGET}.so.${SOREV}::  $(OBJS)
	@for i in $(OBJS) ; do ( $(CP) $(SODIR)32/$$i $(SHARED_DIR)32; $(CP) $(SODIR)64/$$i $(SHARED_DIR)64; $(CP) $(SODIR)32/$$i .); done

lib${LIBTARGET}.a:: $(OBJS)
	$(AR) $(TARGETDIR)$@ $(OBJS)

preparedirs:
	-@if [ ! -d $(SODIR)32 ]; then mkdir $(SODIR)32; else exit 0; fi
	-@if [ ! -d $(SODIR)64 ]; then mkdir $(SODIR)64; else exit 0; fi

depend::
	$(DEPEND) $(DEPENDFLAGS) -- $(ALLDEFINES) $(DEPEND_DEFINES) -- $(SRCS)

includes::
	@if [ -d $(BUILDINCDIR)/$(XVPUB_INCDIR) ]; then set +x; \
	else (set -x; $(MKDIRHIER) $(BUILDINCDIR)/$(XVPUB_INCDIR)); fi
	@(set -x; cd $(BUILDINCDIR)/$(XVPUB_INCDIR); for i in $(HDRSPUBLIC); do \
	$(RM) $$i; \
	$(LN) $(BUILDINCTOP)//$(CURRENT_DIR)/$$i .; \
	done)

install::
	$(MKDIRHIER) $(XVPUB_HEADER_DEST)

install:: $(HDRSPUBLIC)
	@if [ -d $(DESTDIR)$(XVPUB_HEADER_DEST) ]; then set +x; \
	else (set -x; $(MKDIRHIER) $(DESTDIR)$(XVPUB_HEADER_DEST)); fi
	@case '${MFLAGS}' in *[i]*) set +e;; esac; \
	for i in $(HDRSPUBLIC); do \
	(set -x; $(INSTALL) $(INSTALLFLAGS) $(INSTDATFLAGS) $$i $(DESTDIR)$(XVPUB_HEADER_DEST)); \
	done

includes::
	@if [ -d $(BUILDINCDIR)/$(XVPRI_INCDIR) ]; then set +x; \
	else (set -x; $(MKDIRHIER) $(BUILDINCDIR)/$(XVPRI_INCDIR)); fi
	@(set -x; cd $(BUILDINCDIR)/$(XVPRI_INCDIR); for i in $(HDRSPRIVATE); do \
	$(RM) $$i; \
	$(LN) $(BUILDINCTOP)//$(CURRENT_DIR)/$$i .; \
	done)

populate::
	@case '${MFLAGS}' in *[i]*) set +e;; esac;
	-@if [ -h SCCS ]; then \
	for i in $(ALLFILES) ; do sccs get $$i ; done ; \
	elif [ -h SCCS- ]; then \
	$(MV) SCCS- SCCS ; \
	for i in $(ALLFILES) ; do sccs get $$i ; done ; \
	$(MV) SCCS SCCS- ; \
	else \
	echo "SCCS or SCCS- do not exist" ; \
	fi

includes::

install::

# -----------------------------------------------------------------------
# common rules for all Makefiles - do not edit

.SUFFIXES: .o .i .c

.c.i:
	$(RM) $@
	$(CC) -E $(CFLAGS) $(_NOOP_) $*.c > $@

emptyrule::

clean::
	$(RM_CMD) *.CKP *.ln *.BAK *.bak *.o core errs ,* *~ *.a .emacs_* tags TAGS make.log MakeOut  "#"*

Makefile::
	-@if [ -f Makefile ]; then set -x; \
	$(RM) Makefile.bak; $(MV) Makefile Makefile.bak; \
	else exit 0; fi
	$(IMAKE_CMD) -DTOPDIR=$(TOP) -DCURDIR=$(CURRENT_DIR)

tags::
	$(TAGS) *.[ch]

# -----------------------------------------------------------------------
# empty rules for directories that do not have SUBDIRS - do not edit

install::
	@echo "install in $(CURRENT_DIR) done"

install.man::
	@echo "install.man in $(CURRENT_DIR) done"

install.linkkit::
	@echo "install.linkkit in $(CURRENT_DIR) done"

Makefiles::

includes::

depend::

retrieve: $(HEADERS)


# convpos.h: RCS/convpos.h,v
# ei.h: RCS/ei.h,v
es.h: RCS/es.h,v
# ev.h: RCS/ev.h,v
ev_impl.h: RCS/ev_impl.h,v
# finger_tbl.h: RCS/finger_tbl.h,v
# primal.h: RCS/primal.h,v
# ps_impl.h: RCS/ps_impl.h,v
# text.h: RCS/text.h,v
# textsw.h: RCS/textsw.h,v
# txt_18impl.h: RCS/txt_18impl.h,v
# txt_impl.h: RCS/txt_impl.h,v

# -----------------------------------------------------------------------
# dependencies generated by makedepend

# DO NOT DELETE
es_file.o: es_file.c /usr/include/stdc-predef.h /usr/include/string.h \
 /usr/include/bits/libc-header-start.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/fcntl.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/fcntl.h /usr/include/bits/fcntl-linux.h \
 /usr/include/bits/types/struct_iovec.h /usr/include/linux/falloc.h \
 /usr/include/bits/types/struct_timespec.h /usr/include/bits/endian.h \
 /usr/include/bits/endianness.h /usr/include/bits/types/time_t.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/sys/dir.h /usr/include/dirent.h /usr/include/bits/dirent.h \
 /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h \
 /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/dirent_ext.h /usr/include/sys/errno.h \
 /usr/include/errno.h /usr/include/bits/errno.h \
 /usr/include/linux/errno.h /usr/include/asm/errno.h \
 /usr/include/asm-generic/errno.h /usr/include/asm-generic/errno-base.h \
 /usr/include/bits/types/error_t.h /usr/include/sys/param.h \
 /usr/include/sys/types.h /usr/include/bits/types/clock_t.h \
 /usr/include/bits/types/clockid_t.h /usr/include/bits/types/timer_t.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h \
 /usr/include/limits.h /usr/include/bits/posix2_lim.h \
 /usr/include/bits/xopen_lim.h /usr/include/bits/uio_lim.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/param.h \
 /usr/include/linux/param.h /usr/include/asm/param.h \
 /usr/include/asm-generic/param.h /usr/include/sys/stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h /usr/include/sys/file.h \
 /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/assert.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 ../../../build/include/xview/base.h /usr/include/malloc.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/txt_18impl.h
es_mem.o: es_mem.c /usr/include/stdc-predef.h /usr/include/sys/types.h \
 /usr/include/features.h /usr/include/features-time64.h \
 /usr/include/bits/wordsize.h /usr/include/bits/timesize.h \
 /usr/include/sys/cdefs.h /usr/include/bits/long-double.h \
 /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h \
 /usr/include/bits/types.h /usr/include/bits/typesizes.h \
 /usr/include/bits/time64.h /usr/include/bits/types/clock_t.h \
 /usr/include/bits/types/clockid_t.h /usr/include/bits/types/time_t.h \
 /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/pkg.h \
 ../../../build/include/xview/attr.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview_private/es.h
es_util.o: es_util.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/txt_18impl.h
es_attr.o: es_attr.c /usr/include/stdc-predef.h /usr/include/sys/types.h \
 /usr/include/features.h /usr/include/features-time64.h \
 /usr/include/bits/wordsize.h /usr/include/bits/timesize.h \
 /usr/include/sys/cdefs.h /usr/include/bits/long-double.h \
 /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h \
 /usr/include/bits/types.h /usr/include/bits/typesizes.h \
 /usr/include/bits/time64.h /usr/include/bits/types/clock_t.h \
 /usr/include/bits/types/clockid_t.h /usr/include/bits/types/time_t.h \
 /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/base.h /usr/include/string.h \
 /usr/include/bits/libc-header-start.h /usr/include/bits/types/locale_t.h \
 /usr/include/bits/types/__locale_t.h /usr/include/strings.h \
 /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/attr.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/es.h
ps_impl.o: ps_impl.c /usr/include/stdc-predef.h /usr/include/stdio.h \
 /usr/include/bits/libc-header-start.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types.h /usr/include/bits/typesizes.h \
 /usr/include/bits/time64.h /usr/include/bits/types/__fpos_t.h \
 /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/attr.h /usr/include/sys/types.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/types/locale_t.h \
 /usr/include/bits/types/__locale_t.h /usr/include/strings.h \
 /usr/include/malloc.h ../../../build/include/xview/xv_c_types.h \
 /usr/include/X11/Xfuncproto.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview_private/ps_impl.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h
ev_display.o: ev_display.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/xview_private/pw_impl.h \
 ../../../build/include/xview_private/draw_impl.h \
 ../../../build/include/xview/cms.h \
 ../../../build/include/xview_private/scrn_vis.h \
 ../../../build/include/xview_private/tty_impl.h /usr/include/termios.h \
 /usr/include/bits/termios.h /usr/include/bits/termios-struct.h \
 /usr/include/bits/termios-c_cc.h /usr/include/bits/termios-c_iflag.h \
 /usr/include/bits/termios-c_oflag.h /usr/include/bits/termios-baud.h \
 /usr/include/bits/termios-c_cflag.h /usr/include/bits/termios-c_lflag.h \
 /usr/include/bits/termios-tcflow.h /usr/include/bits/termios-misc.h \
 /usr/include/sys/ttydefaults.h ../../../build/include/xview/tty.h
ev_op_bdry.o: ev_op_bdry.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h
ev_edit.o: ev_edit.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/pixrect/memvar.h
ev_once.o: ev_once.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h
ev_attr.o: ev_attr.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h
ev_field.o: ev_field.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h
ev_update.o: ev_update.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/tty_impl.h /usr/include/termios.h \
 /usr/include/bits/termios.h /usr/include/bits/termios-struct.h \
 /usr/include/bits/termios-c_cc.h /usr/include/bits/termios-c_iflag.h \
 /usr/include/bits/termios-c_oflag.h /usr/include/bits/termios-baud.h \
 /usr/include/bits/termios-c_cflag.h /usr/include/bits/termios-c_lflag.h \
 /usr/include/bits/termios-tcflow.h /usr/include/bits/termios-misc.h \
 /usr/include/sys/ttydefaults.h ../../../build/include/xview/tty.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/xview_private/pw_impl.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/xview_private/draw_impl.h \
 ../../../build/include/xview/cms.h \
 ../../../build/include/xview_private/scrn_vis.h
es_cp_file.o: es_cp_file.c /usr/include/stdc-predef.h \
 /usr/include/stdio.h /usr/include/bits/libc-header-start.h \
 /usr/include/features.h /usr/include/features-time64.h \
 /usr/include/bits/wordsize.h /usr/include/bits/timesize.h \
 /usr/include/sys/cdefs.h /usr/include/bits/long-double.h \
 /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types.h /usr/include/bits/typesizes.h \
 /usr/include/bits/time64.h /usr/include/bits/types/__fpos_t.h \
 /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/sys/param.h \
 /usr/include/sys/types.h /usr/include/bits/types/clock_t.h \
 /usr/include/bits/types/clockid_t.h /usr/include/bits/types/time_t.h \
 /usr/include/bits/types/timer_t.h /usr/include/bits/stdint-intn.h \
 /usr/include/endian.h /usr/include/bits/endian.h \
 /usr/include/bits/endianness.h /usr/include/bits/byteswap.h \
 /usr/include/bits/uintn-identity.h /usr/include/sys/select.h \
 /usr/include/bits/select.h /usr/include/bits/types/sigset_t.h \
 /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h \
 /usr/include/limits.h /usr/include/bits/posix1_lim.h \
 /usr/include/bits/local_lim.h /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/posix2_lim.h /usr/include/bits/xopen_lim.h \
 /usr/include/bits/uio_lim.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/param.h \
 /usr/include/linux/param.h /usr/include/asm/param.h \
 /usr/include/asm-generic/param.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h /usr/include/fcntl.h \
 /usr/include/bits/fcntl.h /usr/include/bits/fcntl-linux.h \
 /usr/include/bits/types/struct_iovec.h /usr/include/linux/falloc.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 ../../../build/include/xview/base.h /usr/include/string.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/bits/types/idtype_t.h /usr/include/sys/time.h \
 /usr/include/sys/resource.h /usr/include/bits/resource.h \
 /usr/include/bits/types/struct_rusage.h /usr/include/sys/signal.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview/attrol.h
ei_text.o: ei_text.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h /usr/include/ctype.h \
 /usr/include/X11/Xlib.h /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/defaults.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/tty_impl.h /usr/include/termios.h \
 /usr/include/bits/termios.h /usr/include/bits/termios-struct.h \
 /usr/include/bits/termios-c_cc.h /usr/include/bits/termios-c_iflag.h \
 /usr/include/bits/termios-c_oflag.h /usr/include/bits/termios-baud.h \
 /usr/include/bits/termios-c_cflag.h /usr/include/bits/termios-c_lflag.h \
 /usr/include/bits/termios-tcflow.h /usr/include/bits/termios-misc.h \
 /usr/include/sys/ttydefaults.h ../../../build/include/xview/tty.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h
ei_attr.o: ei_attr.c /usr/include/stdc-predef.h /usr/include/sys/types.h \
 /usr/include/features.h /usr/include/features-time64.h \
 /usr/include/bits/wordsize.h /usr/include/bits/timesize.h \
 /usr/include/sys/cdefs.h /usr/include/bits/long-double.h \
 /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h \
 /usr/include/bits/types.h /usr/include/bits/typesizes.h \
 /usr/include/bits/time64.h /usr/include/bits/types/clock_t.h \
 /usr/include/bits/types/clockid_t.h /usr/include/bits/types/time_t.h \
 /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/base.h /usr/include/string.h \
 /usr/include/bits/libc-header-start.h /usr/include/bits/types/locale_t.h \
 /usr/include/bits/types/__locale_t.h /usr/include/strings.h \
 /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/attr.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview_private/es.h
finger_tbl.o: finger_tbl.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/base.h /usr/include/string.h \
 /usr/include/bits/libc-header-start.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types.h /usr/include/bits/typesizes.h \
 /usr/include/bits/time64.h /usr/include/bits/types/__fpos_t.h \
 /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/sys/types.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/pkg.h \
 ../../../build/include/xview/attr.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview_private/es.h
txt_once.o: txt_once.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 /usr/include/fcntl.h /usr/include/bits/fcntl.h \
 /usr/include/bits/fcntl-linux.h /usr/include/bits/types/struct_iovec.h \
 /usr/include/linux/falloc.h ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_notify.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/defaults.h \
 ../../../build/include/xview/cursor.h \
 ../../../build/include/xview_private/attr_impl.h \
 ../../../build/include/xview_private/font_impl.h \
 ../../../build/include/xview_private/ps_impl.h \
 ../../../build/include/xview_private/ev_impl.h /usr/include/sys/dir.h \
 /usr/include/dirent.h /usr/include/bits/dirent.h \
 /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h \
 /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/dirent_ext.h
txt_input.o: txt_input.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/help.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/cursor.h \
 ../../../build/include/xview_private/win_keymap.h \
 ../../../build/include/xview_private/windowimpl.h \
 ../../../build/include/xview_private/draw_impl.h \
 ../../../build/include/xview/cms.h \
 ../../../build/include/xview_private/scrn_vis.h
txt_edit.o: txt_edit.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview/notice.h
txt_menu.o: txt_menu.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h /usr/include/assert.h \
 ../../../build/include/pixrect/pr_util.h \
 ../../../build/include/xview/file_chsr.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/xview/defaults.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/fullscreen.h \
 ../../../build/include/xview/cursor.h \
 ../../../build/include/xview/notice.h
txt_disp.o: txt_disp.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/win_info.h \
 ../../../build/include/xview_private/scrn_vis.h \
 ../../../build/include/xview/win_notify.h
txt_event.o: txt_event.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/draw_impl.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/cms.h \
 ../../../build/include/xview_private/scrn_vis.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h /usr/include/X11/Xutil.h \
 /usr/include/X11/keysym.h /usr/include/X11/keysymdef.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/win_input.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/errno.h /usr/include/bits/errno.h \
 /usr/include/linux/errno.h /usr/include/asm/errno.h \
 /usr/include/asm-generic/errno.h /usr/include/asm-generic/errno-base.h \
 /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview/win_notify.h
txt_find.o: txt_find.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h
txt_file.o: txt_file.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h /usr/include/assert.h \
 /usr/include/sys/dir.h /usr/include/dirent.h /usr/include/bits/dirent.h \
 /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h \
 /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/dirent_ext.h /usr/include/sys/file.h \
 /usr/include/fcntl.h /usr/include/bits/fcntl.h \
 /usr/include/bits/fcntl-linux.h /usr/include/bits/types/struct_iovec.h \
 /usr/include/linux/falloc.h /usr/include/sys/param.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h \
 /usr/include/limits.h /usr/include/bits/posix2_lim.h \
 /usr/include/bits/xopen_lim.h /usr/include/bits/uio_lim.h \
 /usr/include/bits/param.h /usr/include/linux/param.h \
 /usr/include/asm/param.h /usr/include/asm-generic/param.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/expandname.h
txt_sel.o: txt_sel.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h /usr/include/assert.h
txt_selsvc.o: txt_selsvc.c /usr/include/stdc-predef.h \
 /usr/include/errno.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/errno.h \
 /usr/include/linux/errno.h /usr/include/asm/errno.h \
 /usr/include/asm-generic/errno.h /usr/include/asm-generic/errno-base.h \
 /usr/include/bits/types/error_t.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/types.h /usr/include/bits/typesizes.h \
 /usr/include/bits/time64.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/attr.h /usr/include/sys/types.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview_private/seln_impl.h /usr/include/netdb.h \
 /usr/include/netinet/in.h /usr/include/bits/stdint-uintn.h \
 /usr/include/sys/socket.h /usr/include/bits/types/struct_iovec.h \
 /usr/include/bits/socket.h /usr/include/bits/socket_type.h \
 /usr/include/bits/sockaddr.h /usr/include/asm/socket.h \
 /usr/include/asm-generic/socket.h /usr/include/asm/sockios.h \
 /usr/include/asm-generic/sockios.h \
 /usr/include/bits/types/struct_osockaddr.h /usr/include/bits/in.h \
 /usr/include/rpc/netdb.h /usr/include/bits/netdb.h \
 ../../../build/include/xview/sel_svc.h /usr/include/rpc/rpc.h \
 /usr/include/rpc/types.h /usr/include/sys/param.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h \
 /usr/include/limits.h /usr/include/bits/posix1_lim.h \
 /usr/include/bits/local_lim.h /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/posix2_lim.h /usr/include/bits/xopen_lim.h \
 /usr/include/bits/uio_lim.h /usr/include/bits/param.h \
 /usr/include/linux/param.h /usr/include/asm/param.h \
 /usr/include/asm-generic/param.h /usr/include/netconfig.h \
 /usr/include/rpc/xdr.h /usr/include/rpc/auth.h \
 /usr/include/rpc/clnt_stat.h /usr/include/rpc/clnt.h \
 /usr/include/sys/un.h /usr/include/rpc/clnt_soc.h \
 /usr/include/rpc/rpc_msg.h /usr/include/rpc/auth_unix.h \
 /usr/include/rpc/auth_des.h /usr/include/rpc/svc_auth.h \
 /usr/include/rpc/svc.h /usr/include/rpc/svc_soc.h \
 /usr/include/rpc/pmap_clnt.h /usr/include/rpc/pmap_prot.h \
 /usr/include/rpc/rpcb_clnt.h /usr/include/rpc/rpcb_prot.h \
 /usr/include/rpc/svc_mt.h /usr/include/rpc/rpcent.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h /usr/include/assert.h \
 /usr/include/dirent.h /usr/include/bits/dirent.h \
 /usr/include/bits/dirent_ext.h
txt_again.o: txt_again.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h /usr/include/ctype.h
txt_getkey.o: txt_getkey.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview_private/ev_impl.h
txt_filter.o: txt_filter.c /usr/include/stdc-predef.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h /usr/include/sys/file.h \
 /usr/include/fcntl.h /usr/include/bits/fcntl.h \
 /usr/include/bits/fcntl-linux.h /usr/include/bits/types/struct_iovec.h \
 /usr/include/linux/falloc.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/sys/dir.h \
 /usr/include/dirent.h /usr/include/bits/dirent.h \
 /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h \
 /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/dirent_ext.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 ../../../build/include/xview/base.h /usr/include/string.h \
 /usr/include/bits/libc-header-start.h /usr/include/bits/types/locale_t.h \
 /usr/include/bits/types/__locale_t.h /usr/include/strings.h \
 /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 /usr/include/sys/errno.h /usr/include/sys/ioctl.h \
 /usr/include/bits/ioctls.h /usr/include/asm/ioctls.h \
 /usr/include/asm-generic/ioctls.h /usr/include/linux/ioctl.h \
 /usr/include/asm/ioctl.h /usr/include/asm-generic/ioctl.h \
 /usr/include/bits/ioctl-types.h /usr/include/sys/ttydefaults.h \
 ../../../build/include/xview_private/ultrix_cpt.h \
 ../../../build/include/xview_private/filter.h \
 ../../../build/include/xview_private/io_stream.h
text.o: text.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview/defaults.h
txt_attr.o: txt_attr.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/attr_impl.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/txt_18impl.h /usr/include/sys/dir.h \
 /usr/include/dirent.h /usr/include/bits/dirent.h \
 /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h \
 /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/dirent_ext.h ../../../build/include/xview/defaults.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/windowimpl.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/cursor.h \
 ../../../build/include/xview_private/draw_impl.h \
 ../../../build/include/xview/cms.h \
 ../../../build/include/xview_private/scrn_vis.h \
 ../../../build/include/xview_private/xv_list.h \
 ../../../build/include/xview_private/font_impl.h
txt_dbx.o: txt_dbx.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h /usr/include/assert.h \
 ../../../build/include/xview/termsw.h
txt_tool.o: txt_tool.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/win_notify.h /usr/include/fcntl.h \
 /usr/include/bits/fcntl.h /usr/include/bits/fcntl-linux.h \
 /usr/include/bits/types/struct_iovec.h /usr/include/linux/falloc.h \
 ../../../build/include/xview/win_struct.h
txt_scroll.o: txt_scroll.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview_private/draw_impl.h \
 ../../../build/include/xview/cms.h \
 ../../../build/include/xview_private/scrn_vis.h
txt_field.o: txt_field.c /usr/include/stdc-predef.h /usr/include/string.h \
 /usr/include/bits/libc-header-start.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_screen.h
txt_caret.o: txt_caret.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h
txt_search.o: txt_search.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_screen.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h
txt_e_menu.o: txt_e_menu.c /usr/include/stdc-predef.h \
 /usr/include/stdlib.h /usr/include/bits/libc-header-start.h \
 /usr/include/features.h /usr/include/features-time64.h \
 /usr/include/bits/wordsize.h /usr/include/bits/timesize.h \
 /usr/include/sys/cdefs.h /usr/include/bits/long-double.h \
 /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/bits/floatn.h /usr/include/bits/floatn-common.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/sys/types.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h /usr/include/sys/file.h \
 /usr/include/fcntl.h /usr/include/bits/fcntl.h \
 /usr/include/bits/fcntl-linux.h /usr/include/bits/types/struct_iovec.h \
 /usr/include/linux/falloc.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 ../../../build/include/xview/base.h /usr/include/string.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 /usr/include/ctype.h ../../../build/include/xview/defaults.h \
 ../../../build/include/xview/wmgr.h ../../../build/include/xview/icon.h \
 ../../../build/include/xview/icon_load.h \
 ../../../build/include/xview/str_utils.h
txt_move.o: txt_move.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h /usr/include/sys/stat.h \
 /usr/include/bits/stat.h /usr/include/bits/struct_stat.h \
 /usr/include/bits/statx.h /usr/include/linux/stat.h \
 /usr/include/linux/types.h /usr/include/asm/types.h \
 /usr/include/asm-generic/types.h /usr/include/asm-generic/int-ll64.h \
 /usr/include/asm/bitsperlong.h /usr/include/asm-generic/bitsperlong.h \
 /usr/include/linux/posix_types.h /usr/include/linux/stddef.h \
 /usr/include/asm/posix_types.h /usr/include/asm/posix_types_64.h \
 /usr/include/asm-generic/posix_types.h /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview/cursor.h \
 ../../../build/include/pixrect/memvar.h \
 ../../../build/include/xview/notice.h /usr/include/ctype.h \
 /usr/include/assert.h /usr/include/time.h /usr/include/bits/time.h \
 /usr/include/bits/timex.h /usr/include/bits/types/struct_tm.h \
 /usr/include/bits/types/struct_itimerspec.h \
 ../../../build/include/images/text_move_cursor.pr
txt_view.o: txt_view.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 /usr/include/stdlib.h /usr/include/bits/waitflags.h \
 /usr/include/bits/waitstatus.h /usr/include/alloca.h \
 /usr/include/bits/stdlib-float.h ../../../build/include/xview/notify.h \
 /usr/include/sys/wait.h /usr/include/signal.h \
 /usr/include/bits/signum-generic.h /usr/include/bits/signum-arch.h \
 /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/attrol.h \
 ../../../build/include/xview/defaults.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/win_info.h \
 ../../../build/include/xview_private/scrn_vis.h \
 ../../../build/include/xview_private/windowimpl.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/cursor.h \
 ../../../build/include/xview_private/draw_impl.h \
 ../../../build/include/xview/cms.h \
 ../../../build/include/xview_private/xv_list.h \
 ../../../build/include/xview/text.h
txt_xsel.o: txt_xsel.c /usr/include/stdc-predef.h \
 ../../../build/include/xview/defaults.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview_private/ev_impl.h /usr/include/ctype.h \
 /usr/include/time.h /usr/include/bits/time.h /usr/include/bits/timex.h \
 /usr/include/bits/types/struct_tm.h \
 /usr/include/bits/types/struct_itimerspec.h
txt_popup.o: txt_popup.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview_private/win_info.h \
 ../../../build/include/xview_private/scrn_vis.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/defaults.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_screen.h \
 ../../../build/include/xview/file_chsr.h
txt_match.o: txt_match.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview_private/txt_18impl.h \
 ../../../build/include/xview_private/svr_impl.h \
 ../../../build/include/xview_private/xv_list.h \
 /usr/include/X11/Xresource.h \
 ../../../build/include/xview_private/svr_atom.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_screen.h
txt_load.o: txt_load.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h /usr/include/sys/param.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h \
 /usr/include/limits.h /usr/include/bits/posix1_lim.h \
 /usr/include/bits/local_lim.h /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/posix2_lim.h /usr/include/bits/xopen_lim.h \
 /usr/include/bits/uio_lim.h /usr/include/bits/param.h \
 /usr/include/linux/param.h /usr/include/asm/param.h \
 /usr/include/asm-generic/param.h ../../../build/include/xview/notice.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_screen.h \
 ../../../build/include/xview/file_chsr.h
txt_line.o: txt_line.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h \
 ../../../build/include/xview/notice.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_screen.h
txt_incl.o: txt_incl.c /usr/include/stdc-predef.h \
 ../../../build/include/xview_private/primal.h \
 ../../../build/include/xview_private/i18n_impl.h \
 ../../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../../build/include/xview/pkg.h ../../../build/include/xview/attr.h \
 /usr/include/sys/types.h /usr/include/features.h \
 /usr/include/features-time64.h /usr/include/bits/wordsize.h \
 /usr/include/bits/timesize.h /usr/include/sys/cdefs.h \
 /usr/include/bits/long-double.h /usr/include/gnu/stubs.h \
 /usr/include/gnu/stubs-64.h /usr/include/bits/types.h \
 /usr/include/bits/typesizes.h /usr/include/bits/time64.h \
 /usr/include/bits/types/clock_t.h /usr/include/bits/types/clockid_t.h \
 /usr/include/bits/types/time_t.h /usr/include/bits/types/timer_t.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stddef.h \
 /usr/include/bits/stdint-intn.h /usr/include/endian.h \
 /usr/include/bits/endian.h /usr/include/bits/endianness.h \
 /usr/include/bits/byteswap.h /usr/include/bits/uintn-identity.h \
 /usr/include/sys/select.h /usr/include/bits/select.h \
 /usr/include/bits/types/sigset_t.h /usr/include/bits/types/__sigset_t.h \
 /usr/include/bits/types/struct_timeval.h \
 /usr/include/bits/types/struct_timespec.h \
 /usr/include/bits/pthreadtypes.h /usr/include/bits/thread-shared-types.h \
 /usr/include/bits/pthreadtypes-arch.h \
 /usr/include/bits/atomic_wide_counter.h /usr/include/bits/struct_mutex.h \
 /usr/include/bits/struct_rwlock.h ../../../build/include/xview/base.h \
 /usr/include/string.h /usr/include/bits/libc-header-start.h \
 /usr/include/bits/types/locale_t.h /usr/include/bits/types/__locale_t.h \
 /usr/include/strings.h /usr/include/malloc.h /usr/include/stdio.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/stdarg.h \
 /usr/include/bits/types/__fpos_t.h /usr/include/bits/types/__mbstate_t.h \
 /usr/include/bits/types/__fpos64_t.h /usr/include/bits/types/__FILE.h \
 /usr/include/bits/types/FILE.h /usr/include/bits/types/struct_FILE.h \
 /usr/include/bits/types/cookie_io_functions_t.h \
 /usr/include/bits/stdio_lim.h /usr/include/bits/floatn.h \
 /usr/include/bits/floatn-common.h /usr/include/stdlib.h \
 /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h \
 /usr/include/alloca.h /usr/include/bits/stdlib-float.h \
 ../../../build/include/xview/notify.h /usr/include/sys/wait.h \
 /usr/include/signal.h /usr/include/bits/signum-generic.h \
 /usr/include/bits/signum-arch.h /usr/include/bits/types/sig_atomic_t.h \
 /usr/include/bits/types/siginfo_t.h /usr/include/bits/types/__sigval_t.h \
 /usr/include/bits/siginfo-arch.h /usr/include/bits/siginfo-consts.h \
 /usr/include/bits/siginfo-consts-arch.h \
 /usr/include/bits/types/sigval_t.h /usr/include/bits/types/sigevent_t.h \
 /usr/include/bits/sigevent-consts.h /usr/include/bits/sigaction.h \
 /usr/include/bits/sigcontext.h /usr/include/bits/types/stack_t.h \
 /usr/include/sys/ucontext.h /usr/include/bits/sigstack.h \
 /usr/include/bits/sigstksz.h /usr/include/unistd.h \
 /usr/include/bits/posix_opt.h /usr/include/bits/environments.h \
 /usr/include/bits/confname.h /usr/include/bits/getopt_posix.h \
 /usr/include/bits/getopt_core.h /usr/include/bits/unistd_ext.h \
 /usr/include/linux/close_range.h /usr/include/bits/ss_flags.h \
 /usr/include/bits/types/struct_sigstack.h /usr/include/bits/sigthread.h \
 /usr/include/bits/signal_ext.h /usr/include/bits/types/idtype_t.h \
 /usr/include/sys/time.h /usr/include/sys/resource.h \
 /usr/include/bits/resource.h /usr/include/bits/types/struct_rusage.h \
 /usr/include/sys/signal.h ../../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../../build/include/xview_private/portable.h \
 ../../../build/include/xview/sun.h \
 ../../../build/include/xview_private/txt_impl.h \
 ../../../build/include/xview/scrollbar.h \
 ../../../build/include/xview/window.h \
 ../../../build/include/xview/generic.h \
 ../../../build/include/xview/pkg_public.h \
 ../../../build/include/xview/xv_error.h \
 ../../../build/include/xview/server.h \
 ../../../build/include/xview/screen.h \
 ../../../build/include/xview/drawable.h \
 ../../../build/include/xview/win_input.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h \
 ../../../build/include/xview/win_event.h \
 ../../../build/include/xview/rect.h \
 ../../../build/include/xview/rectlist.h \
 ../../../build/include/xview/attrol.h \
 ../../../build/include/xview_private/ev.h \
 ../../../build/include/pixrect/pixrect.h \
 ../../../build/include/xview/pixwin.h \
 ../../../build/include/pixrect/pr_line.h \
 ../../../build/include/pixrect/traprop.h \
 ../../../build/include/pixrect/pixfont.h \
 ../../../build/include/xview_private/es.h \
 ../../../build/include/xview_private/ei.h \
 ../../../build/include/xview_private/finger_tbl.h \
 /usr/include/sys/stat.h /usr/include/bits/stat.h \
 /usr/include/bits/struct_stat.h /usr/include/bits/statx.h \
 /usr/include/linux/stat.h /usr/include/linux/types.h \
 /usr/include/asm/types.h /usr/include/asm-generic/types.h \
 /usr/include/asm-generic/int-ll64.h /usr/include/asm/bitsperlong.h \
 /usr/include/asm-generic/bitsperlong.h /usr/include/linux/posix_types.h \
 /usr/include/linux/stddef.h /usr/include/asm/posix_types.h \
 /usr/include/asm/posix_types_64.h /usr/include/asm-generic/posix_types.h \
 /usr/include/bits/statx-generic.h \
 /usr/include/bits/types/struct_statx_timestamp.h \
 /usr/include/bits/types/struct_statx.h \
 ../../../build/include/xview/textsw.h \
 ../../../build/include/xview/openwin.h \
 ../../../build/include/xview/sel_attrs.h \
 ../../../build/include/xview/frame.h \
 ../../../build/include/xview/openmenu.h \
 ../../../build/include/xview/panel.h \
 ../../../build/include/xview/canvas.h \
 ../../../build/include/xview/svrimage.h \
 ../../../build/include/xview/font.h \
 ../../../build/include/xview/dragdrop.h \
 ../../../build/include/xview/sel_pkg.h /usr/include/X11/Xatom.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/errno.h \
 /usr/include/bits/errno.h /usr/include/linux/errno.h \
 /usr/include/asm/errno.h /usr/include/asm-generic/errno.h \
 /usr/include/asm-generic/errno-base.h /usr/include/bits/types/error_t.h \
 ../../../build/include/xview_private/ev_impl.h /usr/include/sys/param.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/limits.h \
 /usr/lib64/gcc/x86_64-suse-linux/14/include/syslimits.h \
 /usr/include/limits.h /usr/include/bits/posix1_lim.h \
 /usr/include/bits/local_lim.h /usr/include/linux/limits.h \
 /usr/include/bits/pthread_stack_min-dynamic.h \
 /usr/include/bits/posix2_lim.h /usr/include/bits/xopen_lim.h \
 /usr/include/bits/uio_lim.h /usr/include/bits/param.h \
 /usr/include/linux/param.h /usr/include/asm/param.h \
 /usr/include/asm-generic/param.h ../../../build/include/xview/notice.h \
 ../../../build/include/xview/wmgr.h \
 ../../../build/include/xview/win_struct.h \
 ../../../build/include/xview/win_screen.h \
 ../../../build/include/xview/file_chsr.h
