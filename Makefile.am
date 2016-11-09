SUBDIRS = po src

ACLOCAL_AMFLAGS = -I m4

dist_pkgdata_DATA = "updf 64x64.png" "updf 128x128.png" "updf 256x256.png"

EXTRA_DIST = config/config.rpath updf.desktop README.asciidoc autogen.sh

install-data-hook: iconsdir = $(DESTDIR)$(datadir)/icons/hicolor
install-data-hook:
	install -D -m644 "$(top_srcdir)/updf 64x64.png" $(iconsdir)/64x64/apps/updf.png
	install -D -m644 "$(top_srcdir)/updf 128x128.png" $(iconsdir)/128x128/apps/updf.png
	install -D -m644 "$(top_srcdir)/updf 256x256.png" $(iconsdir)/256x256/apps/updf.png
	install -D -m644 $(top_srcdir)/updf.desktop $(DESTDIR)$(datadir)/applications/updf.desktop
