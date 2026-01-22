# Maintainer: Frederik Schwan <freswa@archlinux.org>
# Contributor: Jelle van der Waa <jelle@archlinux.org>
# Contributor: Daniel Micay <danielmicay@gmail.com>
# Contributor: Julien Virey <julien.virey@gmail.com>
# Contributor: Adrian Perez de Castro <aperez@igalia.com>

pkgdesc='A simple VTE-based terminal'
pkgname=termite
pkgver=16.11
pkgrel=1
url=https://github.com/barbuk/termite
license=(LGPL-2.1-only)
depends=(gtk3 pcre2 gnutls vte-common)
makedepends=(git gperf 'meson>=1.0' ninja glib2-devel)
arch=(x86_64)
backup=(etc/xdg/termite/config)
validpgpkeys=(9F59A49568EE372AF17E5452B6D01F84A7519939)
# git rev-parse "v$pkgver"
_tag=a8eccf7a508413bdbe4bf210ccc4b11e9339fb50
source=("git+$url.git?signed#tag=$_tag")
sha256sums=('0b20db55085f9d734914fb2b3b6c7120124c28620ca6ed40ffb7083c49405e93')

build () {
	rm -rf _build
	arch-meson --wrap-mode=forcefallback _build "${pkgname}"
	meson compile -C _build
}

package () {
	meson install -C _build --skip-subprojects vte --destdir "${pkgdir}"
}
