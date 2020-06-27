Short guide to build Pioneers from source in MSYS2

Prepare the build environment for building from a released tarball
==================================================================
* Install GTK as instructed on https://www.gtk.org/docs/installations/windows/
  A tailored copy is copied here
* Download the 64-bit version of MSYS2 from http://www.msys2.org/
* After the installation, a MSYS shell is started
* Install the basic installation of MSYS2
* Update and install the packages. Here 2 possibilities are listed:
  A) Typical installation
  B) Installation using virtual machines without a network connection
** A) Use the MSYS2 shell and perform all updates that are mentioned on the webpage above, i.e.
  - pacman -Syu
    Close the shell with the X
    Restart the MSYS shell
  - pacman -Su
** B)
  * Use the MSYS2 shell and prepare a copy of the following folders from a regular installation
    - /var/cache/pacman/pkg
    - /var/lib/pacman/sync
  * Use the regular steps from A, but instead of '-Syu' use '-Su'
  * Placing files in a qemu-qcow2 image:
    - modprobe nbd
    - qemu-nbd -c /dev/nbd0 image.qcow2
    - mount /dev/nbd0p1 mountpoint
    - ...
    - umount mountpoint
    - qemu-nbd -d /dev/nbd0
* Install GTK+3:
  - pacman -S mingw-w64-x86_64-gtk3 
* Install the development environment (use the default: all):
  - pacman -S mingw-w64-x86_64-toolchain base-devel
* Install the zip package:
  - pacman -S zip

Prepare the build environment for building from unreleased code
===============================================================
Note: releases for Windows should always be based on tarballs
Note: you need to be in the 'MSYS2 MinGW 64-bit' shell

In addition to the default build environment, the following needs to be installed:
 - pacman -S openssh
 - pacman -S mingw-w64-x86_64-libnotify
 - pacman -S subversion
 - svn checkout https://svn.code.sf.net/p/pio/code/trunk/pioneers trunk

* Download the GOB2 tarball from http://www.jirka.org/gob.html
  - ./configure
  - make
  - make install
  
* Updating the pixbuf loaders, in a MSYS64 shell:
  - gdk-pixbuf-query-loaders > MinGW/loaders.cache
  - vim MinGW/loaders.cache
  - Change the line-ending to Linux: :set ff=unix
  - Keep only png and svg (and the trailing empty line)

* The following fonts are used
** FreeSans
** Gentium

Note: The dependency walker helps during porting
      https://dependencywalker.com/
Note: Sysinternals Process Monitor helps during porting
      https://docs.microsoft.com/en-us/sysinternals/downloads/procmon

If any of the downloads cannot be found, a newer version will probably work.

Build and install Pioneers
==========================
1) Download the source tarball to your home directory
   (c:\msys64\home\%username%)
2) Start the 'MSYS2 MinGW 64-bit' shell
3) Expand the source tarball
   (tar xvzf pioneers-%versionnumber%.tar.gz)
4) Enter the source directory (cd pioneers-%versionnumber%)
5) Configure, build and install
     ./configure --prefix=/usr/local123
     make
     make package

Start Pioneers
==============
a) Start Pioneers by double clicking on the executable
    (found in C:\msys64\usr\local123)
b) or start pioneers.exe from /usr/local123 in the shell

Rebuild the icons
=================
a) Load the svg file at 768x768 in Gimp 2.8 (768=least common multiple(48,256))
b) Execute the Script-Fu script "Iconify2.scm"
c) Export as *.ico

Known limitations
=================
* The online help is not built
* The metaserver is not built. It is recommended to use the existing metaservers
* The notification system does not work
* The beep does not work (libcanberra not ported to Windows)

Roland Clobus
2020-06-27 Pioneers-15.6
