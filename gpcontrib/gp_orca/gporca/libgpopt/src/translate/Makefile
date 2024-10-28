#
# Makefile for optimizer
#
# src/backend/gporca/libgpopt/src/translate/Makefile
#

subdir = gpcontrib/gp_orca/gporca/libgpopt/src/translate
top_builddir = ../../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/gpcontrib/gp_orca/gporca/gporca.mk

OBJS        = CTranslatorDXLToExpr.o \
              CTranslatorDXLToExprUtils.o \
              CTranslatorExprToDXL.o \
              CTranslatorExprToDXLUtils.o

include $(top_srcdir)/src/backend/common.mk

