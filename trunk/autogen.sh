#! /bin/sh
#
# See README.Debian in package gnome-common 
# http://packages.debian.org/testing/gnome/gnome-common
#
set -x
export USE_GNOME2_MACROS=1
export REQUIRED_AUTOMAKE_VERSION=1.6

$(gnome-config --bindir)/gnome-autogen.sh \
  --enable-maintainer-mode \
  --enable-compile-warnings=error \
  --enable-debug \
  --enable-iso-c
