#
# Makefile for optimizer
#
# src/backend/gporca/libgpopt/src/Makefile
#

subdir = gpcontrib/gp_orca/gporca/libgpopt/src
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/gpcontrib/gp_orca/gporca/gporca.mk

SUBDIRS     = base engine eval hints mdcache metadata minidump operators optimizer search translate xforms
OBJS        = exception.o init.o

include $(top_srcdir)/src/backend/common.mk

