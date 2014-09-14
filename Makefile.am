ACLOCAL_AMFLAGS = -I m4 --install
AM_CFLAGS = -I $(srcdir)/src -I $(srcdir)/proto
RIAK_PROTOBUF_C_SRCS = proto/riak.pb-c.c \
		       proto/riak_kv.pb-c.c \
		       proto/riak_dt.pb-c.c \
		       proto/riak_search.pb-c.c \
		       proto/riak_yokozuna.pb-c.c
PROTOBUF_C_HDRS = $(patsubst %.c,%.h,$(RIAK_PROTOBUF_C_SRCS))
EXTRA_DIST = $(patsubst %.pb-c.c,%.proto,$(RIAK_PROTOBUF_C_SRCS))

lib_LTLIBRARIES = lib/libriakccs.la
bin_PROGRAMS = bin/rk
TESTS = tests/riak_net src/tests/rk_tests.sh
check_PROGRAMS = tests/riak_net
check_SCRIPTS = src/tests/valgrind.riak_net.sh src/tests/rk_tests.sh
BUILT_SOURCES = $(RIAK_PROTOBUF_C_SRCS) \
		$(PROTOBUF_C_HDRS)
EXTRA_DIST += $(check_SCRIPTS) src/tests/data/test_search.json

dist_man3_MANS = man/riakccs.3

valgrind: $(check_PROGRAMS)
	@src/tests/valgrind.riak_net.sh

# Libraries.
nodist_lib_libriakccs_la_SOURCES = $(RIAK_PROTOBUF_C_SRCS)
lib_libriakccs_la_SOURCES = src/riakccs/debug.h \
			    src/riakccs/debug.c \
			    src/riakccs/api.c \
			    src/riakccs/comms.c \
			    src/riakccs/pb.h \
			    src/riakccs/pb.c
lib_libriakccs_la_CFLAGS = $(AM_CFLAGS) $(COVERAGE_CFLAGS)
# Version updating rules:
# 1. If the library source code has changed but interfaces are the same,
#    then increment revision (c:r:a becomes c:r+1:a).
# 2. If any interfaces have been removed or changed since the last release,
#    set revision and age to 0 and increment current (c:r:a becomes c+1:0:0).
# 3. If interfaces have only been added since the last release, set
#    release to 0 and increment current and age (c:r:a becomes c+1:0:a+1).
lib_libriakccs_la_LDFLAGS = $(COVERAGE_LDFLAGS) -version-info 1:0:0

# Headers for libraries.
libriakccsdir = $(includedir)/riakccs
libriakccs_HEADERS = $(PROTOBUF_C_HDRS) src/riakccs/api.h

# Binaries.
bin_rk_SOURCES = src/kv/rk.c src/kv/rk_parse.h src/kv/rk_parse.c
bin_rk_CFLAGS = $(AM_CFLAGS) $(COVERAGE_CFLAGS)
bin_rk_LDADD = lib/libriakccs.la $(COVERAGE_LDFLAGS)
bin_rk_LDFLAGS = -static

# Static analysis.
analyze_srcs = $(lib_libriakccs_la_SOURCES) \
	       $(bin_rk_SOURCES)
.PHONY: analyze
analyze: all
	@for f in $(analyze_srcs); do \
	  if test "@AM_V@" = 1; then \
	    echo clang --analyze -x c -I proto -I src $$f -o /dev/null; \
	  else \
	    echo "  CCSA  " $$f; \
	  fi; \
	  clang --analyze -x c -I proto -I src $$f -o /dev/null; \
	done

# Test programs.
tests_riak_net_SOURCES = src/tests/riak_net.c
tests_riak_net_CFLAGS = $(AM_CFLAGS) $(COVERAGE_CFLAGS) @CHECK_CFLAGS@
tests_riak_net_LDADD = lib/libriakccs.la $(COVERAGE_LDFLAGS) @CHECK_LIBS@
tests_riak_net_LDFLAGS = -static

# Define macros to have quiet protobufc-c statements.
AM_V_PROTOC_C = $(AM_V_PROTOC_C_@AM_V@)
AM_V_PROTOC_C_ = $(AM_V_PROTOC_C_@AM_DEFAULT_V@)
AM_V_PROTOC_C_0 = @echo "  PBC_C " $<;
%.pb-c.c %.pb-c.h: %.proto
	$(AM_V_PROTOC_C)$(PROTOC_C) --proto_path=$(dir $<) --c_out=$(dir $@) $<

# Splint "splint" target.
include am/aminclude_static.am
include am/aminclude_coverage.am
include am/aminclude_doxygen.am

.PHONY: changelog
changelog:
	@git shortlog --pretty='* [%h] %s' -e `git log -n 1 --pretty='%H' -- ChangeLog`..HEAD > ChangeLog.new

# Add things to remove for the "distclean" target.
distclean-local: distclean-local-ac
.PHONY: distclean-local-ac
distclean-local-ac:
	@rm -rf autom4te.cache build-aux/*~ m4/*~

# Add things to remove for the "clean" target.
clean-local: clean-local-build-dirs clean-coverage-html
.PHONY: clean-local-build-dirs
clean-local-build-dirs:
	@rm -rf bin lib tests
MOSTLYCLEANFILES = $(DX_CLEANFILES) \
		   $(BUILT_SOURCES)
