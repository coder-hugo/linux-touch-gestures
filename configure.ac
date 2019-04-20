#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([linux-touch-gestures], [1.0.2], [coder-hugo@users.noreply.github.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/main.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([m], [sqrt], [], [AC_MSG_ERROR([libm is required])])
AC_CHECK_LIB([iniparser], [iniparser_load], [], [AC_MSG_ERROR([libiniparser from http://ndevilla.free.fr/iniparser/ is required])])
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR([pthread not found])])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stddef.h stdint.h stdlib.h string.h unistd.h pthread.h])
AC_CHECK_HEADERS([linux/input.h linux/uinput.h], [], [AC_MSG_ERROR([The linux header files are required])])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset], [], [AC_MSG_ERROR([Function memset not found])])
AC_CHECK_FUNCS([sqrt], [], [AC_MSG_ERROR([Function sqrt not found])])
AC_CHECK_FUNCS([fabs], [], [AC_MSG_ERROR([Function fabs not found])])

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 man/Makefile
                 src/Makefile])
AC_OUTPUT
