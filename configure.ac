#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# To rebuild the configure and Makefile.in files (as well as some
# supporting files mainly in build-aux/ am/ and m4/) run the following
# command:
#     autoreconf -vif

AC_PREREQ([2.68])
AC_INIT([riakccs], [0.1], [https://github.com/lyda/riakccs/])
AC_CONFIG_SRCDIR([src/riakccs/api.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([build-aux/config.h])
# The -Wno-portability is here to tell automake not to complain about
# $@ and $< which are used to define protobuf-c build rules.
AM_INIT_AUTOMAKE([1.11 subdir-objects -Wno-portability color-tests])
AM_SILENT_RULES([yes])
LT_INIT
AC_SUBST([LIBTOOL_DEPS])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_CHECK_PROG(PROTOC_C, protoc-c, protoc-c)
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG
if test -n "$PKG_CONFIG"; then
  # Kludge for systems where the pkg-config install dir is simply wrong!
  if $PKG_CONFIG --variable=pc_path pkg-config 2>/dev/null \
      | grep -q /libdata/; then
    PKG_INSTALLDIR(['${prefix}/libdata/pkgconfig'])
  else
    PKG_INSTALLDIR
  fi
else
  # This should never be reached.
  AC_MSG_ERROR([pkg-config is required!])
fi

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])
AC_DEFUN([AMINCLUDE_STATIC],[am/aminclude_static.am])
AX_PROG_SPLINT([-weak +posixlib -formatcode -I src -I proto -I /usr/local/include -I /usr/include/$($CC -dumpmachine)])
AC_TDD_GCOV
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_MAN_FEATURE(ON)
DX_DOT_FEATURE(ON)
DX_INIT_DOXYGEN([riakccs], [docs/Doxyfile], [docs/doxygen])
AS_IF([test -z "x$DX_DOXYGEN"], [have_doxygen=no], [have_doxygen=yes])

# Checks for libraries.  Each defines a with_LIB var that can be
# one of three values: yes, discover or no. By default the value is
# discover which means the library will be used if discovered.  If
# the user passes --with-LIB to configure, the configure script will
# fail if the library cannot be found.
AC_CHECK_LIB([protobuf-c], [protobuf_c_message_get_packed_size],
    [], [AC_MSG_FAILURE([protobuf-c required - is -dev pkg installed?])])
AC_CHECK_LIB([yaml], [yaml_parser_initialize],
    [], [AC_MSG_FAILURE([yaml required - is libyaml-dev pkg installed?])])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h stddef.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_MMAP
AM_CONDITIONAL([BUILD_TWEAK], [test x$ac_cv_func_mmap_fixed_mapped = xyes])
AC_CHECK_FUNCS([gettimeofday memmove memset select strchr strtol])

# Generate files.
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_MSG_RESULT([
    $PACKAGE $VERSION

        CC           : ${CC}
        CFLAGS       : ${CFLAGS}
        LDFLAGS      : ${LDFLAGS}
        LIBS         : ${LIBS}
        PROTOC_C     : ${PROTOC_C}
        doxygen-doc  : ${have_doxygen}

        prefix       : ${prefix}
        sysconfdir   : ${sysconfdir}
        libdir       : ${libdir}
        includedir   : ${includedir}
        pkgconfigdir : ${pkgconfigdir}
])
