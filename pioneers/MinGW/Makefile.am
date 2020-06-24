# Pioneers - Implementation of the excellent Settlers of Catan board game.
#   Go buy a copy.
#
# Copyright (C) 2011 Roland Clobus <rclobus@rclobus.nl>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

if IS_MINGW_PORT

install-exec-hook: install-MinGW

install-MinGW: install-MinGW-DLLs install-MinGW-icons install-MinGW-fileopen

install-MinGW-DLLs:
	$(INSTALL_PROGRAM) /mingw64/bin/libgdk-3-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgio-2.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgdk_pixbuf-2.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libcairo-2.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libglib-2.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgtk-3-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgobject-2.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libintl-8.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libpango-1.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libpangocairo-1.0-0.dll $(bindir)
	-$(INSTALL_PROGRAM) /mingw64/bin/libnotify-4.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libfontconfig-1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libfreetype-6.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libpangoft2-1.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libpangowin32-1.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libfribidi-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libthai-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libiconv-2.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgcc_s_seh-1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libexpat-1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libbz2-1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libharfbuzz-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libpng16-16.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/zlib1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libpixman-1-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libcairo-gobject-2.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libepoxy-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgmodule-2.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libwinpthread-1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libpcre-1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libbrotlidec.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libbrotlicommon.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgraphite2.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libffi-7.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libatk-1.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libdatrie-1.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libstdc++-6.dll $(bindir)
	$(MKDIR_P) $(bindir)/lib/gdk-pixbuf-2.0/2.10.0
	$(INSTALL_DATA) $(srcdir)/MinGW/loaders.cache $(bindir)/lib/gdk-pixbuf-2.0/2.10.0
	$(MKDIR_P) $(bindir)/lib/gdk-pixbuf-2.0/2.10.0/loaders
	$(INSTALL_PROGRAM) /mingw64/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.dll $(bindir)/lib/gdk-pixbuf-2.0/2.10.0/loaders
	$(INSTALL_PROGRAM) /mingw64/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-svg.dll $(bindir)/lib/gdk-pixbuf-2.0/2.10.0/loaders
	$(INSTALL_PROGRAM) /mingw64/bin/librsvg-2-2.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libgdk_pixbuf-2.0-0.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/libxml2-2.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/liblzma-5.dll $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/gspawn-win64-helper.exe $(bindir)
	$(INSTALL_PROGRAM) /mingw64/bin/gspawn-win64-helper-console.exe $(bindir)

install-MinGW-icons:
	$(MKDIR_P) $(bindir)/share/icons/hicolor
	$(INSTALL_DATA) /mingw64/share/icons/hicolor/index.theme $(bindir)/share/icons/hicolor
	$(MKDIR_P) $(bindir)/share/icons/Adwaita/scalable/actions
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/index.theme $(bindir)/share/icons/Adwaita
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/actions/list-add-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/actions
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/actions/list-remove-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/actions
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/actions/edit-undo-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/actions
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/actions/edit-delete-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/actions/edit-delete.svg
	$(MKDIR_P) $(bindir)/share/icons/Adwaita/scalable/ui
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/ui/pan-down-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/ui
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/ui/pan-up-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/ui
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/ui/window-close-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/ui

install-MinGW-fileopen:
	$(MKDIR_P) $(bindir)/share/glib-2.0/schemas
	$(INSTALL_DATA) /mingw64/share/glib-2.0/schemas/gschemas.compiled $(bindir)/share/glib-2.0/schemas
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/actions/document-open-recent-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/actions
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/actions/media-eject-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/actions
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/ui/pan-start-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/ui
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/ui/pan-end-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/ui
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/ui/pan-down-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/ui
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/ui/pan-up-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/ui
	$(MKDIR_P) $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/user-home-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/user-desktop-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/folder-documents-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/folder-download-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/folder-music-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/folder-pictures-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/folder-videos-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/user-trash-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/places/folder-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/places
	$(MKDIR_P) $(bindir)/share/icons/Adwaita/scalable/devices
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/devices/drive-harddisk-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/devices
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/devices/drive-harddisk-usb-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/devices
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/devices/drive-optical-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/devices
	$(MKDIR_P) $(bindir)/share/icons/Adwaita/scalable/mimetypes
	$(INSTALL_DATA) /mingw64/share/icons/Adwaita/scalable/mimetypes/text-x-generic-symbolic.svg $(bindir)/share/icons/Adwaita/scalable/mimetypes

package: install-strip
	$(INSTALL_DATA) NEWS $(bindir)/NEWS.txt
	$(INSTALL_DATA) AUTHORS $(bindir)/AUTHORS.txt
	$(INSTALL_DATA) COPYING $(bindir)/COPYING.txt
	$(INSTALL_DATA) README $(bindir)/README.txt
	cd $(bindir); zip -r $(abs_top_srcdir)/pioneers-$(VERSION)-portable.zip *

endif # IS_MINGW_PORT

EXTRA_DIST += \
	$(srcdir)/MinGW/loaders.cache \
	$(srcdir)/MinGW/README.txt
