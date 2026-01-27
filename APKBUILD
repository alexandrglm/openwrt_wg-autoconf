# Maintainer: Alexander Gomez <148530039+alexandrglm@users.noreply.github.com>
pkgname=wg-autoconf
pkgver=1.0.1
pkgrel=2
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
	install -Dm755 "$builddir/usr/bin/wg-autoconf-test" "$pkgdir/usr/bin/wg-autoconf"
	install -Dm755 "$builddir/etc/init.d/wg_boot_cleanup" "$pkgdir/etc/init.d/wg-autoconfig_boot_cleanup"
}

post_install() {
	echo "[wg-autoconf] Enabling  /etc/init.d/wg-autoconfig_boot_cleanup ..."
	chmod +x /etc/init.d/wg-autoconfig_boot_cleanup
	/etc/init.d/wg-autoconfig_boot_cleanup enable
	sleep 1
	echo ""
	echo "[wg-autoconf] Check README, check network, firewall, dhcp, init.d cleanups, ..."
	echo "[wg-autoconf] D0ne. 3njoy! "
	echo ""
}
