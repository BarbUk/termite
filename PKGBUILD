# Maintainer: Frederik Schwan <freswa@archlinux.org>
# Contributor: Jelle van der Waa <jelle@archlinux.org>
# Contributor: Daniel Micay <danielmicay@gmail.com>
# Contributor: Julien Virey <julien.virey@gmail.com>
# Contributor: Adrian Perez de Castro <aperez@igalia.com>

pkgdesc='A simple VTE-based terminal'
pkgname=termite
pkgver=16.12
pkgrel=1
url=https://github.com/barbuk/termite
license=(LGPL-2.1-only)
depends=(gtk3 pcre2 gnutls vte-common)
makedepends=(git gperf 'meson>=1.0' ninja glib2-devel)
arch=(x86_64)
backup=(etc/xdg/termite/config)
validpgpkeys=(9F59A49568EE372AF17E5452B6D01F84A7519939)
# git rev-parse "v$pkgver"
_tag=1ef7da0c6d27b4a4c2a39da0cec3c3adebbef963
source=("git+$url.git?signed#tag=$_tag")
sha256sums=('4e239a8ebe1be409a09a9327f6682ad90a1a8f0492324644aa444f86ca8ca28b')

build () {
	rm -rf _build
	arch-meson --wrap-mode=forcefallback _build "${pkgname}"
	meson compile -C _build
}

package () {
	meson install -C _build --skip-subprojects vte --destdir "${pkgdir}"
}
