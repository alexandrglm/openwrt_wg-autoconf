# Maintainer: Alexander Gomez <alexandrglm@proton.me>
pkgname=wg-autoconf
pkgver=1.0.0
pkgrel=5
pkgdesc="WireGuard Auto-Configuration tool for OpenWrt"
url="https://github.com/alexandrglm/openwrt_wg-autoconf"
arch="noarch"
license="MIT"
depends="wireguard-tools"
makedepends=""
options="!check !strip !scanelf"
source="source.tar.gz"

prepare() {
	mkdir -p "$builddir"
	cp -r "$startdir/source"/* "$builddir/" 2>/dev/null || true
}

build() {
	return 0
}

package() {
	install -Dm755 "$builddir/usr/bin/wg-autoconf.clean" "$pkgdir/usr/bin/wg-autoconf"
	install -Dm755 "$builddir/etc/init.d/wg-autoconf_boot_cleanup.source" "$pkgdir/etc/init.d/wg-autoconfig_boot_cleanup"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/lifecyle/wg-autoconf_prerm.source" "$pkgdir/usr/libexec/wg-autoconf/scripts/wg-autoconfig_prerm.sh"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/lifecyle/wg-autoconf_preinst.source" "$pkgdir/usr/libexec/wg-autoconf/scripts/wg-autoconfig_preinst.sh"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/lifecyle/wg_autoconf_postinst.source" "$pkgdir/usr/libexec/wg-autoconf/scripts/wg-autoconfig_postinst.sh"
}

pre_install() {
	/usr/libexec/wg-autoconf/scripts/wg-autoconf_preinst.sh
}

post_install() {
	/usr/libexec/wg-autoconf/scripts/wg-autoconf_postinst.sh
}
pre_deinstall() {
	/usr/libexec/wg-autoconf/scripts/wg-autoconf_postinst.sh
}
