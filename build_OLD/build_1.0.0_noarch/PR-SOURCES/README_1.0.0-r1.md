# wg-autoconf for OpenWRT

Automated WireGuard configuration tool for OpenWrt. Supports multiple simultaneous WG clients with selective LAN routing + WireGuard server mode with multiple connected clients. Automatic firewall, DNS, routing and policy-based traffic management, safe operations

**Version:** v1.0.0

---

## Features

- **Multiple VPN Clients:** Setup and activate simultaneous VPN tunnels, route different LANs through different VPNs
- **WireGuard Server Mode:** Create VPN servers, add multiple client configurations, manage access
- **Policy-Based Routing:** Route only specific LANs through VPN, keep others on WAN (not full tunnel)
- **Automatic Configuration:** Firewall zones, forwarding rules, routing tables, DNS all handled automatically
- **Safe Operations:** Tagged config blocks, atomic operations, automatic backups with rollback
- **Collision Detection:** Prevents IP address conflicts before they break things
- **Debug Support:** Comprehensive logging for troubleshooting network issues
- **Boot Cleanup:** Automatically removes stale configs on reboot

---

## Installation

Requires OpenWrt 25.12+ with APK package manager enabled.

> [!WARNING]
> **wg-autoconf uses the new APK package format** (Alpine Linux). Supported on OpenWrt 25.12+ with APK enabled. At the time no Opkg is offered for older systems (but planned).

1. VIA LOCAL APK
```bash
# Add public key
cp wg-autoconf.rsa.pub /etc/apk/keys/

# Install
apk update
apk add ./wg-autoconf-*.apk
```

2. VIA OpenWRT repositories (when available)
```bash
apk update
apk add wg-autoconf
```

**Dependencies:** `wireguard-tools` (as of this will also add `kmod-wireguard` and `ip-tiny`|`ip-full`).

**THIS TOOL (NEITHER ANY WG TOOL) CANNOT WORK WITH CORE BUSYBOX `ip` BINARY**

---

## Quick Start: VPN Client

### 1. Create WireGuard Config File

Save `.conf` file in `/etc/wireguard/`:

```ini
[Interface]
PrivateKey = uEcbqUV3DpqVgoElw2EV/m00T0Jwj9173y2nhTjnMnQ=
Address = 10.2.0.2/32
DNS = 1.1.1.1, 1.0.0.1

[Peer]
PublicKey = X9DFBhm20MXz/f6H2uoApgNF+ZMmizfUXp0uW2XZiQ==
AllowedIPs = 0.0.0.0/0, ::/0
Endpoint = vpn.example.com:51820
PersistentKeepalive = 25
```

### 2. Setup & Activate

```bash
# Setup (creates wg_us-vpn)
wg-autoconf setup us-vpn

# Activate
wg-autoconf up wg_us-vpn

# Verify
wg-autoconf status wg_us-vpn
```

### 3. Route LAN Through VPN

```bash
# Route port3 (LAN3) through VPN
wg-autoconf routes set wg_us-vpn port3

# Verify
wg-autoconf status
ip route show table _vpn_wg_us-vpn_port3
```

### 4. Test

```bash
# Ping via VPN interface
ping -I wg_us-vpn 8.8.8.8

# Trace route
traceroute -i wg_us-vpn openwrt.org

# Check all active interfaces
wg-autoconf status
```

---

## Quick Start: WireGuard Server

> [!WARNING]
> Full server configuration is supported but currently remains in beta. Use with caution as this feature is under active development.

### 1. Create Server (Interactive)

```bash
wg-autoconf server create

# Responds to:
# Server name: myserver
# Subnet [10.99.0.0/24]: 10.99.0.0/24
# DNS [10.99.0.1]: 10.99.0.1
# Listen port [51820]: 51820
# Server endpoint (hostname/IP): vpn.example.com

# Output: Public key, subnet, DNS, port
# Config files saved to: /usr/libexec/wg-autoconf/configs/myserver/
```

### 2. Add Clients

```bash
# Add client 1 (auto-assigns IP 10.99.0.2)
wg-autoconf server add myserver client1
# Output: /usr/libexec/wg-autoconf/configs/myserver/client1.conf

# Add client 2 (auto-assigns IP 10.99.0.3)
wg-autoconf server add myserver client2

# Add more as needed
wg-autoconf server add myserver client3
```

### 3. Distribute Configs

Each `.conf` file is ready to use on client device:

```bash
# View generated config
cat /usr/libexec/wg-autoconf/configs/myserver/client1.conf

# Transfer to client (USB, email, QR code scan in WireGuard app, etc.)
# Client imports .conf in WireGuard app
# Client connects
# Handshake appears in: wg-autoconf server stats myserver
```

### 4. Manage

```bash
# List server + clients
wg-autoconf server list

# View statistics
wg-autoconf server stats myserver

# Revoke client access (requires confirmation)
wg-autoconf server revoke myserver client1

# Remove entire server
wg-autoconf server remove myserver
```

---

## Commands Reference

### Configuration Management

```bash
wg-autoconf list                    # Show .conf files in /etc/wireguard/
wg-autoconf test myconfig           # Validate config file
wg-autoconf setup myconfig          # Create interface (name: wg_myconfig)
wg-autoconf manual myconfig         # Advanced setup (custom interface name)
```

### Interface Control

```bash
wg-autoconf status                  # Show all active interfaces
wg-autoconf status wg_myconfig      # Show specific interface
wg-autoconf up wg_myconfig          # Activate
wg-autoconf down wg_myconfig        # Deactivate
wg-autoconf remove wg_myconfig      # Delete completely + restore backup
```

### Routing

```bash
wg-autoconf routes show             # List all configured routes
wg-autoconf routes set wg_myconfig port3      # Route LAN through VPN
wg-autoconf routes unset wg_myconfig port3    # Remove routing
```

### Server Management

```bash
wg-autoconf server create           # Create server (interactive)
wg-autoconf server add <name> <user>        # Add client
wg-autoconf server revoke <name> <user>     # Revoke client
wg-autoconf server remove <name>            # Delete server
wg-autoconf server list             # List servers + clients
wg-autoconf server stats <name>     # Show statistics
```

### Cleanup

```bash
wg-autoconf clean                   # Interactive (choose which to remove)
wg-autoconf clean wg_myconfig       # Remove specific interface
wg-autoconf nuke                    # Remove ALL (requires YES confirmation)
```

### Backup & Restore

```bash
wg-autoconf backups show            # List available backups
wg-autoconf backups restore         # Restore latest
wg-autoconf backups restore backup_name   # Restore specific
wg-autoconf backups diag [config]   # Diagnose backup issues
wg-autoconf backups cleanup         # Remove old backups
```

### Settings

```bash
wg-autoconf settings show           # Display current settings
wg-autoconf settings set dns 8.8.8.8            # Change default DNS
wg-autoconf settings set colours off            # Disable CLI colours
wg-autoconf settings set port 51821             # Change default port
wg-autoconf settings set verbose 1              # Enable verbose logging
wg-autoconf settings edit           # Edit settings file directly
wg-autoconf settings reset          # Reset to defaults
```

### Debug

```bash
wg-autoconf debug on                # Enable debug logging
wg-autoconf debug off               # Disable debug logging
wg-autoconf debug show              # View debug log
wg-autoconf debug live              # Tail log live
wg-autoconf debug clear             # Clear debug log
wg-autoconf debug status            # Check debug state
wg-autoconf debug states            # View state machine
wg-autoconf debug network           # View network config
wg-autoconf debug firewall          # View firewall rules
```

### Upgrade

```bash
wg-autoconf upgrade                 # Upgrade wg-autoconf
```

### Global Flags

```bash
wg-autoconf --verbose command       # High verbosity (any command)
wg-autoconf -v command              # Short form
wg-autoconf --help                  # Show help
wg-autoconf -h                      # Short form
```

---

## Use Cases

### Multiple VPNs, Different LANs

Route different LANs through different VPN providers:

```bash
# Setup both VPNs
wg-autoconf setup us-vpn
wg-autoconf setup eu-vpn
wg-autoconf up wg_us-vpn
wg-autoconf up wg_eu-vpn

# Route LAN3 through US VPN
wg-autoconf routes set wg_us-vpn port3

# Route LAN4 through EU VPN
wg-autoconf routes set wg_eu-vpn port4

# Verify
wg-autoconf status
```

Traffic from port3 uses US VPN. Traffic from port4 uses EU VPN. LAN1 (br-lan) uses default WAN. Zero interference.

### Failover Setup

Route via VPN but keep WAN as fallback:

```bash
wg-autoconf setup primary-vpn
wg-autoconf up wg_primary-vpn
wg-autoconf routes set wg_primary-vpn port3

# If VPN drops, traffic automatically falls back to WAN
# (because routing table lookup fails, uses default route)
```

### Server + Multiple Clients

Create VPN server that multiple devices connect to:

```bash
# Setup server
wg-autoconf server create
# > Server: mycompany
# > Subnet: 10.0.0.0/24
# > Endpoint: vpn.example.com

# Add clients
wg-autoconf server add mycompany laptop
wg-autoconf server add mycompany phone
wg-autoconf server add mycompany tablet

# Distribute configs
# Each device imports .conf in WireGuard app
# All devices connected to same server

# Monitor
wg-autoconf server stats mycompany
```

### Manual Interface Setup

Custom interface naming (not auto `wg_*`):

```bash
wg-autoconf manual myconfig
# Prompts:
# Interface name? > wgVPN1
# Private key? > [defaults from .conf or manual entry]
# ... rest of parameters

# Result: Creates wgVPN1 (not wg_myconfig)
```

---

## Troubleshooting

### Interface Won't Come Up

```bash
# Check configuration validity
wg-autoconf test myconfig

# View detailed logs
wg-autoconf debug on
wg-autoconf up wg_myconfig --verbose
wg-autoconf debug show

# Check UCI config
uci show network.wg_myconfig

# Check system logs
logread | grep wg-autoconf
```

### Routes Configured But No Traffic

```bash
# Verify routing table exists
ip route show table _vpn_wg_myconfig_port3

# Check IP rules
ip rule show

# Verify interface is UP
wg show wg_myconfig

# Check firewall zones
uci show firewall | grep wg_myconfig

# Check nftables rules
nft list chain inet fw4 accept_to_wg_myconfig
```

### Address Already In Use

```bash
# Find which interface has the IP
grep -r "10.2.0.2" /etc/config/network

# List all active interfaces
wg-autoconf status

# Use different IP in config file
# Edit /etc/wireguard/myconfig.conf
# Change Address to new IP
# Re-setup: wg-autoconf setup myconfig
```

### DNS Not Working

```bash
# Check if interface is UP
wg-autoconf status

# Test with specific DNS
dig @8.8.8.8 google.com

# Check DNS config on interface
uci show network.wg_myconfig.dns

# Set DNS explicitly
uci set network.wg_myconfig.dns='8.8.8.8 8.8.4.4'
uci commit network
ifup wg_myconfig
```

### Restore From Broken State

```bash
# View available backups
wg-autoconf backups show

# Restore latest
wg-autoconf backups restore

# If that fails, nuke and restart
wg-autoconf nuke
# (Removes everything, clean slate)
```

---

## Performance Notes

No realtime traffic impact after setup. All routing happens in kernel (no userland processing).

It's down to your router and its capacity what actual performance you achieve.

A router with real offloading/multicore enabled will always be better; but wg-autoconf has been developed and tested on various old boards, without any offloading, single-core, different CPU architectures, low RAM, without any issues.

For a test on a 2010 device (Arcadyan VRV9510KWAC23, 1 CPU CORE @ 500MHz, 100Mb free RAM, WITHOUT any offloading), the times were:

- **Firewall reload:** ~1-2 seconds (can't be avoided, system limitation)
- **Routes set:** ~1-2 seconds (includes firewall reload)
- **Multi-interface overhead:** Minimal (each has own routing table)
- **Boot cleanup:** ~5~8 seconds/interface (runs before network startup)


---

## Design Notes

- **Policy-based routing:** Only selected LANs use VPN, others unaffected
- **Tagged config blocks:** Safe removal without touching manual edits
- **Atomic operations:** Backup before changes, rollback on failure
- **Boot cleanup:** Prevents orphaned interfaces after crashes
- **NFTables workaround:** Re-applies rules after firewall reload (Session 13 fix)
- **Dynamic priorities:** Routing table IDs × 10 = rule priorities (prevents collisions)

---

## Limitations

1. **Single Peer per WG Interface:** WireGuard limitation, not tool
2. **No Key Rotation:** Manual updates required
3. **No Web UI:** CLI only. **Pending a luci-proto-x module**
4. **APK Only:** No Opkg version

---

## File Locations

```
/etc/wireguard/                           Config files (.conf)

/etc/config/network                       Network interfaces
/etc/config/firewall                      Firewall rules
/etc/config/dhcp                          DNSMASQ/DHCP sets
/etc/iproute2/rt_tables                   Routing tables

/usr/libexec/wg-autoconf/states           Interface state (persistent)
/usr/libexec/wg-autoconf/user_settings    User settings override
/usr/libexec/wg-autoconf/debug/           Debug logs
/usr/libexec/wg-autoconf/configs/         Generated server configs

/etc/init.d/wg-autoconf_boot_cleanup      Boot cleanup service
```

---

## Full Documentation

See [DOCS.md](./DOCS.md) for complete technical documentation, architecture, design decisions, and advanced troubleshooting.

---

## License

MIT

---
