EXTENSION = gp_orca
MODULE_big = gp_orca

OBJS = gporca.o

REGRESS = ""
EXTRA_REGRESS_OPTS = --init-file=$(top_builddir)/src/test/regress/init_file

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/gp_orca
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
