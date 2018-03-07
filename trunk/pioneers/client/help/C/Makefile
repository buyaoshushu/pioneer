# Pioneers - Implementation of the excellent Settlers of Catan board game.
#   Go buy a copy.
#
# Copyright (C) 2013 Roland Clobus <rclobus@rclobus.nl>
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

# Various snippets for generating other representations of the help

# HTMLHelp is the compressed help format for Microsoft Windows
htmlhelp: index.docbook
	xmlto -m custom.xsl htmlhelp index.docbook
	-/cygdrive/c/Program\ Files/HTML\ Help\ Workshop/hhc.exe htmlhelp.hhp
	rm htmlhelp.hhp
	rm toc.hhc
	rm *.html

# An export to HTML with a simple stylesheet
simplehtml: index.docbook
	xmlto -m custom.xsl html index.docbook

# An export to HTML with the looks of yelp
yelphtml: index.docbook
	xsltproc /usr/share/yelp-xsl/xslt/docbook/html/db2html.xsl index.docbook
	cp /usr/share/yelp-xsl/icons/hicolor/24x24/status/yelp-note*.png .

# Reformatting the Docbook file
xmllint: index.docbook
	xmllint --format index.docbook -o index.docbook.good
	mv index.docbook.good index.docbook
