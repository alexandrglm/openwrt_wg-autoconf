# Maintainer: Alexander Gomez <148530039+alexandrglm@users.noreply.github.com>
pkgname=wg-autoconf
pkgver=1.0.0
pkgrel=4
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
	install -Dm755 "$builddir/usr/bin/wg-autoconf.source" "$pkgdir/usr/bin/wg-autoconf"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/debug.source" "$pkgdir/usr/libexec/wg-autoconf/debug"
	install -Dm755 "$builddir/etc/init.d/wg-autoconfig_boot_cleanup.source" "$pkgdir/etc/init.d/wg-autoconfig_boot_cleanup"
}

post_install() {

	# echo "[wg-autoconf] Enabling  /etc/init.d/wg-autoconfig_boot_cleanup and LAN4 ..." > /dev/kmesg
	chmod +x /etc/init.d/wg-autoconfig_boot_cleanup
	sleep 1
	/etc/init.d/wg-autoconfig_boot_cleanup enable
	/etc/init.d/wg-autoconfig_boot_cleanup start
	sleep 1
	# echo "[wg-autoconf] Check README, check network, firewall, dhcp, init.d cleanups, ..." > /dev/kmesg
	# echo "[wg-autoconf] D0ne. 3njoy! " > /dev/kmesg

}

pre_upgrade() {


	if ip link show 2>/dev/null | grep -qE '^ *[0-9]+: wg[a-zA-Z0-9_-]+'; then

		echo "[UPGRADE wg-autoconf] ACTIVE WG SESSION!!!!! Aborting upgrade" > /dev/kmsg
		exit 1

	fi

}
pre_deinstall() {

	# 1. NUKE MODE
    if /usr/bin/wg-autoconf nuke 2>/dev/null; then

        echo "[INIT.D/wg-autoconfig_boot_cleanup] pre_deinstall() NUKE d0ne!" > /dev/kmsg
        return 0

    else

        echo "[INIT.D/wg-autoconfig_boot_cleanup] NUKE failed, using fallback cleanup" > /dev/kmsg


		for iface in $(ip link show 2>/dev/null | grep -oE "wg[0-9a-zA-Z_-]*" | sort -u); do

			ip link delete "$iface" 2>/dev/null

		done

		uci show network 2>/dev/null | grep "network\.wg[^=]*=interface" | \

			cut -d. -f2 | cut -d= -f1 | while read -r iface; do
			uci delete "network.$iface" 2>/dev/null
			uci delete "network.${iface}_peer" 2>/dev/null
		done

		for backup in /etc/config/*.BACKUP_PRE_WIREGUARD; do

			[ -f "$backup" ] || continue

			original="${backup%.BACKUP_PRE_WIREGUARD}"
			cp "$backup" "$original" 2>/dev/null
			rm -rf "$backup" 2>/dev/null

			uci commit network 2>/dev/null
			uci commit dhcp 2>/dev/null
			uci commit firewall 2>/dev/null
			/etc/init.d/firewall restart 2>/dev/null

		echo "[INIT.D/wg-autoconfig_boot_cleanup] Fallback cleanup completed" > /dev/kmsg

		done
	fi

	/etc/init.d/wg-autoconfig_boot_cleanup stop 2>/dev/null
	/etc/init.d/wg-autoconfig_boot_cleanup disable 2>/dev/null

	rm -rf /etc/init.d/wg-autoconfig_boot_cleanup 2>/dev/null
	rm -rf /tmp/wg-backup 2>/dev/null
	rm -rf /usr/bin/wg-autoconf 2>/dev/null
	rm -rf /usr/libexec/wg-autoconf/debug 2>/dev/null

}

post_deinstall() {

	# echo " " > /dev/kmesg
	echo "[wg-autoconf] Succesfully uninstalled. Check your network/firewall/dhcp. Reboot if needed." > /dev/kmesg
	echo "(: Thanks for using this tool!" > /dev/kmesg
	# echo " " > /dev/kmesg
}
