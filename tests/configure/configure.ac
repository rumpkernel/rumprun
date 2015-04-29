AC_PREREQ([2.66])
AC_INIT([apptools-test], [11], [rumpkernel-users@lists.sourceforge.net])

AC_CONFIG_SRCDIR([test.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11 foreign subdir-objects -Wall -Werror])
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_CHECK_HEADERS([sys/epoll.h])
AC_CHECK_FUNCS([select sigaction])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
