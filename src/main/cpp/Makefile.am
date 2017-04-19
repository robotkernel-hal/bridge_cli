
libdir = $(prefix)/lib
lib_LTLIBRARIES = libbridge_cli.la

libbridge_cli_la_SOURCES  = cli_bridge.cpp cli_server.cpp
libbridge_cli_la_CXXFLAGS = -Wall \
									  -I$(top_srcdir)/src \
									  -I$(top_builddir) \
									  @ROBOTKERNEL_CFLAGS@ \
									  @LIBSTRING_UTIL_CFLAGS@ \
									  @YAML_CPP_CFLAGS@

libbridge_cli_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

