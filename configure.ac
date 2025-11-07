AC_INIT([luma], [git], [https://github.com/TheDevConnor/Luma])
AC_PREREQ([2.69])
AC_CONFIG_SRCDIR([src/main.c]) # sanity check to make sure someone didn't pass a garbage --srcdir
#AM_INIT_AUTOMAKE([foreign 1.16 dist-bzip2])

AC_PROG_CXX
AC_LANG(C++)
AC_SUBST(CXX)
# connor, i have no idea why you're expecting CC to be a C++ compiler.
# that's what CXX is for.

CXXFLAGS="$CXXFLAGS $CFLAGS $CPPFLAGS" # grab any env for flags
LDFLAGS="$LDFLAGS"
AC_SUBST(CXXFLAGS)
AC_SUBST(LDFLAGS)

AC_PREFIX_DEFAULT(/usr/local)
AC_SUBST(prefix)

AC_CONFIG_FILES([config.mk])
AC_OUTPUT
