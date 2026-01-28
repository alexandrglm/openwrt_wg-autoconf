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
	install -Dm755 "$builddir/usr/bin/wg-autoconf.sh" "$pkgdir/usr/bin/wg-autoconf"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/debug.sh" "$pkgdir/usr/libexec/wg-autoconf/debug.sh"
	install -Dm755 "$builddir/usr/libexec/wg-autoconf/log/wg-autoconf.log" "$pkgdir/usr/libexec/wg-autoconf/wg-autoconf.log"
	install -Dm755 "$builddir/etc/init.d/wg-autoconfig_boot_cleanup" "$pkgdir/etc/init.d/wg-autoconfig_boot_cleanup"
	install -Dm755 "$builddir/etc/wireguard/.WG_CONF_FILES_GOES_HERE" "$pkgdir/etc/wireguard/.WG_CONF_FILES_GOES_HERE"
}

post_install() {

	echo "[wg-autoconf] Enabling  /etc/init.d/wg-autoconfig_boot_cleanup and LAN4 ..." > /dev/kmesg
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
	echo "[wg-autoconf] Check README, check network, firewall, dhcp, init.d cleanups, ..." > /dev/kmesg
	echo "[wg-autoconf] D0ne. 3njoy! " > /dev/kmesg
	echo ""
}


pre_deinstall() {


	# 1. USE CLEANUP
	if /usr/bin/wg-autoconf nuke-all >/dev/null 2>&1; then

		echo "[DEBUG wg-autoconf] PRE_DEINSTALL() NUKE-ALL start WORKS!" > /dev/kmesg
		sleep 3

	else

		echo "[DEBUG wg-autoconf] PRE_DEINSTALL() NUKE-ALL start DIDNT WORK!!!!" > /dev/kmesg
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

		# 3. EMERGENCY COMMITS
		uci commit firewall 2>/dev/null
		uci commit dhcp 2>/dev/null
		uci commit network 2>/dev/null

	fi

	/etc/init.d/wg-autoconfig_boot_cleanup stop 2>/dev/null
	/etc/init.d/wg-autoconfig_boot_cleanup disable 2>/dev/null

	rm -f /etc/init.d/wg-autoconfig_boot_cleanup 2>/dev/null
	rm -f /etc/init.d/wg_boot_LAN4 2>/dev/null

	rm -rf /tmp/wg-backup 2>/dev/null


}
post_deinstall() {

	echo " " > /dev/kmesg
	echo "[wg-autoconf] Succesfully uninstalled. Check your network/firewall/dhcp. Reboot if needed.(: Thanks for using this tool!" > /dev/kmesg
	echo " " > /dev/kmesg
}
