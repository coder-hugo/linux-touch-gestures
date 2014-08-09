#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([touch-gestures], [0.1], [coder-hugo@users.noreply.github.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/main.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADER([linux/input.h],
                [AC_DEFINE([HAVE_LINUX_INPUT], [1],
                  [Define to 1 if you have <linux/input.h>.])],
                [AC_MSG_ERROR([The linux header files are required])])
AC_CHECK_HEADER([linux/uinput.h],
                [AC_DEFINE([HAVE_LINUX_UINPUT], [1],
                  [Define to 1 if you have <linux/uinput.h>.])],
                [AC_MSG_ERROR([The linux header files are required])])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_OUTPUT(Makefile src/Makefile doc/Makefile man/Makefile)
