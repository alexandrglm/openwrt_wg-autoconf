Alpine Linux / OpenWRT Signing Key (ECDSA prime256v1)
Generated on: Wed Feb 11 23:21:03 UTC 2026
Original packager: builder@alpine
Key name: wg-autoconf-DEBUG
Key fingerprint (SHA1): 93534ea3da9ef1fa7b7a657600a4be8cb45e87cb
Key type: ECDSA (Elliptic Curve) - Compatible with OpenWRT 24.10+

FILES:
  - wg-autoconf-DEBUG.pem     : Private signing key (KEEP SECURE! NEVER SHARE)
  - wg-autoconf-DEBUG.pem.pub : Public verification key (SHARE WITH USERS)
  - wg-autoconf-DEBUG.abuild.conf : Example abuild configuration

INSTRUCTIONS FOR OPENWRT (24.10+):

1. Copy the public key to OpenWRT:
   scp wg-autoconf-DEBUG.pem.pub root@openwrt-router:/etc/apk/keys/wg-autoconf-DEBUG.pem

   NOTE: OpenWRT expects .pem extension in /etc/apk/keys/ (without .pub)

2. Verify the key is installed:
   ssh root@openwrt-router "ls -la /etc/apk/keys/"

3. Now packages signed with this key will install without --allow-untrusted

FOR ALPINE LINUX:
   cp wg-autoconf-DEBUG.pem.pub /etc/apk/keys/wg-autoconf-DEBUG.pem

TO SIGN PACKAGES WITH THIS KEY:
   abuild-sign -k wg-autoconf-DEBUG.pem package.apk

TO VERIFY PACKAGE SIGNATURE:
   abuild-verify -k wg-autoconf-DEBUG.pem.pub package.apk

KEY TYPE: ECDSA prime256v1 (P-256) - Same as OpenWRT official keys
GENERATED WITH: Alpine 3.23 (stable) - Manual ECDSA generation
