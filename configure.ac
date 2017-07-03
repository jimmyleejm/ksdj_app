dnl Process this file with autoconf to produce a configure script.

AC_INIT([linphone],[3.6.1],[linphone-developers@nongnu.org])
AC_CANONICAL_SYSTEM
AC_CONFIG_SRCDIR([coreapi/linphonecore.c])

dnl Source packaging numbers

LINPHONE_MAJOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f1)
LINPHONE_MINOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f2)
LINPHONE_MICRO_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f3)
LINPHONE_EXTRA_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f4)

dnl program extension
LINPHONE_VERSION=$LINPHONE_MAJOR_VERSION.$LINPHONE_MINOR_VERSION.${LINPHONE_MICRO_VERSION}
if test "$LINPHONE_EXTRA_VERSION" != "" ;then
	LINPHONE_VERSION=$LINPHONE_VERSION.${LINPHONE_EXTRA_VERSION}
fi

LIBLINPHONE_SO_CURRENT=5 dnl increment this number when you add/change/remove an interface
LIBLINPHONE_SO_REVISION=0 dnl increment this number when you change source code, without changing interfaces; set to 0 when incrementing CURRENT
LIBLINPHONE_SO_AGE=0 dnl increment this number when you add an interface, set to 0 if you remove an interface

LIBLINPHONE_SO_VERSION=$LIBLINPHONE_SO_CURRENT:$LIBLINPHONE_SO_REVISION:$LIBLINPHONE_SO_AGE

AC_SUBST(LIBLINPHONE_SO_VERSION, $LIBLINPHONE_SO_VERSION)
AC_SUBST(LINPHONE_VERSION)

AC_MSG_NOTICE([$PACKAGE_NAME-$PACKAGE_VERSION		A full featured audio/video sip phone.])
AC_MSG_NOTICE([licensed under the terms of the General Public License (GPL)])

AM_INIT_AUTOMAKE([1.9 tar-pax])
AC_SUBST([LIBTOOL_DEPS])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],)
AC_SUBST([docdir], [${datadir}/doc])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])
AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AC_C_INLINE
AC_HEADER_STDC
AM_PROG_CC_C_O
AC_CHECK_PROGS(MD5SUM,[md5sum md5])
AM_CONDITIONAL(HAVE_MD5SUM,test -n $MD5SUM)

AC_SUBST(ACLOCAL_MACOS_FLAGS)
AC_SUBST(CONSOLE_FLAGS)
AC_SUBST(GUI_FLAGS)

dnl localization tools
IT_PROG_INTLTOOL([0.40], [no-xml])

AM_CONDITIONAL(BUILD_TESTS,test x$build_tests != xno)

dnl Initialize libtool
LT_INIT([win32-dll shared disable-static])

dnl Enable library dependencies linking
AC_ARG_ENABLE(deplibs-link,
	[AS_HELP_STRING([--disable-deplibs-link ], [Disable library dependencies linking (might break builds)])],
	[enable_deplibs_linking="$enableval"],
	[enable_deplibs_linking="yes"]
)
AC_MSG_NOTICE([Enable library dependencies linking: $enable_interlib_deps])
if test "${enable_deplibs_linking}" == "yes"; then
	link_all_deplibs=yes
	link_all_deplibs_CXX=yes
else
	link_all_deplibs=no
	link_all_deplibs_CXX=no
fi


dnl Add the languages which your application supports here.
PKG_PROG_PKG_CONFIG
ALL_LINGUAS="zh_CN zh_TW"
AC_SUBST(ALL_LINGUAS)
AC_DEFINE_UNQUOTED(LINPHONE_ALL_LANGS, "$ALL_LINGUAS", [All supported languages])

dnl gettext macro does not work properly under mingw. And we want to use the one provided by GTK.
	
dnl AM_GNU_GETTEXT pollutes CPPFLAGS: workaround this.
CPPFLAGS_save=$CPPFLAGS
AM_GNU_GETTEXT([external])
AC_SUBST(INTLLIBS)
CPPFLAGS=$CPPFLAGS_save
LIBS="$LIBS $LIBINTL"

GETTEXT_PACKAGE=linphone
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[The name of the gettext package name])
dnl AC_CHECK_LIB(intl,libintl_gettext)

AC_CHECK_FUNCS([get_current_dir_name strndup stpcpy] )

dnl conditionnal build of console interface.
AC_ARG_ENABLE(console_ui,
	[AS_HELP_STRING([--enable-console_ui=[yes/no]], [Turn on or off compilation of console interface (default=yes)])],
	[case "${enableval}" in
		yes)	console_ui=true ;;
		no)	console_ui=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-console_ui) ;;
	esac],
	[console_ui=true]
)

dnl conditionnal build of tools.
AC_ARG_ENABLE(tools,
	[AS_HELP_STRING([--enable-tools=[yes/no]], [Turn on or off compilation of console interface (default=yes)])],
	[case "${enableval}" in
		yes)	build_tools=true ;;
		no)	build_tools=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-tools) ;;
	esac],
	[build_tools=check]
)

dnl check for installed version of libupnp
AC_ARG_ENABLE(upnp,
	[AS_HELP_STRING([--disable-upnp], [Disable uPnP support])],
	[case "${enableval}" in
		yes)	build_upnp=true ;;
		no)	build_upnp=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-upnp) ;;
	esac],
	[build_upnp=auto]
)

if test "$build_upnp" != "false" ; then
	PKG_CHECK_MODULES([LIBUPNP], [libupnp], 
		[if  pkg-config --atleast-version=1.6 "libupnp < 1.7"; then
			build_upnp=true
		else
			AC_MSG_ERROR([libupnp >= 1.6 < 1.5 required.])
		fi],
		[if test "$build_upnp" == "true" ; then
			AC_MSG_ERROR([libupnp not found.])
		else
			build_upnp=false
		fi]
	)

fi

AM_CONDITIONAL(BUILD_UPNP, test x$build_upnp != xfalse)
if test "$build_upnp" != "false" ; then
	AC_DEFINE(BUILD_UPNP, 1, [Define if upnp enabled])
fi

dnl check libxml2 (needed for tools)
if test "$build_tools" != "false" ; then
	PKG_CHECK_MODULES(LIBXML2, [libxml-2.0],[],
		[if test "$build_tools" = "true" ; then
			AC_MSG_ERROR([Could not found libxml2, tools cannot be compiled.])
		else
			build_tools=false
		fi]
	)
fi

AM_CONDITIONAL(BUILD_TOOLS, test x$build_tools != xfalse)
if test "$build_tools" != "false" ; then
	build_tools=true
	AC_DEFINE(BUILD_TOOLS, 1, [Define if tools enabled] ) 
fi


AC_ARG_ENABLE(notify,
	[AS_HELP_STRING([--enable-notify=[yes/no]], [Enable libnotify support (default=yes)])],
	[case "${enableval}" in
		yes)	notify=true ;;
		no)	notify=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-notify) ;;
	esac],
	[notify=true]
)


dnl os-specific problems not handled by existing macros.
case "$host_os" in
	*freebsd*)
		LDFLAGS="$LDFLAGS -pthread"
	;;
esac

case "$host_cpu" in
	*arm*)
		AC_DEFINE(__ARM__,1,[Defined if we are compiling for arm processor])
		use_arm_toolchain=yes
	;;
esac

AC_ARG_WITH(configdir,
	[AS_HELP_STRING([--with-configdir], [Set a APPDATA subdir where linphone is supposed to find its config (windows only)])],
	[ configdir=${withval}],[ configdir="Linphone" ])

AC_DEFINE_UNQUOTED(LINPHONE_CONFIG_DIR,"$configdir",[Windows appdata subdir where linphonerc can be found])


AC_ARG_ENABLE(date,
	[AS_HELP_STRING([--enable-date], [Use build date in internal version number])],
	[case "${enableval}" in
		yes)	use_date=yes ;;
		no)	use_date=no ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-date) ;;
	esac],
	[use_date=no]
)

if test x$use_date =  xyes ; then
	AC_DEFINE(USE_BUILDDATE_VERSION,1,[Tell whether date_version.h must be used])
fi


dnl enable ipv6 support
AC_ARG_ENABLE(ipv6,
	[AS_HELP_STRING([--enable-ipv6], [Turn on ipv6 support])],
	[case "${enableval}" in
		yes)	ipv6=true;;
		no)	ipv6=false;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-ipv6) ;;
	esac],
	[ipv6=true]
)
IPV6_CFLAGS=
if test x$ipv6 = xtrue ; then
	IPV6_CFLAGS=-DINET6
fi
AC_SUBST(IPV6_CFLAGS)

dnl enable timestamp support
AC_ARG_ENABLE(ntp-timestamp,
	[AS_HELP_STRING([--enable-ntp-timestamp], [Turn on NTP timestamping on received packet])],
	[case "${enableval}" in
		yes)	ntptimestamp=true;;
		no)	ntptimestamp=false;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-ntp-timestamp) ;;
	esac],
	[ntptimestamp=false]
)

AC_ARG_ENABLE(debug,
	[AS_HELP_STRING([--enable-debug=[yes/no]], [Enables the display of traces showing the execution of the library. (default=yes)])],
	[case "${enableval}" in
		yes)	debug_enabled=yes;;
		no)	debug_enabled=no;;
		*)	AC_MSG_ERROR("Bad value for --enable-debug");;
	esac],
	[debug_enabled=no]
)

dnl enable truespeech codec support
AC_ARG_ENABLE(truespeech,
	[AS_HELP_STRING([--enable-truespeech], [Turn on TrueSpeech support (x86 only)])],
	[case "${enableval}" in
		yes)	truespeech=true;;
		no)	truespeech=false;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-truespeech) ;;
	esac],
	[truespeech=false]
)
TRUESPEECH_CFLAGS=
if test x$truespeech = xtrue ; then
	TRUESPEECH_CFLAGS=-DTRUESPEECH
fi
AC_SUBST(TRUESPEECH_CFLAGS)
AM_CONDITIONAL([BUILD_TRUESPEECH], [test x$truespeech = xtrue])

AC_ARG_ENABLE(nonstandard-gsm,
	[AS_HELP_STRING([--enable-nonstandard-gsm], [Enable GSM codec at nonstandard rates (11025hz, 16000hz)])],
	[case "${enableval}" in
		yes)
			exotic_gsm=yes
	 		AC_DEFINE(ENABLE_NONSTANDARD_GSM,1,[Defined when using gsm at nonstandard rates])
	 	;;
		no)	exotic_gsm=no ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-nonstandard-gsm) ;;
	esac],
	[exotic_gsm=no]
)


dnl support for RSVP (by Vincent Maury)
AC_ARG_ENABLE(rsvp,
	[AS_HELP_STRING([--enable-rsvp], [Enable support for QoS reservations.])],
	AC_DEFINE(VINCENT_MAURY_RSVP,1,[Tell whether RSVP support should be compiled.])
)

if test "x${prefix}" = "xNONE"; then
	package_prefix=${ac_default_prefix}
else
	package_prefix=${prefix}
fi


dnl Set PACKAGE_LOCALE_DIR in config.h.
DATADIRNAME=share
AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR, "${package_prefix}/${DATADIRNAME}/locale",[Defines the place where locales can be found])

AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${package_prefix}/${DATADIRNAME}",[Defines the place where data are found])

dnl Set PACKAGE_SOUND_DIR in config.h.
AC_DEFINE_UNQUOTED(PACKAGE_SOUND_DIR, "${package_prefix}/${DATADIRNAME}/sounds/linphone",[Defines the place where linphone sounds are found])


dnl check if we have the getifaddrs() sytem call
AC_CHECK_FUNCS(getifaddrs)

dnl check for osip2
LP_CHECK_OSIP2

dnl conditionnal build for ssl
AC_ARG_ENABLE(ssl,
	[AS_HELP_STRING([--enable-ssl], [Turn on ssl support compiling. Required for sip tls. (default=false)])],
	[case "${enableval}" in
		yes)	build_ssl=true ;;
		no)	build_ssl=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-ssl) ;;
	esac],
	[build_ssl=false]
)

if test "$build_ssl" = "true"; then
	PKG_CHECK_MODULES(OPENSSL, libssl >= 0.9.8)
fi
dnl setup flags for exosip library
LP_SETUP_EXOSIP

if test "$console_ui" = "true" ; then
	dnl check gnu readline
	LP_CHECK_READLINE
else
	echo "Console interface compilation is disabled."
fi

AC_WORDS_BIGENDIAN

dnl conditionnal build of video support
AC_ARG_ENABLE(video,
	[AS_HELP_STRING([--enable-video], [Turn on video support compiling])],
	[case "${enableval}" in
		yes)	video=true ;;
		no)	video=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-video) ;;
	esac],
	[video=true]
)

AC_ARG_WITH(ffmpeg,
	[AS_HELP_STRING([--with-ffmpeg], [Sets the installation prefix of ffmpeg, needed for video support. (default=/usr)])],
	[ ffmpegdir=${withval}],
	[ ffmpegdir=/usr ]
)

if test "$video" = "true"; then
	
	AC_DEFINE(VIDEO_ENABLED,1,[defined if video support is available])
fi

AC_ARG_ENABLE(alsa,
	[AS_HELP_STRING([--enable-alsa], [Turn on alsa native support compiling])],
	[case "${enableval}" in
		yes)	alsa=true ;;
		no)	alsa=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-alsa) ;;
	esac],
	[alsa=true]
)

AC_ARG_ENABLE(zrtp,
	[AS_HELP_STRING([--enable-zrtp], [Turn on zrtp support])],
	[case "${enableval}" in
		yes)	zrtp=true ;;
		no)	zrtp=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-zrtp) ;;
	esac],
	[zrtp=false]
)


AC_ARG_ENABLE(portaudio,
	[AS_HELP_STRING([--enable-portaudio], [Turn on portaudio native support compiling])],
	[case "${enableval}" in
		yes)	portaudio=true ;;
		no)	portaudio=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-portaudio) ;;
	esac],
	[portaudio=false]
)

dnl build console if required
AM_CONDITIONAL(BUILD_CONSOLE, test x$console_ui = xtrue)

dnl special things for arm-linux cross compilation toolchain
AM_CONDITIONAL(ARMBUILD, test x$use_arm_toolchain = xyes)

dnl compilation of zrtp interface
AM_CONDITIONAL(BUILD_ZRTP, test x$zrtp = xtrue)

dnl check getenv
AH_TEMPLATE([HAVE_GETENV])
AC_CHECK_FUNC([getenv], AC_DEFINE([HAVE_GETENV], [1], [If present, the getenv function allows fim to read environment variables.]))

dnl
AC_MSG_CHECKING([for sighandler_t])
AC_TRY_COMPILE([#include <signal.h>],[sighandler_t *f;],
has_sighandler_t=yes,has_sighandler_t=no)
AC_MSG_RESULT($has_sighandler_t)
if test "$has_sighandler_t" = "yes" ; then
	AC_DEFINE( HAVE_SIGHANDLER_T, 1, [Define if sighandler_t available] )
fi

AC_ARG_ENABLE(assistant,
	[AS_HELP_STRING([--enable-assistant], [Turn on assistant compiling])],
	[case "${enableval}" in
		yes)	build_wizard=true ;;
		no)	build_wizard=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-assistant) ;;
	esac],
	[build_wizard=check]
)

dnl check libsoup (needed for wizard)
if test "$build_wizard" != "false" ; then
	PKG_CHECK_MODULES(LIBSOUP, [libsoup-2.4 >= 2.26],[],
		[if test "$build_wizard" = "true" ; then
			AC_MSG_ERROR([Could not found libsoup, assistant cannot be compiled.])
		else
			build_wizard=false
		fi]
	)
fi

AC_SUBST(LIBSOUP_CFLAGS)
AC_SUBST(LIBSOUP_LIBS)
AM_CONDITIONAL(BUILD_WIZARD, test x$build_wizard != xfalse)
if test "$build_wizard" != "false" ; then
	build_wizard=true
	 AC_DEFINE(BUILD_WIZARD, 1, [Define if wizard enabled] ) 
fi

AC_CHECK_HEADERS(libudev.h)
AC_CHECK_LIB(udev,udev_new)

##################################################
# Stricter build options (after external packages)
##################################################


AC_ARG_ENABLE(strict,
	AC_HELP_STRING([--enable-strict], [Build with stricter options (gcc only) @<:@yes@:>@]),
	[strictness="${enableval}"],
	[strictness=yes]
)

STRICT_OPTIONS="-Wall "

if test "$strictness" = "yes" ; then
	STRICT_OPTIONS="$STRICT_OPTIONS -Werror"
	CFLAGS="$CFLAGS -fno-strict-aliasing"
fi

AC_SUBST(STRICT_OPTIONS)

top_srcdir=`dirname $0`

AC_ARG_ENABLE([external-mediastreamer],
	[AS_HELP_STRING([--enable-external-mediastreamer],[Use external mediastreamer library])],,
	[enable_external_mediastreamer=no]
)

AS_CASE($enable_external_mediastreamer,
	[yes],
	[PKG_CHECK_MODULES([MEDIASTREAMER], [mediastreamer])
	MS2_VERSION=`$PKG_CONFIG --modversion mediastreamer`],
	[no],
	[AC_CONFIG_SUBDIRS( mediastreamer2 )
	MEDIASTREAMER_DIR=${top_srcdir}/mediastreamer2
	MEDIASTREAMER_CFLAGS="-I\$(top_srcdir)/mediastreamer2/include"
	MEDIASTREAMER_LIBS="\$(top_builddir)/mediastreamer2/src/libmediastreamer_base.la \$(top_builddir)/mediastreamer2/src/libmediastreamer_voip.la"
dnl need to temporary change quotes to allow square brackets
	changequote(<<, >>)
	MS2_VERSION=`grep -e '^.C_INIT(' $MEDIASTREAMER_DIR/configure.ac | sed -e 's:\([^(]\+\)(\[mediastreamer\],\[\(.*\)\]):\2:g'`
	changequote([, ])
	MS2_DIR=mediastreamer2],
	[AC_MSG_ERROR([bad value '${enable_external_mediastreamer}' for --enable-external-mediastreamer])]
)

AC_SUBST(MEDIASTREAMER_CFLAGS)
AC_SUBST(MEDIASTREAMER_LIBS)
AC_SUBST([MS2_VERSION])
AC_SUBST([MS2_DIR])



AC_ARG_ENABLE(tunnel,
	[AS_HELP_STRING([--enable-tunnel=[yes/no]], [Turn on compilation of tunnel support (default=no)])],
	[case "${enableval}" in
		yes)	enable_tunnel=true ;;
		no)	enable_tunnel=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-tunnel) ;;
	esac],
	[enable_tunnel=false]
)
AM_CONDITIONAL(BUILD_TUNNEL, test x$enable_tunnel = xtrue)
if test x$enable_tunnel = xtrue; then
	PKG_CHECK_MODULES(TUNNEL, tunnel >= 0.3.3)
	AC_DEFINE(TUNNEL_ENABLED,1,[Tells tunnel extension is built-in])
fi

AC_ARG_ENABLE(msg-storage,
	[AS_HELP_STRING([--enable-msg-storage=[yes/no]], [Turn on compilation of message storage (default=auto)])],
	[case "${enableval}" in
		yes)	enable_msg_storage=true ;;
		no)	enable_msg_storage=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-msg-storage) ;;
	esac],
	[enable_msg_storage=auto]
)


AM_CONDITIONAL(BUILD_MSG_STORAGE, test x$enable_msg_storage = xtrue)
if test x$enable_msg_storage != xfalse; then
	PKG_CHECK_MODULES(SQLITE3,[ sqlite3 >= 3.7.0],[
		SQLITE3_CFLAGS+="-DMSG_STORAGE_ENABLED"
		AC_SUBST(SQLITE3_CFLAGS)
		AC_SUBST(SQLITE3_LIBS)
		enable_msg_storage=true
		],[
		if test x$enable_msg_storage = xtrue; then 
			AC_MSG_ERROR([sqlite3 required for message storage not found.])
		fi
		enable_msg_storage=false
		] )
	
fi


dnl check for db2html (docbook) to generate html user manual
AC_CHECK_PROG(have_sgmltools, sgmltools, yes, no)
AM_CONDITIONAL(ENABLE_MANUAL, test x$have_sgmltools$build_manual = xyesyes )

dnl for external use of linphone libs
LINPHONE_CFLAGS="-I${includedir} -I${includedir}/linphone"
LINPHONE_LIBS="-L${libdir} -llinphone"


AC_SUBST(LINPHONE_CFLAGS)
AC_SUBST(LINPHONE_LIBS)

AC_DEFINE_UNQUOTED(LINPHONE_VERSION, "$PACKAGE_VERSION", [Linphone\'s version number])

AC_DEFINE_UNQUOTED(LINPHONE_PLUGINS_DIR, "${package_prefix}/lib/liblinphone/plugins" ,[path of liblinphone plugins, not mediastreamer2 plugins])
LINPHONE_PLUGINS_DIR="${package_prefix}/lib/liblinphone/plugins"
AC_SUBST(LINPHONE_PLUGINS_DIR)

AC_ARG_ENABLE(external-ortp,
	[AS_HELP_STRING([--enable-external-ortp], [Use external oRTP library])],
	[case "${enableval}" in
		yes)	external_ortp=true ;;
		no)	external_ortp=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-external-ortp) ;;
	esac],
	[external_ortp=false]
)

if test "$external_ortp" = 'true'; then
	PKG_CHECK_MODULES([ORTP], [ortp])
	ORTP_VERSION=`$PKG_CONFIG --modversion ortp`
else
	AC_CONFIG_SUBDIRS( oRTP )
	ORTP_CFLAGS="-I\$(top_srcdir)/oRTP/include"
	ORTP_LIBS="\$(top_builddir)/oRTP/src/libortp.la"
	if test x$ac_cv_c_bigendian = xyes ; then
		ORTP_CFLAGS="$ORTP_CFLAGS -DORTP_BIGENDIAN"
	fi
	if test x$ntptimestamp = xtrue ; then
		ORTP_CFLAGS="$ORTP_CFLAGS -DORTP_TIMESTAMP"
	fi
	ORTP_DIR=oRTP
	changequote(<<, >>)
	ORTP_VERSION=`grep -E ^[AC]+_INIT ${top_srcdir}/oRTP/configure.ac | sed -e 's:^.*_INIT(.*,\[\(.*\)\]):\1:g'`
	changequote([, ])
fi
AC_SUBST(ORTP_CFLAGS)
AC_SUBST(ORTP_LIBS)
AC_SUBST([ORTP_VERSION])
AC_SUBST([ORTP_DIR])

AC_ARG_ENABLE(tests_enabled,
	[AS_HELP_STRING([--disable-tests], [Disable compilation of tests])],
	[case "${enableval}" in
		yes)	tests_enabled=true ;;
		no)	tests_enabled=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-tests) ;;
	esac],
	[tests_enabled=false]
)
AM_CONDITIONAL(ENABLE_TESTS, test x$tests_enabled = xyes)



dnl ##################################################
dnl # Check for doxygen
dnl ##################################################

AC_PATH_PROG(DOXYGEN,doxygen,false)
AM_CONDITIONAL(HAVE_DOXYGEN, test $DOXYGEN != false)


AC_CONFIG_FILES([ 
	Makefile
	m4/Makefile
	po/Makefile.in
	coreapi/Makefile
	coreapi/help/Makefile
	coreapi/help/Doxyfile
	console/Makefile
	share/Makefile
	share/C/Makefile
	share/linphone.pc
	share/linphone.desktop
	scripts/Makefile
	tools/Makefile
	linphone.spec
])

AC_OUTPUT

echo "Linphone build configuration ended."
echo "Summary of build options:"
printf "* %-30s %s\n" "Video support"		$video
printf "* %-30s %s\n" "Account assistant"	$build_wizard
printf "* %-30s %s\n" "Console interface"	$console_ui
printf "* %-30s %s\n" "Tools"			$build_tools
printf "* %-30s %s\n" "Message storage"		$enable_msg_storage
printf "* %-30s %s\n" "zRTP encryption (GPLv3)"	$zrtp
printf "* %-30s %s\n" "uPnP support" 		$build_upnp

if test "$enable_tunnel" = "true" ; then
	printf "* Tunnel support\t\ttrue\n"
fi

echo "Now type 'make' to compile, and then 'make install' as root to install it."
