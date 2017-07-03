dnl -*- autoconf -*-
AC_DEFUN([MS_CHECK_DEP],[
	dnl $1=dependency description
	dnl $2=dependency short name, will be suffixed with _CFLAGS and _LIBS
	dnl $3=headers's place
	dnl $4=lib's place
	dnl $5=header to check
	dnl $6=lib to check
	dnl $7=function to check in library
	
	dep_name=$2
	dep_headersdir=$3
	dep_libsdir=$4
	dep_header=$5
	dep_lib=$6
	dep_funclib=$7
	other_libs=$8	
	
	CPPFLAGS_save=$CPPFLAGS
	LDFLAGS_save=$LDFLAGS
	LIBS_save=$LIBS

	case "$target_os" in
		*mingw*)
			ms_check_dep_mingw_found=yes
		;;
	esac
	if test "$ms_check_dep_mingw_found" != "yes" ; then
		CPPFLAGS=`echo "-I$dep_headersdir"|sed -e "s:-I/usr/include[\ ]*$::"`
		LDFLAGS=`echo "-L$dep_libsdir"|sed -e "s:-L/usr/lib\(64\)*[\ ]*::"`
	else
		CPPFLAGS="-I$dep_headersdir"	
		LDFLAGS="-L$dep_libsdir"
	fi


	LIBS="-l$dep_lib"

	
	$2_CFLAGS="$CPPFLAGS"
	$2_LIBS="$LDFLAGS $LIBS"

	AC_CHECK_HEADERS([$dep_header],[AC_CHECK_LIB([$dep_lib],[$dep_funclib],found=yes,found=no, [$other_libs])
	],found=no)
	
	if test "$found" = "yes" ; then
		eval $2_found=yes
	else
		eval $2_found=no
		eval $2_CFLAGS=
		eval $2_LIBS=
	fi
	AC_SUBST($2_CFLAGS)
	AC_SUBST($2_LIBS)
	CPPFLAGS=$CPPFLAGS_save
	LDFLAGS=$LDFLAGS_save
	LIBS=$LIBS_save
])


AC_DEFUN([MS_CHECK_VIDEO],[

	dnl conditionnal build of video support
	AC_ARG_ENABLE(video,
		  [AS_HELP_STRING([--enable-video], [Turn on video support compiling (default=yes])],
		  [case "${enableval}" in
			yes) video=true ;;
			no)  video=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --enable-video) ;;
		  esac],[video=true])

	AC_ARG_ENABLE(ffmpeg,
		  [AS_HELP_STRING([--disable-ffmpeg], [Disable ffmpeg support])],
		  [case "${enableval}" in
			yes) ffmpeg=true ;;
			no)  ffmpeg=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --disable-ffmpeg) ;;
		  esac],[ffmpeg=true])
	
	if test "$video" = "true"; then
		
		if test "$macosx_found" = "yes" ; then
			dnl we use quartz+opengl directly on mac os for video display.
			enable_sdl_default=false
			enable_x11_default=false
			enable_glx_default=false
			OBJCFLAGS="$OBJCFLAGS -framework QTKit"
			LIBS="$LIBS -framework QTKit -framework CoreVideo "
			dnl the following check is necessary but due to automake bug it forces every platform to have an objC compiler !
			dnl AC_LANG_PUSH([Objective C])
			dnl AC_CHECK_HEADERS([QTKit/QTKit.h],[],[AC_MSG_ERROR([QTKit framework not found, required for video support])])
			dnl AC_LANG_POP([Objective C])
		elif test "$ios_found" = "yes" ; then
			enable_sdl_default=false
			enable_x11_default=false
			enable_glx_default=false
		elif test "$ms_check_dep_mingw_found" = "yes" ; then
			enable_sdl_default=false
			enable_x11_default=false
			enable_glx_default=false
		else
			enable_sdl_default=false
			enable_x11_default=true
			enable_glx_default=true
		fi

		if test "$ffmpeg" = "true"; then
			dnl test for ffmpeg presence
			PKG_CHECK_MODULES(FFMPEG, [libavcodec >= 51.0.0 ],ffmpeg_found=yes , ffmpeg_found=no)
			if test x$ffmpeg_found = xno ; then
			   AC_MSG_ERROR([Could not find libavcodec (from ffmpeg) headers and library. This is mandatory for video support])
			fi
			
			FFMPEG_LIBS="$FFMPEG_LIBS -lavutil"
			
			PKG_CHECK_MODULES(SWSCALE, [libswscale >= 0.7.0 ],swscale_found=yes , swscale_found=no)
			if test x$swscale_found = xno ; then
			   AC_MSG_ERROR([Could not find libswscale (from ffmpeg) headers and library. This is mandatory for video support])
			fi

			dnl check for new/old ffmpeg header file layout
			CPPFLAGS_save=$CPPFLAGS
			CPPFLAGS="$FFMPEG_CFLAGS $CPPFLAGS"
			AC_CHECK_HEADERS(libavcodec/avcodec.h)
			CPPFLAGS=$CPPFLAGS_save

			dnl to workaround a bug on debian and ubuntu, check if libavcodec needs -lvorbisenc to compile	
			AC_CHECK_LIB(avcodec,avcodec_register_all, novorbis=yes , [
				LIBS="$LIBS -lvorbisenc"
				], $FFMPEG_LIBS )

			dnl when swscale feature is not provided by
			dnl libswscale, its features are swallowed by
			dnl libavcodec, but without swscale.h and without any
			dnl declaration into avcodec.h (this is to be
			dnl considered as an ffmpeg bug).
			dnl 
			dnl #if defined(HAVE_LIBAVCODEC_AVCODEC_H) && !defined(HAVE_LIBSWSCALE_SWSCALE_H)
			dnl # include "swscale.h" // private linhone swscale.h
			dnl #endif
			CPPFLAGS_save=$CPPFLAGS
			CPPFLAGS="$SWSCALE_CFLAGS $CPPFLAGS"
			AC_CHECK_HEADERS(libswscale/swscale.h)
			CPPFLAGS=$CPPFLAGS_save

		fi

		AC_ARG_ENABLE(theora,
		  [AS_HELP_STRING([--disable-theora], [Disable theora support])],
		  [case "${enableval}" in
			yes) theora=true ;;
			no)  theora=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --disable-theora) ;;
		  esac],[theora=true])

		if test x$theora = xtrue; then
		PKG_CHECK_MODULES(THEORA, [theora >= 1.0alpha7 ], [have_theora=yes],
					[have_theora=no])
		fi
		
		AC_ARG_ENABLE(vp8,
		  [AS_HELP_STRING([--disable-vp8], [Disable vp8 support])],
		  [case "${enableval}" in
			yes) vp8=true ;;
			no)  vp8=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --disable-vp8) ;;
		  esac],[vp8=true])

		vp8dir=/usr
		if test x$vp8 = xtrue; then
			PKG_CHECK_MODULES(VP8, [vpx >= 0.9.6 ], [have_vp8=yes],
					[have_vp8=no])
			if test "$have_vp8" = "no" ; then
				MS_CHECK_DEP([VP8 codec],[VP8],[${vp8dir}/include],
					[${vp8dir}/lib],[vpx/vpx_encoder.h],[vpx],[vpx_codec_encode])
				if test "$VP8_found" = "yes" ; then
					have_vp8=yes
				fi
			fi
		fi

		if test "$ffmpeg" = "false"; then
		   FFMPEG_CFLAGS=" $FFMPEG_CFLAGS -DNO_FFMPEG"
		fi

		VIDEO_CFLAGS=" $FFMPEG_CFLAGS -DVIDEO_ENABLED"
		VIDEO_LIBS=" $FFMPEG_LIBS $SWSCALE_LIBS"

		if test "$mingw_found" = "yes" ; then
			VIDEO_LIBS="$VIDEO_LIBS -lvfw32 -lgdi32"
		fi
		if test "$ios_found" = "yes" ; then
			LIBS="$LIBS -framework AVFoundation -framework CoreVideo -framework CoreMedia"
		fi
	fi
	
	AC_SUBST(VIDEO_CFLAGS)
	AC_SUBST(VIDEO_LIBS)
])
