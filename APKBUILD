# Maintainer: Alexander Gomez <148530039+alexandrglm@users.noreply.github.com>
pkgname=wg-autoconf
pkgver=1.0.0
pkgrel=3
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
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/debug.source" "$pkgdir/usr/libexec/wg-autoconf/debug.sh"
	install -Dm755 "$builddir/etc/init.d/wg-autoconfig_boot_cleanup.source" "$pkgdir/etc/init.d/wg-autoconfig_boot_cleanup"
	install -Dm755 "$builddir/etc/wireguard/.WG_CONF_FILES_GOES_HERE" "$pkgdir/etc/wireguard/.WG_CONF_FILES_GOES_HERE"
}

post_install() {

	# echo "[wg-autoconf] Enabling  /etc/init.d/wg-autoconfig_boot_cleanup and LAN4 ..." > /dev/kmesg
	chmod +x /etc/init.d/wg-autoconfig_boot_cleanup
	sleep 1
	/etc/init.d/wg-autoconfig_boot_cleanup enable
	/etc/init.d/wg-autoconfig_boot_cleanup start
	sleep 1
	echo ""
	uci commit dhcp
	uci commit firewall
	uci commit network
	sleep 1
	# echo "[wg-autoconf] Check README, check network, firewall, dhcp, init.d cleanups, ..." > /dev/kmesg
	# echo "[wg-autoconf] D0ne. 3njoy! " > /dev/kmesg

}


pre_deinstall() {

	# 1. NUKE MODE
	if /usr/bin/wg-autoconf nuke-all >/dev/null 2>&1; then

		# echo "[DEBUG wg-autoconf] PRE_DEINSTALL() NUKE-ALL start WORKS!" > /dev/kmesg
		sleep 3

	else

		# echo "[DEBUG wg-autoconf] PRE_DEINSTALL() NUKE-ALL start DIDNT WORK!!!!" > /dev/kmesg
		# 2. or EMERGENCY MODE
		if [ -f "/etc/config/network.BACKUP_PRE_WIREGUARD" ]; then
			cp /etc/config/network.BACKUP_PRE_WIREGUARD /etc/config/network
		fi

		if [ -f "/etc/config/dhcp.BACKUP_PRE_WIREGUARD" ]; then
			cp /etc/config/dhcp.BACKUP_PRE_WIREGUARD /etc/config/dhcp
		fi

		if [ -f "/etc/config/firewall.BACKUP_PRE_WIREGUARD" ]; then
			cp /etc/config/firewall.BACKUP_PRE_WIREGUARD /etc/config/firewall
		fi
		rm -f /etc/config/*.BACKUP_PRE_WIREGUARD 2>/dev/null

		# 3. THEN, EMERGENCY COMMITS
		uci commit firewall 2>/dev/null
		uci commit dhcp 2>/dev/null
		uci commit network 2>/dev/null
		/etc/init.d/firewall reload

	fi

	/etc/init.d/wg-autoconfig_boot_cleanup stop 2>/dev/null
	/etc/init.d/wg-autoconfig_boot_cleanup disable 2>/dev/null

	rm -f /etc/init.d/wg-autoconfig_boot_cleanup 2>/dev/null
	rm -rf /tmp/wg-backup 2>/dev/null


}
post_deinstall() {

	# echo " " > /dev/kmesg
	echo "[wg-autoconf] Succesfully uninstalled. Check your network/firewall/dhcp. Reboot if needed." > /dev/kmesg
	echo "(: Thanks for using this tool!" > /dev/kmesg
	# echo " " > /dev/kmesg
}
sha512sums="
e04631de2abd18c845d329c586d367328b774d714be7418f2d5f070d39fd944f59f4827f87e2cd5e88cdc67907dbfe9a849b3ba42e3e7d2b08599bdedf71c265  source.tar.gz
"
