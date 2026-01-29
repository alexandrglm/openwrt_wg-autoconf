# wg-autoconf for OpenWRT
**Last Updated:** *2026-01-29*  
**Version:** *1.0.0r3*  
Automated WireGuard configuration tool for OpenWrt, making easy the interface setup, routing policies, firewall rules, even multiple simultaneous wireguard clients at the same time, via (ash) CLI.  

---

## Installation

### 1) From Release (Recommended)

1. Download the latest `.apk` and public rsa cert from releases
2. Copy to your OpenWrt device
3. Add the public key (if needed): `cp wg-autoconf.rsa.pub /etc/apk/keys/`
4. Install: `apk add ./wg-autoconf-*.apk`
5. Verify: `wg-autoconf help`
6. Wireguard configuration files PATH: `/etc/wireguard/*.conf`

### 2) From Source

wireguard-tools (and its requirements via 'apk add wireguard-tools' or 'opkg install wireguard-tools)


**Requirements:**
- Alpine-SDK <3.x already configured/enabled.
- aports, abuild, abuild-fetch, abuild-tar, abuild-sign, etc...

**Build:**
```bash
git clone https://github.com/alexandrglm/openwrt_wg-autoconf
cd openwrt_wg-autoconf
abuild -r
apk add ./packages/*/wg-autoconf-*.apk
```

**You may prefer to use [this script](https://github.com/alexandrglm/easy_apk_abuild/blob/main/easy_abuild.sh), included, for easy builds (interactive CLI).**

---

## Quick Start

### 1. Configuration Files

Place WireGuard `.conf` files in `/etc/wireguard/`. Standard format:

```ini
[Interface]
PrivateKey = <private_key>
Address = 10.2.0.2/32
DNS = 1.1.1.1 1.0.0.1

[Peer]
PublicKey = <public_key>
AllowedIPs = 0.0.0.0/0, ::/0
Endpoint = vpn.example.com:51820
PersistentKeepalive = 25
```

The tool extracts `PrivateKey`, `PublicKey`, `Address`, `Endpoint`, and `AllowedIPs` automatically.

### 2. Setup and Activate

**Automatic mode** (recommended for simple setups):
```bash
wg-autoconf setup myconfig
# Creates interface wg_myconfig from myconfig.conf
wg-autoconf up wg_myconfig
```

**Advanced mode** (manual interface naming):
```bash
wg-autoconf setup --advanced
# Prompts for all parameters, allows names like wg0, wgVPN, etc.
```

### 3. Verify

```bash
wg-autoconf status wg_myconfig
# Shows IP, status (UP/DOWN), endpoint, handshake, data transferred

wg show wg_myconfig
# Raw WireGuard interface details
```

### 4. Test Connectivity

```bash
ping -I wg_interface 8.8.8.8
curl --interface wg_myconfig https://ipinfo.io
traceroute -i yahoo.com
```
### 5. Route interfaces to any wireguard

- CMD: wg-autoconf routes set/unset *<wireguard inteface name>* *[destination interface]*  

```bash
wg-autoconf routes show

wg-autoconf routes set wg_interface1 lan1
wg-autoconf routes set wg_interface4 br_lan3_vlan11

wg-autoconf routes unset ...

```

---

### 6. Need more WireGuard tunnels in parallel? Just repeat the process
Manage each connection simultaneously, isolated from the others.  
Base configuration designed with security and isolation in mind. You can easily route a LAN port, a specific VLAN on a specific port to a specific bridge (or an entire bridge), and quickly roll back the changes.


---

### 7. Properly remove a single connection, all connections, or (safely) *nuke* everything

```bash
wg-autoconf clean

wg-autoconf clean wg_interface

wg-autoconf nuke-all

```

---

## Commands Reference

### Setup & Lifecycle

| Command | Purpose |
|---------|---------|
| `list` | Show available `.conf` files |
| `list-full` | Show configs + active interfaces with details |
| `test <name>` | Validate `.conf` file (key syntax, format) |
| `setup <name>` | Create interface from config (auto mode: `wg_<name>`) |
| `setup --advanced [name]` | Interactive setup with custom naming |

### Interface Control

| Command | Purpose |
|---------|---------|
| `up <iface>` | Activate interface (ifup) |
| `down <iface>` | Deactivate interface (ifdown) |
| `status [iface]` | Show all interfaces or specific one |
| `remove <iface>` | Delete interface and all configs |

### Routing

| Command | Purpose |
|---------|---------|
| `routes show` | List all configured routes |
| `routes show <wg_iface>` | Routes for specific interface |
| `routes set <wg_iface> <lan_iface>` | Route LAN traffic through WG |
| `routes unset <wg_iface> <lan_iface>` | Remove LAN routing |

### Cleanup

| Command | Purpose |
|---------|---------|
| `clean` | Interactive: select interface to remove |
| `clean <iface>` | Remove specific interface with confirmation |
| `nuke-all` | Destroy ALL configs, no prompts (pre_deinstall only) |
| `backups show` | List backup files |
| `backups restore [name]` | Restore from backup |

---

## Common Workflows

### Single VPN for Internet

```bash
# Setup
wg-autoconf setup us-free
wg-autoconf up wg_us-free

# Route a dedicated LAN interface through it
wg-autoconf routes set wg_us-free lan4

# Status
wg-autoconf status wg_us-free
```

**Why this design:** Policy-based routing isolates VPN traffic to specific LANs. Default WAN routing remains unchanged, preventing interference with other network segments.

### Multiple VPN Configurations

```bash
# Create multiple interfaces with different endpoints
wg-autoconf setup us-free
wg-autoconf setup eu-paid
wg-autoconf setup asia-test

# Only activate what's needed
wg-autoconf up wg_us-free
wg-autoconf routes set wg_us-free lan4

# eu-paid and asia-test stay configured but inactive
wg-autoconf status
```

**Why multiple configs:** Each can target different LAN segments. Address collision detection prevents accidental dual-activation of conflicting IPs.

### Manual Control (Advanced)

```bash
# Custom naming
wg-autoconf setup --advanced us-free
# Prompts: Interface name? → wgVPN1
# Creates wgVPN1, not wg_us-free

# Manually manage
wg-autoconf routes set wgVPN1 lan2
wg-autoconf routes show wgVPN1
wg-autoconf clean wgVPN1
```

---

## Technical Decisions

### Interface Naming Convention

- **Auto mode:** `wg_<confname>` (e.g., `wg_us-free`)
  - Predictable, avoids naming conflicts
  - UIanaged by config filename
  
- **Advanced mode:** No prefix requirement (e.g., `wg0`, `wgVPN`)
  - Manual control, flexible naming
  - Require validation: must start with `wg`

UCI/kernel restrictions on character sets. The `wg_` prefix in auto mode ensures consistency and enables emergency recovery via simple pattern matching.

### Routing Model: Policy-Based, Not Full Tunnel

The tool uses **policy routing** rather than forcing all traffic through WireGuard. This means:

```
Default behaviour (no routes set):
  - LAN  WAN: Normal path
  - External queries: Native ISP routing

After 'routes set wg0 lan3':
  - lan3 <--> WAN: Via wg0 (VPN)
  - lan3, lan2, lan1: Unaffected
  - WAN still uses native routing
```

1. **Isolation:** Only configured LANs traverse the tunnel
2. **Flexibility:** Can route different LANs through different VPNs
3. **Failover safety:** If VPN drops, isolated LANs still have fallback paths (if configured)
4. **Performance:** No overhead on uninvolved traffic

**Implication:** You cannot route "all traffic" automatically. Physical LAN ports attached to the LAN bridge cannot be selectively routed—only entire network interfaces (e.g., `lan4` if isolated from `br-lan`).

### Firewall Integration

When `routes set <wg> <lan>` is called, the tool automatically adds bidirectional forwarding rules:

```
config forwarding
    option src 'lan3'
    option dest 'wg_123'

config forwarding
    option src 'wg_123'
    option dest 'lan3'
```

**Why automatic:** Without these rules, the routing table alone won't allow packets through. The firewall zone system is orthogonal to IP routing—both must permit traffic.

**Why bidirectional:** Allows responses from the VPN back to the LAN without additional rules (**keeping native "defaults' forward REJECTed"**)

### Backuping

The tool creates backups of `/etc/config/{network,dhcp,firewall}` **once**, on first setup:

```
/etc/config/network.BACKUP_PRE_WIREGUARD
/etc/config/dhcp.BACKUP_PRE_WIREGUARD
/etc/config/firewall.BACKUP_PRE_WIREGUARD
```

**Why limited backups:**
1. Prevents backup proliferation (one per native conf subsystem)
2. `nuke-all` restores these automatically on full cleanup
3. Rotation/management left to user preference

**Why not per-operation:** All changes made to network, DHCP, and firewall are tagged with a unique ID that the application detects when an interface is brought up or down, made clean or nuke operation is performed.
An original backup of each native file is preserved before the first configuration, for emergency rescue purposes if anything goes wrong.

### DNS Configuration

The tool hardcodes `1.1.1.1 1.0.0.1` (Cloudflare) for all interfaces by default.

**Why:**
1. Many VPN providers don't supply DNS (E.g.: ProtonVPN free tier)
2. Avoids DNS leaks (**queries are tunneled, not on WAN path**)
3. Cloudflare is widely available, no geofencing

**Override if needed:**
Use `setup --advanced` mode, or set up via uci:

```bash
uci set network.wg_myconfig.dns='10.2.0.2 8.8.8.8'
uci commit network
ifup wg_myconfig
```

### Tag System for Config Management

All additions use tagged blocks:

```bash
# wg-autoconf network start id 1
config interface 'wg_us-free'
    ...
# wg-autoconf network end id 1
```

**Why tagged blocks:**
1. Enables safe removal without touching other configs
2. Supports multiple simultaneous setups (each has unique ID)
3. Survives manual edits (finds blocks by markers, not position)
4. Emergency recovery: `grep "# wg-autoconf" /etc/config/*` shows all managed sections

---

## Troubleshooting

### Interface exists but won't come up

```bash
# Check kernel interface state
ip link show wg_myconfig

# Check UCI configuration
uci show network.wg_myconfig

# View logs
logread | grep wg-autoconf
logread | grep netifd | tail -20
```

**Reason:** Missing or misconfigured `Address` field. WireGuard requires a local IP (e.g., `10.2.0.2/32`).

### Routes configured but traffic not flowing

```bash
# Verify routing table exists
ip route show table lan4_vpn_wg_myconfig

# Check firewall zones
uci show firewall | grep wg_myconfig

# Verify interface is UP
wg show wg_myconfig
```

**Reason:** Interface might be DOWN (check `wg-autoconf status`). Routes are configured but interface needs activation with `up`.


### Address already in use

```bash
# Check which interface owns the address
grep -r "10.2.0.2" /etc/config/network
```

The tool prevents accidental IP collisions during setup. If collision occurs, use `remove <iface>` or `setup` again with different IP.

### Firewall blocking traffic

```bash
# Check WG zone exists
uci show firewall | grep "option name 'wg_myconfig'"

# Check forwarding rules exist
uci show firewall | grep "forwarding"
```

If routes work but traffic is blocked, manually verify zone and forwarding rules are present. Also, **check your native configuration** (defaults/device -> Forward/Input/Output).

---

## File Locations

| Path | Purpose |
|------|---------|
| `/etc/wireguard/*.conf` | Configuration files (read by tool) |
| `/etc/config/network` | Network interface definitions |
| `/etc/config/dhcp` | DHCP/DNS configuration |
| `/etc/config/firewall` | Firewall zones and rules |
| `/etc/iproute2/rt_tables` | Custom routing tables |
| `/etc/libexec/wg-autoconf/logs/wg-autoconf.log` | Tool logs (persistent) |

---

## Known Limitations

1. **No automatic address assignment:** Requires manual IP configuration in `.conf` file. This is intentional—WireGuard peers are static.

2. **Single DNS per interface:** All interfaces use the same default DNS. Override using `setup --advanced` mode or per-interface via `uci`.

3. **No credential rotation:** As long as this tool depends on `wireguard-tools` package, keys might be updated by them, when availabnle, or  manually in `.conf` files.

4. **Policy routing only:** Due to enforcement/security reasons, this tool won'tforce "all traffic" through VPN automatically, so that's why an isolated device/interface are needed for being routed over them

5. **No multi-peer support:** But, planned!

---

## Development & Contribution

Tool is written in `ash` (POSIX shell subset).

- Modular functions (setup, cleanup, routing, firewall)
- Tag-based config block identification
- Idempotent operations (safe to rerun)
- Based on `wireguard-tools` package.
- No more external dependencies beyond those already included in OpenWrt

~~Logs are stored in `/etc/libexec/wg-autoconf/logs/wg-autoconf.log` for debugging.~~ Debugging app method included in sources, but not in builds nor in .apk's.

---

## Licence

MIT.

---

Made with love for OpenWrt ❤️.  
