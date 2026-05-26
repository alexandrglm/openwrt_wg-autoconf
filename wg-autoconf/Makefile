#
# SPDX-License-Identifier: MIT
#
include $(TOPDIR)/rules.mk

PKG_NAME:=wg-autoconf
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_MAINTAINER:=Alexander Gomez <alexandrgomez@proton.me>
PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk

define Package/wg-autoconf
	SECTION:=net
	CATEGORY:=Network
	TITLE:=WireGuard Auto-Configuration tool for OpenWrt
	URL:=https://github.com/alexandrglm/openwrt_wg-autoconf
	DEPENDS:=+wireguard-tools +luci-proto-wireguard
endef

define Package/wg-autoconf/description
	Automatic WireGuard configuration manager for OpenWrt devices.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/wg-autoconf/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/wg-autoconf.source $(1)/usr/bin/wg-autoconf

	$(INSTALL_DIR) $(1)/usr/libexec/wg-autoconf/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wg-get_conf_value $(1)/usr/libexec/wg-autoconf/lib/wg-get_conf_value
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wg-interface       $(1)/usr/libexec/wg-autoconf/lib/wg-interface
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wg-route           $(1)/usr/libexec/wg-autoconf/lib/wg-route
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wg-setup           $(1)/usr/libexec/wg-autoconf/lib/wg-setup
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wg-validator       $(1)/usr/libexec/wg-autoconf/lib/wg-validator
	$(INSTALL_BIN) ./files/chandler.source             $(1)/usr/libexec/wg-autoconf/lib/chandler.sh

	$(INSTALL_DIR) $(1)/usr/libexec/wg-autoconf/scripts
	$(INSTALL_BIN) ./files/wg-autoconf_preinst.source    $(1)/usr/libexec/wg-autoconf/scripts/wg-autoconf_preinst.sh
	$(INSTALL_BIN) ./files/wg-autoconf_postinst.source   $(1)/usr/libexec/wg-autoconf/scripts/wg-autoconf_postinst.sh
	$(INSTALL_BIN) ./files/wg-autoconf_postupgrade.source $(1)/usr/libexec/wg-autoconf/scripts/wg-autoconf_postupgrade.sh
	$(INSTALL_BIN) ./files/wg-autoconf_prerm.source      $(1)/usr/libexec/wg-autoconf/scripts/wg-autoconf_prerm.sh

	$(INSTALL_DIR) $(1)/usr/libexec/wg-autoconf/atomics
	$(INSTALL_DIR) $(1)/usr/libexec/wg-autoconf/debug

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/wg-autoconf_boot_cleanup.source $(1)/etc/init.d/wg-autoconf_boot_cleanup

	$(INSTALL_DATA) ./files/states.source $(1)/usr/libexec/wg-autoconf/states
endef

define Package/wg-autoconf/preinst
#!/bin/sh
SCRIPT="/usr/libexec/wg-autoconf/scripts/wg-autoconf_preinst.sh"
if [ -f "$$SCRIPT" ]; then
	sh "$$SCRIPT"
fi
exit 0
endef

define Package/wg-autoconf/postinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	VERSION="1.0.0-r1"
	mkdir -p /etc/wireguard/
	touch /etc/wireguard/put_conf_files_here
	mkdir -p /usr/libexec/wg-autoconf/atomics
	cat << 'EOF' > /usr/libexec/wg-autoconf/states
IS_INSTALLED=1
IS_FIRST_EXEC=1
IS_PREV_TO_UPGRADE=0
IS_UPGRADED=0
EOF
	if [ -f /tmp/wg-autoconf-is-upgrading ]; then
		awk '
			/^IS_UPGRADED=0$$/ { print "IS_UPGRADED=1"; next }
			{ print }
		' /usr/libexec/wg-autoconf/states > /tmp/states.tmp
		mv /tmp/states.tmp /usr/libexec/wg-autoconf/states
		rm /tmp/wg-autoconf-is-upgrading
	fi
	chmod +x /etc/init.d/wg-autoconf_boot_cleanup
	sleep 1
	/etc/init.d/wg-autoconf_boot_cleanup enable 2>/dev/null
	echo "[wg-autoconf $$VERSION] Successfully installed!"
fi
exit 0
endef

define Package/wg-autoconf/prerm
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	SCRIPT="/usr/libexec/wg-autoconf/scripts/wg-autoconf_prerm.sh"
	if [ -f "$$SCRIPT" ]; then
		sh "$$SCRIPT"
	fi
fi
exit 0
endef

$(eval $(call BuildPackage,wg-autoconf))
