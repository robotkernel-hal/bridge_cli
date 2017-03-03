
libdir = $(prefix)/lib
lib_LTLIBRARIES = libservice_provider_cli.la

libservice_provider_cli_la_SOURCES  = cli_bridge.cpp
libservice_provider_cli_la_CXXFLAGS = -Wall -I$(top_srcdir)/src -I$(top_builddir) @ROBOTKERNEL_CFLAGS@ \
				 @LIBSTRING_UTIL_CFLAGS@ @YAML_CPP_CFLAGS@

libservice_provider_cli_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

