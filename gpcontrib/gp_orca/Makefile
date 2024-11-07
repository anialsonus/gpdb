MODULE_big = gp_orca

OBJS = gporca.o \

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

install: install-gp-orca

install-gp-orca: $(DESTDIR)$(datadir)/postgresql.conf.sample
	sed -i "s/#shared_preload_libraries = ''/shared_preload_libraries = 'gp_orca'/g" $(DESTDIR)$(datadir)/postgresql.conf.sample
