# Makefile generated by imake - do not edit!
# $XConsortium: imake.c,v 1.91 95/01/12 16:15:47 kaleb Exp $

# ----------------------------------------------------------------------
# Makefile generated from "Imake.tmpl" and <Imakefile>
# $XConsortium: Imake.tmpl,v 1.224.1.1 95/06/19 17:51:01 gildea Exp $
# $XFree86: xc/config/cf/Imake.tmpl,v 3.18 1995/07/12 15:27:23 dawes Exp $
#

.SUFFIXES: .i

# $XConsortium: Imake.cf,v 1.19 95/01/05 19:24:32 kaleb Exp $
# $XFree86: xc/config/cf/Imake.cf,v 3.15 1995/06/08 06:18:39 dawes Exp $

# -----------------------------------------------------------------------
# site-specific configuration parameters that need to come before
# the platform-specific parameters - edit site.def to change

# site:  $XConsortium: site.sample,v 1.9 94/04/08 17:02:06 rws Exp $
# site:  $XFree86: xc/config/cf/site.def,v 3.8 1995/05/27 01:56:50 dawes Exp $

# $XFree86: xc/config/cf/xf86site.def,v 3.48 1995/07/22 04:11:45 dawes Exp $

# -----------------------------------------------------------------------
# platform-specific configuration parameters - edit linux.cf to change

# platform:  $XConsortium: linux.cf,v 1.11 95/01/23 18:32:03 kaleb Exp $
# platform:  $XFree86: xc/config/cf/linux.cf,v 3.26 1995/07/12 15:27:34 dawes Exp $

# $XConsortium: lnxLib.rules,v 1.8 95/01/16 21:11:00 kaleb Exp $
# $XFree86: xc/config/cf/lnxLib.rules,v 3.14 1995/06/01 08:18:56 dawes Exp $

# $XConsortium: xfree86.cf,v 1.9 95/01/25 16:42:40 kaleb Exp $
# $XFree86: xc/config/cf/xfree86.cf,v 3.66 1995/07/22 04:11:47 dawes Exp $

LINKKITDIR = $(USRLIBDIR)/Server

       XF86SRC = $(SERVERSRC)/hw/xfree86
  XF86ACCELSRC = $(XF86SRC)/accel
    XF86COMSRC = $(XF86SRC)/common
 XF86CONFIGSRC = $(XF86COMSRC)
     XF86HWSRC = $(XF86SRC)/common_hw
     XF86OSSRC = $(XF86SRC)/os-support
  VGADRIVERSRC = $(XF86SRC)/vga256/drivers
VGA16DRIVERSRC = $(XF86SRC)/vga16/drivers
 VGA2DRIVERSRC = $(XF86SRC)/vga2/drivers
 MONODRIVERSRC = $(XF86SRC)/mono/drivers
   S3DRIVERSRC = $(XF86SRC)/accel/s3/drivers

        XFREE86DOCDIR = $(LIBDIR)/doc
      XFREE86PSDOCDIR = $(XFREE86DOCDIR)/PostScript
    XFREE86HTMLDOCDIR = $(XFREE86DOCDIR)/html
XFREE86JAPANESEDOCDIR = $(XFREE86DOCDIR)/Japanese

# $XConsortium: xf86.rules,v 1.7 95/01/25 16:34:39 kaleb Exp $
# $XFree86: xc/config/cf/xf86.rules,v 3.9 1995/07/12 15:27:38 dawes Exp $

# -----------------------------------------------------------------------
# site-specific configuration parameters that go after
# the platform-specific parameters - edit site.def to change

# site:  $XConsortium: site.sample,v 1.9 94/04/08 17:02:06 rws Exp $
# site:  $XFree86: xc/config/cf/site.def,v 3.8 1995/05/27 01:56:50 dawes Exp $

# -----------------------------------------------------------------------
# Imake rules for building libraries, programs, scripts, and data files
# rules:  $XConsortium: Imake.rules,v 1.197.1.1 95/06/19 18:01:48 gildea Exp $
# rules:  $XFree86: xc/config/cf/Imake.rules,v 3.18 1995/07/22 09:39:32 dawes Exp $

 _NULLCMD_ = @ echo -n

          PATHSEP = /
            SHELL = /bin/sh

              TOP = ../../..
      CURRENT_DIR = .

            IMAKE = imake
           DEPEND = gccmakedep
        MKDIRHIER = mkdir -p
        CONFIGSRC = $(TOP)/config
         IMAKESRC = $(CONFIGSRC)/imake
        DEPENDSRC = $(CONFIGSRC)/util
           IXXSRC = $(UNSUPPORTEDSRC)/programs/ixx
              IXX = ixx
         IXXFLAGS = -s BaseObject -m TypeObj -r RequestObj -p Xf
      IXXINCLUDES = -i '<X11/Fresco/enter-scope.h>'

          INCROOT = /usr/X11R6/include
        USRLIBDIR = /usr/X11R6/lib
         SHLIBDIR = /usr/X11R6/lib
       LINTLIBDIR = $(USRLIBDIR)/lint
          MANPATH = /usr/X11R6/man
    MANSOURCEPATH = $(MANPATH)/man
           MANDIR = $(MANSOURCEPATH)1
        LIBMANDIR = $(MANSOURCEPATH)3
       FILEMANDIR = $(MANSOURCEPATH)5

               AR = ar clq
  BOOTSTRAPCFLAGS =
               CC = gcc
               AS = as

.SUFFIXES: .cxx

              CXX = g++
    CXXDEBUGFLAGS = -O2 -Wall
 CXXEXTRA_DEFINES =
CXXEXTRA_INCLUDES =
  CXXIDL_INCLUDES = -I$(TOP)/include
   CXXSTD_DEFINES = -Dlinux -D_POSIX_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE -DX_LOCALE
       CXXOPTIONS =
      CXXINCLUDES = $(INCLUDES) $(TOP_INCLUDES) $(CXXEXTRA_INCLUDES) $(CXXIDL_INCLUDES)
       CXXDEFINES = $(CXXINCLUDES) $(CXXSTD_DEFINES) $(THREADS_CXXDEFINES) $(CXXEXTRA_DEFINES)
         CXXFLAGS = $(CXXDEBUGFLAGS) $(CXXOPTIONS) $(THREADS_CXXFLAGS) $(CXXDEFINES)

         COMPRESS = compress
              CPP = /lib/cpp $(STD_CPP_DEFINES)
    PREPROCESSCMD = gcc -E $(STD_CPP_DEFINES)
          INSTALL = install
     INSTALLFLAGS = -c
               LD = ld
              LEX = flex -l
           LEXLIB = -lfl
             YACC = bison -y
           CCYACC = bison -y
             LINT = lint
      LINTLIBFLAG = -C
         LINTOPTS = -axz
               LN = ln -s
             MAKE = make
               MV = mv
               CP = cp

           RANLIB = ranlib
  RANLIBINSTFLAGS =

               RM = rm -f
        MANSUFFIX = 1x
     LIBMANSUFFIX = 3x
    FILEMANSUFFIX = 5x
            TROFF = psroff
         MSMACROS = -ms
              TBL = tbl
              EQN = eqn

            DVIPS = dvips
            LATEX = latex

     STD_INCLUDES =
  STD_CPP_DEFINES = -traditional -Dlinux -D_POSIX_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE -DX_LOCALE
      STD_DEFINES = -Dlinux -D_POSIX_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE -DX_LOCALE
 EXTRA_LOAD_FLAGS = -Wl,-rpath,/usr/X11R6/lib
  EXTRA_LDOPTIONS =
  EXTRA_LIBRARIES =
             TAGS = ctags

    SHAREDCODEDEF =
         SHLIBDEF =

     SHLIBLDFLAGS = -shared

         PICFLAGS = -fPIC

      CXXPICFLAGS = -fPIC

    PROTO_DEFINES = -DFUNCPROTO=15 -DNARROWPROTO

     INSTPGMFLAGS = -s

     INSTBINFLAGS = -m 0755
     INSTUIDFLAGS = -m 4755
     INSTLIBFLAGS = -m 0644
     INSTINCFLAGS = -m 0444
     INSTMANFLAGS = -m 0444
     INSTDATFLAGS = -m 0444
    INSTKMEMFLAGS = -m 4755

      PROJECTROOT = /usr/X11R6

     TOP_INCLUDES = -I$(INCROOT)

      CDEBUGFLAGS = -O2 -Wall
# ../default.linux defines CDEBUGFLAGS
include ../default.linux
        CCOPTIONS = -ansi

      ALLINCLUDES = $(INCLUDES) $(EXTRA_INCLUDES) $(TOP_INCLUDES) $(STD_INCLUDES)
       ALLDEFINES = $(ALLINCLUDES) $(STD_DEFINES) $(EXTRA_DEFINES) $(PROTO_DEFINES) $(THREADS_DEFINES) $(DEFINES)
           CFLAGS = $(CDEBUGFLAGS) $(CCOPTIONS) $(THREADS_CFLAGS) $(ALLDEFINES)
        LINTFLAGS = $(LINTOPTS) -DLINT $(ALLDEFINES) $(DEPEND_DEFINES)
         LDPRELIB = -L$(USRLIBDIR)
        LDPOSTLIB =
        LDOPTIONS = $(CDEBUGFLAGS) $(CCOPTIONS)  $(EXTRA_LDOPTIONS) $(THREADS_LDFLAGS) $(LOCAL_LDFLAGS) $(LDPRELIB)
     CXXLDOPTIONS = $(CXXDEBUGFLAGS) $(CXXOPTIONS) $(EXTRA_LDOPTIONS) $(THREADS_CXXLDFLAGS) $(LOCAL_LDFLAGS) $(LDPRELIB)

           LDLIBS = $(LDPOSTLIB) $(THREADS_LIBS) $(SYS_LIBRARIES) $(EXTRA_LIBRARIES)

           CCLINK = $(CC)

          CXXLINK = $(CXX)

     LDSTRIPFLAGS = -x
   LDCOMBINEFLAGS = -r
      DEPENDFLAGS =

        MACROFILE = linux.cf
           RM_CMD = $(RM)

    IMAKE_DEFINES =

         IRULESRC = $(CONFIGDIR)
        IMAKE_CMD = $(IMAKE) -DUseInstalled -I$(IRULESRC) $(IMAKE_DEFINES)

     ICONFIGFILES = $(IRULESRC)/Imake.tmpl $(IRULESRC)/Project.tmpl 			$(IRULESRC)/site.def $(IRULESRC)/$(MACROFILE) 			$(IRULESRC)/xfree86.cf $(IRULESRC)/xf86.rules $(IRULESRC)/xf86site.def $(IRULESRC)/host.def $(EXTRA_ICONFIGFILES)

# ----------------------------------------------------------------------
# X Window System Build Parameters and Rules
# $XConsortium: Project.tmpl,v 1.249 95/05/23 21:36:40 matt Exp $
# $XFree86: xc/config/cf/Project.tmpl,v 3.18 1995/07/22 04:11:42 dawes Exp $

# -----------------------------------------------------------------------
# X Window System make variables; these need to be coordinated with rules

           BINDIR = /usr/X11R6/bin
     BUILDINCROOT = $(TOP)
      BUILDINCDIR = $(BUILDINCROOT)/X11
      BUILDINCTOP = ..
      BUILDLIBDIR = $(TOP)/usrlib
      BUILDLIBTOP = ..
           INCDIR = $(INCROOT)/X11
           ADMDIR = /usr/adm
           LIBDIR = $(USRLIBDIR)/X11

          FONTDIR = $(LIBDIR)/fonts
         XINITDIR = $(LIBDIR)/xinit
           XDMDIR = $(LIBDIR)/xdm
           TWMDIR = $(LIBDIR)/twm
           XSMDIR = $(LIBDIR)/xsm
           NLSDIR = $(LIBDIR)/nls
       XLOCALEDIR = $(LIBDIR)/locale
        PEXAPIDIR = $(LIBDIR)/PEX
      XAPPLOADDIR = $(LIBDIR)/app-defaults
       FONTCFLAGS = -t

     INSTAPPFLAGS = $(INSTDATFLAGS)

              RGB = rgb
            FONTC = bdftopcf
        MKFONTDIR = mkfontdir

       DOCUTILSRC = $(TOP)/doc/util
       XDOCMACROS = $(DOCUTILSRC)/macros.t
       XIDXMACROS = $(DOCUTILSRC)/indexmacros.t
       PROGRAMSRC = $(TOP)/programs
           LIBSRC = $(TOP)/lib
          FONTSRC = $(TOP)/fonts
       INCLUDESRC = $(TOP)/X11
        SERVERSRC = $(TOP)/programs/Xserver
       CONTRIBSRC = $(TOP)/../contrib
   UNSUPPORTEDSRC = $(TOP)/unsupported
           DOCSRC = $(TOP)/doc
           RGBSRC = $(TOP)/programs/rgb
      BDFTOPCFSRC = $(PROGRAMSRC)/bdftopcf
     MKFONTDIRSRC = $(PROGRAMSRC)/mkfontdir
    FONTSERVERSRC = $(PROGRAMSRC)/xfs
       FONTINCSRC = $(TOP)/include/fonts
        EXTINCSRC = $(TOP)/include/extensions
     TRANSCOMMSRC = $(LIBSRC)/xtrans
   TRANS_INCLUDES = -I$(TRANSCOMMSRC)

# $XConsortium: lnxLib.tmpl,v 1.5 95/01/11 21:44:44 kaleb Exp $
# $XFree86: xc/config/cf/lnxLib.tmpl,v 3.7 1995/04/09 13:39:23 dawes Exp $

          XLIBSRC = $(LIBSRC)/X11

SOXLIBREV = 6.0
DEPXONLYLIB =
XONLYLIB =  -lX11

LINTXONLY = $(LINTLIBDIR)/llib-lX11.ln

         XLIBONLY = $(XONLYLIB)

      XEXTLIBSRC = $(LIBSRC)/Xext
   LBXXEXTLIBSRC = $(TOP)/workInProgress/lbx/lib/Xext

    XEXEXTLIBSRC = $(LIBSRC)/XExExt

SOXEXTREV = 6.0
DEPXEXTLIB =
XEXTLIB =  -lXext

LINTXEXT = $(LINTLIBDIR)/llib-lXext.ln

SOXEXTREV = 6.0
DEPLBXXEXTLIB =
LBXXEXTLIB =  -llbxXext

LINTLBXXEXT = $(LINTLIBDIR)/llib-llbxXext.ln

DEPXEXEXTLIB = $(USRLIBDIR)/libXExExt.a
XEXEXTLIB =  -lXExExt

LINTXEXEXT = $(LINTLIBDIR)/llib-lXExExt.ln

     EXTENSIONLIB = $(XEXEXTLIB) $(XEXTLIB)
  DEPEXTENSIONLIB = $(DEPXEXEXTLIB) $(DEPXEXTLIB)
    LINTEXTENSION = $(LINTXEXEXT) $(LINTXEXT)

 LINTEXTENSIONLIB = $(LINTEXTENSION)
          DEPXLIB = $(DEPEXTENSIONLIB) $(DEPXONLYLIB)
             XLIB = $(EXTENSIONLIB) $(XONLYLIB)
         LINTXLIB = $(LINTXONLYLIB)

         XAUTHSRC = $(LIBSRC)/Xau

DEPXAUTHLIB = $(USRLIBDIR)/libXau.a
XAUTHLIB =  -lXau

LINTXAUTH = $(LINTLIBDIR)/llib-lXau.ln

      XDMCPLIBSRC = $(LIBSRC)/Xdmcp

DEPXDMCPLIB = $(USRLIBDIR)/libXdmcp.a
XDMCPLIB =  -lXdmcp

LINTXDMCP = $(LINTLIBDIR)/llib-lXdmcp.ln

           XMUSRC = $(LIBSRC)/Xmu

SOXMUREV = 6.0
DEPXMULIB =
XMULIB =  -lXmu

LINTXMU = $(LINTLIBDIR)/llib-lXmu.ln

       OLDXLIBSRC = $(LIBSRC)/oldX

DEPOLDXLIB = $(USRLIBDIR)/liboldX.a
OLDXLIB =  -loldX

LINTOLDX = $(LINTLIBDIR)/llib-loldX.ln

       TOOLKITSRC = $(LIBSRC)/Xt

SOXTREV = 6.0
DEPXTOOLONLYLIB =
XTOOLONLYLIB =  -lXt

LINTXTOOLONLY = $(LINTLIBDIR)/llib-lXt.ln

      DEPXTOOLLIB = $(DEPXTOOLONLYLIB) $(DEPSMLIB) $(DEPICELIB)
         XTOOLLIB = $(XTOOLONLYLIB) $(SMLIB) $(ICELIB)
     LINTXTOOLLIB = $(LINTXTOOLONLYLIB)

       AWIDGETSRC = $(LIBSRC)/Xaw

SOXAWREV = 6.0
DEPXAWLIB =
XAWLIB =  -lXaw

LINTXAW = $(LINTLIBDIR)/llib-lXaw.ln

       XTFSRC = $(TOP)/workInProgress/Xtf

SOXTFREV = 0.7
DEPXTFLIB =
XTFLIB =  -lXtf

LINTXTF = $(LINTLIBDIR)/llib-lXtf.ln

       FRESCOSRC = $(TOP)/workInProgress/Fresco

SOFRESCOREV = 0.7
DEPFRESCOLIB =
FRESCOLIB =  -lFresco

LINTFRESCO = $(LINTLIBDIR)/llib-lFresco.ln

         XILIBSRC = $(LIBSRC)/Xi

SOXINPUTREV = 6.0
DEPXILIB =
XILIB =  -lXi

LINTXI = $(LINTLIBDIR)/llib-lXi.ln

      XTESTLIBSRC = $(LIBSRC)/Xtst

SOXTESTREV = 6.0
DEPXTESTLIB =
XTESTLIB =  -lXtst

LINTXTEST = $(LINTLIBDIR)/llib-lXtst.ln

        PEXLIBSRC = $(LIBSRC)/PEX5

SOPEXREV = 6.0
DEPPEXLIB =
PEXLIB =  -lPEX5

LINTPEX = $(LINTLIBDIR)/llib-lPEX5.ln

        XIELIBSRC = $(LIBSRC)/XIE

SOXIEREV = 6.0
DEPXIELIB =
XIELIB =  -lXIE

LINTXIE = $(LINTLIBDIR)/llib-lXIE.ln

      PHIGSLIBSRC = $(LIBSRC)/PHIGS

DEPPHIGSLIB = $(USRLIBDIR)/libphigs.a
PHIGSLIB =  -lphigs

LINTPHIGS = $(LINTLIBDIR)/llib-lphigs.ln

DEPXBSDLIB = $(USRLIBDIR)/libXbsd.a
XBSDLIB =  -lXbsd

LINTXBSD = $(LINTLIBDIR)/llib-lXbsd.ln

           ICESRC = $(LIBSRC)/ICE

SOICEREV = 6.0
DEPICELIB =
ICELIB =  -lICE

LINTICE = $(LINTLIBDIR)/llib-lICE.ln

            SMSRC = $(LIBSRC)/SM

SOSMREV = 6.0
DEPSMLIB =
SMLIB =  -lSM

LINTSM = $(LINTLIBDIR)/llib-lSM.ln

         FSLIBSRC = $(LIBSRC)/FS

DEPFSLIB = $(USRLIBDIR)/libFS.a
FSLIB =  -lFS

LINTFS = $(LINTLIBDIR)/llib-lFS.ln

         FONTLIBSRC = $(LIBSRC)/font

DEPFONTLIB = $(USRLIBDIR)/libfont.a
FONTLIB =  -lfont

LINTFONT = $(LINTLIBDIR)/llib-lfont.ln

          DEPLIBS = $(DEPXAWLIB) $(DEPXMULIB) $(DEPXTOOLLIB) $(DEPXLIB)

         DEPLIBS1 = $(DEPLIBS)
         DEPLIBS2 = $(DEPLIBS)
         DEPLIBS3 = $(DEPLIBS)

XMULIBONLY = -lXmu
XMULIB = $(XMULIBONLY) $(XTOOLLIB) $(XLIB)

        CONFIGDIR = $(LIBDIR)/config

# -----------------------------------------------------------------------
# start of Imakefile

###########################################################################
# @(#)XView.cf	1.10 09 Feb 1992 SMI
# XView.cf for XView release 3.0.  Specify site preferences here.

###########################################################################
# @(#)XView.tmpl	1.15 12 May 1993 SMI
# XView.tmpl for XView release 3.2.  Template to add additional config
# rules to those supplied with X Version 11 release 5.

              MKDIRHIER = mkdirhier

         LOCALE_DEFINES = -DOS_HAS_LOCALE

           MMAP_DEFINES = -DOS_HAS_MMAP

           XV_CFLAGS.optimized =
           XV_CFLAGS.shared = -DDYNAMICLIB

           SHLIBLDFLAGS=-shared

                DEFINES = $(LOCALE_DEFINES) $(MMAP_DEFINES) $(XV_ALLOC_DEFINES) 			  $(XV_STRDUP_DEFINES) $(XV_CFLAGS.optimized) $(XV_CFLAGS.shared)

              XVDESTDIR = $$OPENWINHOME
           LIBRARY_DEST = $(XVDESTDIR)/lib
      LINT_LIBRARY_DEST = $(XVDESTDIR)/lib/lint
                 BINDIR = $(XVDESTDIR)/bin
                MANPATH = $(XVDESTDIR)/man
            HEADER_DEST = $(XVDESTDIR)/include
               SRC_DEST = $(XVDESTDIR)/share/src/xview
               DOC_DEST = $(XVDESTDIR)/share/doc/xview

              CONFIGDIR = $(LIBRARY_DEST)/xview/config

               XVLIBSRC = $(TOP)/lib
              HELP_DEST = $(LIBRARY_DEST)/help
    LIBRARY_DEST_EXTRAS = $(LIBRARY_DEST)
       TEXT_EXTRAS_MENU = $(LIBRARY_DEST)/.text_extras_menu
                     AR = ar rv
                  CHMOD = chmod

                DEPXLIB =

            BUILDINCDIR = $(TOP)/build
            BUILDINCTOP = ../../..

                 INCDIR = /include

               XVPUBINC = xview
               XVPRIINC = xview_private
               OLPUBINC = olgx
               OLPRIINC = olgx_private
               PSPUBINC = xvps
               PSPRIINC = xvps_private
               IMAGEINC = images
              BITMAPINC = bitmaps
         PIXRECT_INCDIR = $(INCDIR)/pixrect
           XVPUB_INCDIR = $(INCDIR)/$(XVPUBINC)
           XVPRI_INCDIR = $(INCDIR)/$(XVPRIINC)
           OLPUB_INCDIR = $(INCDIR)/$(OLPUBINC)
           OLPRI_INCDIR = $(INCDIR)/$(OLPRIINC)
           PSPUB_INCDIR = $(INCDIR)/$(PSPUBINC)
           PSPRI_INCDIR = $(INCDIR)/$(PSPRIINC)
           IMAGE_INCDIR = $(INCDIR)/$(IMAGEINC)
          BITMAP_INCDIR = $(INCDIR)/$(BITMAPINC)
    PIXRECT_HEADER_DEST = $(HEADER_DEST)/pixrect
      XVPUB_HEADER_DEST = $(HEADER_DEST)/$(XVPUBINC)
      XVPRI_HEADER_DEST = $(HEADER_DEST)/$(XVPRIINC)
      OLPUB_HEADER_DEST = $(HEADER_DEST)/$(OLPUBINC)
      OLPRI_HEADER_DEST = $(HEADER_DEST)/$(OLPRIINC)
      PSPUB_HEADER_DEST = $(HEADER_DEST)/$(PSPUBINC)
      PSPRI_HEADER_DEST = $(HEADER_DEST)/$(PSPRIINC)
            IMAGES_DEST = $(HEADER_DEST)/$(IMAGEINC)
            BITMAP_DEST = $(HEADER_DEST)/$(BITMAPINC)

                LINTDIR = $(LINT_LIBRARY_DEST)

                    PIC = -fPIC
               STRCONST =
                   XSTR = xstr
             XSTR_ARRAY = _libxview_xstr
                OBJSORT = ./objsort.sh
                OBJLIST = ./lorder-sparc
     INSTSHAREDLIBFLAGS = -m 0755

            DEPXVIEWLIB =
               XVIEWLIB = -L$(LIBRARY_DEST) -lxview

            DEPOLGXLIB =
               OLGXLIB = -L$(LIBRARY_DEST) -lolgx

            DEPXVPSLIB =
               XVPSLIB = -L$(LIBRARY_DEST) -lxvps

             XLIB = $(EXTENSIONLIB)  -lX11

             SYSV_CLIENT_LIB =
             XVCLIENTSYSDEFINES = -DSUNOS41

    XVCLIENTI18NDEFINES = -DOW_I18N_L3
    XVCLIENTMISCLIB =
    XVCLIENTINCLUDES =

    XVCLIENTDEFINES = $(XVCLIENTSYSDEFINES) $(XVCLIENTI18NDEFINES)

             SOXVIEWREV = 3.2
              SOOLGXREV = 3.2

                  SODIR = shared
                  SADIR = unshared

             SHARED_DIR = $(TARGETDIR)xshared
           UNSHARED_DIR = $(TARGETDIR)xunshared
      XSTR_STRINGS_FILE = $(TARGETDIR)xstrings

###########################################################################
# @(#)XView.rules	1.8 12 May 1993 SMI
# XView.rules for XView release 3.0.  Imake rules needed by distribution.

# $XConsortium: Library.tmpl,v 1.11 91/07/27 13:10:13 rws Exp $

_NULLCMD_ = @ echo -n

         CC = gcc
  CCOPTIONS = -ansi
STD_DEFINES = -Dlinux -D_POSIX_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE -DX_LOCALE

###########################################################################
# @(#)Imakefile	1.8 28 Jun 1993 SMI

HDRSPUBLIC=     cms.h
HDRSPRIVATE=    cms_impl.h

SRCS=	cms.c	cms_pblc.c
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

RM = rm -f
CC=cc
MV=mv
CP=cp
AR=ar r
DEPEND=gccmakedep
DEPEND_DEFINES=
CHECKOUT,v=$(CO) $(COFLAGS) $<

HEADERS= ${HDRSPUBLIC} ${HDRSPRIVATE}
BUILDINCDIR = ../../build
INCLUDES= -I$(BUILDINCDIR)/include
INCROOT = /usr/X11R6/include
TOP_INCLUDES = -I$(INCROOT)
PROTO_DEFINES = -DFUNCPROTO=15 -DNARROWPROTO
LOCALE_DEFINES = -DOS_HAS_LOCALE
MMAP_DEFINES = -DOS_HAS_MMAP
DEFINES = $(LOCALE_DEFINES) $(MMAP_DEFINES)
ALLDEFINES = $(INCLUDES) $(TOP_INCLUDES) $(STD_DEFINES) $(PROTO_DEFINES) $(DEFINES)
CFLAGS = $(CDEBUGFLAGS) -ansi $(ALLDEFINES)

ALLFILES = $(SRCS) $(HEADERS)

all: preparedirs retrieve lib${LIBTARGET}.so.${SOREV} lib${LIBTARGET}.a

.c.o:
	$(RM) $@ $(SODIR)32/$@ $(SODIR)64/$@
	$(CC) -m32 $(PIC) $(CFLAGS) -c $*.c
	$(MV) $@ $(SODIR)32/$*.o
	$(CC) $(PIC) $(CFLAGS) -c $*.c
	$(MV) $@ $(SODIR)64/$*.o

clean::
	$(RM) strings*
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

retrieve: $(HEADERS)

# -----------------------------------------------------------------------
# common rules for all Makefiles - do not edit

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

install:
	@echo install NOT FROM HERE

install.man::
	@echo "install.man in $(CURRENT_DIR) done"

install.linkkit::
	@echo "install.linkkit in $(CURRENT_DIR) done"

Makefiles::

includes::

depend::

# -----------------------------------------------------------------------
# dependencies generated by makedepend

# DO NOT DELETE
cms.o: cms.c /usr/include/stdc-predef.h /usr/include/X11/Xlib.h \
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
 /usr/include/bits/struct_rwlock.h /usr/include/X11/X.h \
 /usr/include/X11/Xfuncproto.h /usr/include/X11/Xosdefs.h \
 /usr/include/X11/Xutil.h /usr/include/X11/keysym.h \
 /usr/include/X11/keysymdef.h /usr/include/X11/Xatom.h \
 ../../build/include/xview_private/i18n_impl.h \
 ../../build/include/xview/xv_c_types.h \
 ../../build/include/xview/pkg.h ../../build/include/xview/attr.h \
 ../../build/include/xview/base.h /usr/include/string.h \
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
 ../../build/include/xview/notify.h /usr/include/sys/wait.h \
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
 /usr/include/sys/signal.h ../../build/include/xview/xv_i18n.h \
 /usr/include/libintl.h /usr/include/locale.h /usr/include/bits/locale.h \
 ../../build/include/xview/cms.h \
 ../../build/include/xview/generic.h \
 ../../build/include/xview/pkg_public.h \
 ../../build/include/xview/xv_error.h \
 ../../build/include/xview/sun.h \
 ../../build/include/xview_private/cms_impl.h \
 ../../build/include/xview/screen.h \
 ../../build/include/xview_private/scrn_vis.h \
 ../../build/include/xview/server.h
cms_pblc.o: cms_pblc.c /usr/include/stdc-predef.h \
 ../../build/include/xview/cms.h /usr/include/sys/types.h \
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
 /usr/include/bits/struct_rwlock.h ../../build/include/xview/generic.h \
 ../../build/include/xview/xv_c_types.h /usr/include/X11/Xfuncproto.h \
 ../../build/include/xview/pkg_public.h \
 ../../build/include/xview/pkg.h ../../build/include/xview/attr.h \
 ../../build/include/xview/base.h /usr/include/string.h \
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
 ../../build/include/xview/notify.h /usr/include/sys/wait.h \
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
 /usr/include/sys/signal.h ../../build/include/xview/xv_error.h \
 ../../build/include/xview/sun.h \
 ../../build/include/xview/defaults.h \
 ../../build/include/xview_private/cms_impl.h /usr/include/X11/Xlib.h \
 /usr/include/X11/X.h /usr/include/X11/Xosdefs.h /usr/include/X11/Xutil.h \
 /usr/include/X11/keysym.h /usr/include/X11/keysymdef.h \
 ../../build/include/xview/screen.h \
 ../../build/include/xview_private/scrn_vis.h \
 ../../build/include/xview_private/i18n_impl.h \
 ../../build/include/xview/xv_i18n.h /usr/include/libintl.h \
 /usr/include/locale.h /usr/include/bits/locale.h \
 ../../build/include/xview_private/portable.h \
 ../../build/include/olgx/olgx.h
