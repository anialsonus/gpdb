override CPPFLAGS := -I$(top_srcdir)/gpcontrib/gp_orca/gporca/libgpos/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/gpcontrib/gp_orca/gporca/libgpopt/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/gpcontrib/gp_orca/gporca/libnaucrates/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/gpcontrib/gp_orca/gporca/libgpdbcost/include $(CPPFLAGS)

# orca is not accessed in JIT (executor stage), avoid the generation of .bc here
# NOTE: accordingly we MUST avoid them in install step (install-postgres-bitcode
# in src/backend/Makefile)
with_llvm = no
