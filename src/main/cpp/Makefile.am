lib_LTLIBRARIES = libbridge_cli.la

libbridge_cli_la_SOURCES  = cli_bridge.cpp cli_server.cpp
libbridge_cli_la_CXXFLAGS = -Wall \
									  -I$(top_srcdir)/src \
									  -I$(top_builddir) \
									  @ROBOTKERNEL_CFLAGS@

libbridge_cli_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(top_srcdir)/share/ln/* $(prefix)/share/ln/
	-cp -rf $(top_builddir)/share/ln/* $(prefix)/share/ln/
