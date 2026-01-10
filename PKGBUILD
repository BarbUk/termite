# Maintainer: Frederik Schwan <freswa@archlinux.org>
# Contributor: Jelle van der Waa <jelle@archlinux.org>
# Contributor: Daniel Micay <danielmicay@gmail.com>
# Contributor: Julien Virey <julien.virey@gmail.com>
# Contributor: Adrian Perez de Castro <aperez@igalia.com>

pkgdesc='A simple VTE-based terminal'
pkgname=termite
pkgver=16.10
pkgrel=1
url=https://github.com/barbuk/termite
license=(LGPL-2.1-only)
depends=(gtk3 pcre2 gnutls vte-common)
makedepends=(git gperf 'meson>=1.0' ninja glib2-devel)
arch=(x86_64)
backup=(etc/xdg/termite/config)
validpgpkeys=(9F59A49568EE372AF17E5452B6D01F84A7519939)
_tag=d72b48759669c53efe3bda424f9319dc9857830c
source=("git+$url.git?signed#tag=$_tag")
sha256sums=('b893c140de3fdbd87ee66c27cdcf1989e5f60de5694a7c201364fe6af40fa241')

build () {
	rm -rf _build
	arch-meson --wrap-mode=forcefallback _build "${pkgname}"
	meson compile -C _build
}

package () {
	meson install -C _build --skip-subprojects vte --destdir "${pkgdir}"
}
