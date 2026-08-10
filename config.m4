PHP_ARG_ENABLE([phpweb], [whether to build PHPWEB SAPI],
  [AS_HELP_STRING([--enable-phpweb], [Build the PHPWEB SAPI])], [no],[no])

if test "$PHP_PHPWEB" != "no"; then

   PKG_CHECK_MODULES([GTK4], [gtk4 >= 4.0.0],
        [PHP_EVAL_INCLINE([$GTK4_CFLAGS])],
        [AC_MSG_FAILURE([gtk4 not found])])
   PHP_EVAL_LIBLINE([$GTK4_LIBS], [PHPWEB_EXTRA_LIBS], [yes])

   dnl Sanity check.
   CFLAGS_save=$CFLAGS
   CFLAGS="$INCLUDES $CFLAGS"
    AC_CHECK_HEADER([gtk/gtk.h],
      [AC_DEFINE([HAVE_GTK4], [1],
        [Define to 1 if Gtk4 confinement is available for phpweb.])],
      [AC_MSG_FAILURE([Required <gtk/gtk.h> header file not found.])])
    CFLAGS=$CFLAGS_save

   PKG_CHECK_MODULES([WEBKITGTK6], [webkitgtk-6.0 >= 2.40.0],
        [PHP_EVAL_INCLINE([$WEBKITGTK6_CFLAGS])],
        [AC_MSG_FAILURE([webkitgtk-6.0 not found])])
   PHP_EVAL_LIBLINE([$WEBKITGTK6_LIBS], [PHPWEB_EXTRA_LIBS], [yes])

   dnl Sanity check.
   CFLAGS_save=$CFLAGS
   CFLAGS="$INCLUDES $CFLAGS"
   AC_CHECK_HEADER([webkit/webkit.h],
      [AC_DEFINE([HAVE_WEBKITGTK6], [1],
        [Define to 1 if WEBKITGTK6 confinement is available for phpweb.])],
      [AC_MSG_FAILURE([Required <webkit/webkit.h> header file not found.])])
    CFLAGS=$CFLAGS_save

  PHP_ADD_MAKEFILE_FRAGMENT([$abs_srcdir/sapi/phpweb/Makefile.frag])
  SAPI_PHPWEB_PATH=sapi/phpweb/phpweb
  PHP_SELECT_SAPI([phpweb], [program], [phpweb.c],
  [
  -I$abs_srcdir/sapi/phpweb
  -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1
  ]
  )

  BUILD_PHPWEB="\$(LIBTOOL) --tag=CC --mode=link \
        \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(PHP_RPATHS) \
                \$(PHP_GLOBAL_OBJS:.lo=.o) \
                \$(PHP_BINARY_OBJS:.lo=.o) \
                \$(PHP_PHPWEB_OBJS:.lo=.o) \
                \$(EXTRA_LIBS) \
                \$(PHPWEB_EXTRA_LIBS) \
                \$(ZEND_EXTRA_LIBS) \
                \$(PHP_FRAMEWORKS) \
         -o \$(SAPI_PHPWEB_PATH)"

  PHP_SUBST([SAPI_PHPWEB_PATH])
  PHP_SUBST([BUILD_PHPWEB])
  PHP_SUBST([PHPWEB_EXTRA_LIBS])

fi
