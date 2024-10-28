#
# Makefile for optimizer
#
# src/backend/gporca/libgpos/src/common/Makefile
#

subdir = gpcontrib/gp_orca/gporca/libgpos/src/common
top_builddir = ../../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/gpcontrib/gp_orca/gporca/gporca.mk

OBJS        = CAutoTimer.o \
              CBitSet.o \
              CBitSetIter.o \
              CBitVector.o \
              CDebugCounter.o \
              CHeapObject.o \
              CMainArgs.o \
              CRandom.o \
              CStackDescriptor.o \
              CStackObject.o \
              CTimerUser.o \
              CWallClock.o \
              clibwrapper.o \
              syslibwrapper.o

include $(top_srcdir)/src/backend/common.mk

