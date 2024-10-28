#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for gpopt
#
#-------------------------------------------------------------------------

subdir = gpcontrib/gp_orca/gpopt/utils
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/gpcontrib/gp_orca/gpopt/gpopt.mk

OBJS = COptTasks.o CConstExprEvaluatorProxy.o CMemoryPoolPalloc.o CMemoryPoolPallocManager.o funcs.o RelationWrapper.o

include $(top_srcdir)/src/backend/common.mk
