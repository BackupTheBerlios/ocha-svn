#! /bin/sh
#
# See: http://packages.debian.org/testing/gnome/gnome-common
#
# Debian users:
#  See README.Debian in package gnome-common 
#  Install packages libgnome-dev and gnome-common 
#
set -x
export USE_GNOME2_MACROS=1
export REQUIRED_AUTOMAKE_VERSION=1.6
PKG_NAME="ocha"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
(test -f $srcdir/configure.in \
  && test -f $srcdir/ChangeLog \
  && test -d $srcdir/src) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}
cd $srcdir

which  gnome-config >/dev/null 2>&1 || {
    echo "You need to install gnome-config into "
    echo "your PATH. (You might find it in a package "
    echo "called something like libgnome-dev)"
    exit 2
}

test -x $(gnome-config --bindir)/gnome-autogen.sh || {
    echo "You need to install package gnome-common from "
    echo "your distribution or from the GNOME CVS"
    exit 3
}

exec $(gnome-config --bindir)/gnome-autogen.sh \
  --enable-maintainer-mode \
  --enable-compile-warnings=error \
  --enable-debug \
  --enable-iso-c
