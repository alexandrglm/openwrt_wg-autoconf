# Maintainer: Alexander Gomez <alexandrglm@proton.me>
pkgname=wg-autoconf
pkgver=1.0.0
pkgrel=1
pkgdesc="WireGuard Auto-Configuration tool for OpenWrt"
url="https://github.com/alexandrglm/openwrt_wg-autoconf"
arch="noarch"
license="MIT"
depends="wireguard-tools"
makedepends=""
options="!check !strip !scanelf !tracedeps"
source="source.tar.gz"
install="wg-autoconf.post-install wg-autoconf.pre-install wg-autoconf.pre-deinstall wg-autoconf.pre-upgrade wg-autoconf.post-upgrade"

prepare() {
    mkdir -p "$builddir"
    cp -r "$srcdir/src/"* "$builddir/"
}

build() {
	return 0
}

package() {
	install -Dm755 "$builddir/usr/bin/wg-autoconf.clean" "$pkgdir/usr/bin/wg-autoconf"
	install -Dm755 "$builddir/etc/init.d/wg-autoconf_boot_cleanup.clean" "$pkgdir/etc/init.d/wg-autoconf_boot_cleanup"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/lifecyle/wg-autoconf_prerm.clean" "$pkgdir/usr/libexec/wg-autoconf/scripts/wg-autoconf_prerm.sh"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/lifecyle/wg-autoconf_preinst.clean" "$pkgdir/usr/libexec/wg-autoconf/scripts/wg-autoconf_preinst.sh"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/lifecyle/wg-autoconf_postinst.clean" "$pkgdir/usr/libexec/wg-autoconf/scripts/wg-autoconf_postinst.sh"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/lifecyle/wg-autoconf_postupgrade.clean" "$pkgdir/usr/libexec/wg-autoconf/scripts/wg-autoconf_postupgrade.sh"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/states.clean" "$pkgdir/usr/libexec/wg-autoconf/states"
}
sha512sums="
fcac4800f2b9a281f06eb42e38ff377c77bc372d36a7275f055101d655b163f633ca23edd3b1ed40d85e45ca9fc47cee22fd557bccd0dcdda06f70025752a213  wg-autoconf_v1.0.0-r1_source.tar.gz
"
