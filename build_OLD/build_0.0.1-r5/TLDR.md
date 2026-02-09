# wg-autoconf - Complete Documentation

Full guide to wg-autoconf: workflows, configuration, technical decisions, and troubleshooting.

---

## Table of Contents

1. [Installation Details](#installation-details)
2. [Configuration Format](#configuration-format)
3. [Common Workflows](#common-workflows)
4. [Technical Design](#technical-design)
5. [Troubleshooting](#troubleshooting)
6. [File Locations](#file-locations)
7. [Limitations](#limitations)

---

## Installation Details

### APK Format

wg-autoconf uses Alpine Linux's APK package format. Available on OpenWrt 24.12+ with APK enabled.

### From Release

1. Download `.apk` and `.rsa.pub` from GitHub releases
2. Copy to device: `scp file user@router:/tmp/`
3. Add public key:
   ```bash
   scp wg-autoconf.rsa.pub user@router:/etc/apk/keys/
   apk update
   ```
4. Install:
   ```bash
   apk add ./wg-autoconf-*.apk
   ```

### From Source (Alpine SDK)

**Prerequisites:**
- Alpine Linux with Alpine SDK installed
- `abuild` configured with all `abuild` tools (`abuild-sign`, `abuild-tar`, etc)

**Steps:**
```bash
git clone https://github.com/alexandrglm/openwrt_wg-autoconf
cd openwrt_wg-autoconf

# Generate signing keys first time (or use my own)
abuild-keygen -a -i

# Build
abuild -r

# Install
cp /*/key.rsa.pub /etc/apk/keys/
apk update
apk add .//*/wg-autoconf-*.apk
```

**Helper script:**
```bash
./easy_abuild.sh
# Interactive CLI for easy builds
```

---

## Configuration Format

### .conf File Location

Place WireGuard configuration files in:
```
/etc/wireguard/*.conf
```

### Format

```ini
[Interface]
PrivateKey = <base64-encoded-private-key>
Address = <vpn-ip>/<prefix>
DNS = <dns-servers-space-separated>

[Peer]
PublicKey = <base64-encoded-public-key>
AllowedIPs = <allowed-ips-comma-or-space-separated>
Endpoint = <server-ip-or-domain>:<port>
PersistentKeepalive = <seconds>
```

### Example

```ini
[Interface]
PrivateKey = uEcbqUV3DpqVgoElw2EV/m00T0Jwj9173y2nhTjnMnQ=
Address = 10.2.0.2/32
DNS = 1.1.1.1 1.0.0.1

[Peer]
PublicKey = X9DFBhm20MXz/f6H2uoApgNF+ZMmizfUXp0uW2XZiQ==
AllowedIPs = 0.0.0.0/0, ::/0
Endpoint = vpn.example.com:51820
PersistentKeepalive = 25
```

### Key Extraction

The tool reads these fields automatically:
- `PrivateKey` → Local private key
- `PublicKey` → Peer public key
- `Address` → Local IP (must be unique)
- `Endpoint` → VPN server
- `AllowedIPs` → Routes through tunnel
- `DNS` → Name servers (override with setup --advanced)

---

## Common Workflows

### Single VPN Connection

```bash
# 1. Create config file: /etc/wireguard/us-free.conf
# 2. Setup interface
wg-autoconf setup us-free
# Creates: wg_us-free

# 3. Activate
wg-autoconf up wg_us-free

# 4. Route a LAN
wg-autoconf routes set wg_us-free lan3

# 5. Test
ping -I wg_us-free 8.8.8.8
```

### Multiple VPN Connections

```bash
# Setup multiple configs
wg-autoconf setup us-free
wg-autoconf setup eu-paid
wg-autoconf setup asia-test

# Activate only what you need
wg-autoconf up wg_us-free
wg-autoconf up wg_eu-paid

# Route different LANs
wg-autoconf routes set wg_us-free lan3
wg-autoconf routes set wg_eu-paid lan5

# Status of all
wg-autoconf status
```

**Each interface has its own routing table, firewall zone, and address space. Zero interference between tunnels.**

### Manual Interface Naming

```bash
# Use --advanced for custom names
wg-autoconf setup --advanced us-free

# Prompts:
# 1. Interface name? → wgVPN1
# 2. Private key? → (enter or default from us-free.conf)
# 3. ... (rest of parameters)

# Result: creates wgVPN1 (not wg_us-free)
```

### Dedicated LAN to VPN

To route a physical LAN port exclusively through VPN:

1. **Isolate the port from br-lan:**
   ```bash
   uci set network.lan3=interface
   uci set network.lan3.device=eth0.2
   uci set network.lan3.proto=static
   uci set network.lan3.ipaddr=192.168.2.1
   uci set network.lan3.netmask=255.255.255.0
   uci commit network
   ```

2. **Setup VPN:**
   ```bash
   wg-autoconf setup myconfig
   wg-autoconf up wg_myconfig
   ```

3. **Route the port:**
   ```bash
   wg-autoconf routes set wg_myconfig lan3
   ```

Now all devices on `lan3` use the VPN exclusively.

### Quick Cleanup

```bash
# Interactive: choose which to remove
wg-autoconf clean

# Remove specific interface
wg-autoconf clean wg_myconfig

# Nuclear option: remove everything
wg-autoconf nuke-all
# (Requires typing YES)
```

---

## Technical Details

### Interface Naming

**Auto mode:** `wg_<confname>`
- Predictable (same config = same name)
- Prevents conflicts
- Easy emergency recovery

**Advanced mode:** Custom names (must start with `wg`)
- Full control
- Examples: `wg0`, `wgVPN1`, `wgUS`

### Routing: Policy-Based, Not Full Tunnel

The tool uses **policy routing**, not full tunnel redirection:

```
Without routes set:
  LAN <---> WAN via default gateway
  DNS via ISP

After routes set wg0 lan3:
  lan3 <---> WAN via wg0 (VPN)
  lan1, lan2, lan4 unchanged
  WAN path still available (failover)
```

**Main Goals:**
1. **Isolation:** Only selected LANs use VPN
2. **Flexibility:** Different LANs → different VPNs
3. **Failover:** If VPN drops, LANs have fallback
4. **Performance:** Zero overhead on uninvolved traffic

**Limitation:** Cannot force "all traffic" automatically. Requires isolated interfaces.

### Firewall Integration

When `routes set <wg> <lan>` is invoked:

```bash
# Automatically adds to /etc/config/firewall:
config forwarding
    option src 'lan3'
    option dest 'wg_myconfig'

config forwarding
    option src 'wg_myconfig'
    option dest 'lan3'
```

**Why needed:** IP routing alone isn't enough. OpenWrt's firewall enforces zones.

**Why bidirectional:** Allows responses from VPN back to LAN (with default REJECT enforced).

### Address Collision Detection

The tool checks all existing WireGuard interfaces:

```bash
wg-autoconf setup myconfig
# If 10.2.0.2 already in use by another interface:
# ERROR: Address 10.2.0.2 already in use by wg_other!
```

**Why:** WireGuard kernel refuses duplicate /32 addresses.

### Tagged Config Blocks

All modifications use comment tags:

```bash
# wg-autoconf network start id 1
config interface 'wg_myconfig'
    ...
# wg-autoconf network end id 1
```

**Benefits:**
- Safe removal (only tagged blocks deleted)
- Multiple simultaneous setups (unique IDs)
- Manual edits survive
- Emergency recovery: `grep "# wg-autoconf" /etc/config/*`

### Backups

First setup creates backups:
```
/etc/config/network.BACKUP_PRE_WIREGUARD
/etc/config/dhcp.BACKUP_PRE_WIREGUARD
/etc/config/firewall.BACKUP_PRE_WIREGUARD
```

`nuke-all` restores these automatically.

**Why limited:** Prevents backup proliferation. Idempotent operations mean rerunning setup is safe.

### DNS Configuration

Default: `1.1.1.1 1.0.0.1` (Cloudflare)

**Why:**
- Many VPN providers omit DNS configs (E.g. ProtonVPN free-tiers)
- Prevents DNS leaks (**queries still tunnel through WireGuard in all cases**)
- Cloudflare is robust is available globally

**Override**  
```bash
# Via advanced mode
wg-autoconf setup --advanced

# Or manually
uci set network.wg_myconfig.dns='10.2.0.2 9.9.9.9'
uci commit network
ifup wg_myconfig
```

---

## Troubleshooting

### Interface Exists But Won't Activate

```bash
# Check kernel state
ip link show wg_myconfig

# Check UCI config
uci show network.wg_myconfig

# Check logs
logread | grep wg-autoconf
logread | grep netifd | tail -20
```

**Common causes:**
- Missing or invalid `Address` field
- Key format issues
- Endpoint unreachable

**Fix:**
```bash
# Validate config
wg-autoconf test myconfig

# Recreate
wg-autoconf remove wg_myconfig
wg-autoconf setup myconfig
```

### Routes Configured But No Traffic

```bash
# Check table exists
ip route show table lan3_vpn_wg_myconfig

# Check rules
ip rule show | grep lan3

# Check interface is UP
wg show wg_myconfig
```

**Common causes:**
- Interface is DOWN (activate with `up`)
- Firewall zones missing
- Physical port still in br-lan

**Fix:**
```bash
# Ensure interface is active
wg-autoconf up wg_myconfig

# Check firewall zones
uci show firewall | grep wg_myconfig

# Remove and reconfigure routes
wg-autoconf routes unset wg_myconfig lan3
wg-autoconf routes set wg_myconfig lan3
```

### Address Already in Use

```bash
# Find which interface owns it
grep -r "10.2.0.2" /etc/config/network

# Check active interfaces
wg-autoconf status
```

**Fix:**
```bash
# Remove the conflicting interface
wg-autoconf remove wg_existing

# Or use different IP
wg-autoconf setup --advanced
# (manually specify new IP)
```

### Firewall Blocking Traffic

```bash
# Check zones exist
uci show firewall | grep "option name 'wg_myconfig'"

# Check forwarding rules
uci show firewall | grep "option src 'lan3'"
```

**Common causes:**
- Zone not created
- Forwarding rules missing
- Native firewall defaults reject traffic

**Fix:**
```bash
# Reconfigure routes (adds firewall rules)
wg-autoconf routes unset wg_myconfig lan3
wg-autoconf routes set wg_myconfig lan3

# Or manually check defaults
uci show firewall.@defaults[0]
# Should have: forward = REJECT (or ACCEPT)
```

### DNS Not Resolving

```bash
# Check DNS config
uci show network.wg_myconfig.dns

# Test resolution
nslookup google.com
dig @1.1.1.1 google.com
```

**Common causes:**
- Interface is DOWN
- DNS queries not tunneled (ISP DNS leak)

**Fix:**
```bash
# Ensure interface UP
wg-autoconf up wg_myconfig

# Override DNS
uci set network.wg_myconfig.dns='1.1.1.1 8.8.8.8'
uci commit network
ifup wg_myconfig
```

---

## File Locations

| Path | Purpose |
|------|---------|
| `/etc/wireguard/*.conf` | Configuration files |
| `/etc/config/network` | Network interfaces |
| `/etc/config/dhcp` | DHCP/DNS config |
| `/etc/config/firewall` | Firewall rules |
| `/etc/iproute2/rt_tables` | Custom routing tables |
| `/etc/libexec/wg-autoconf/logs/wg-autoconf.log` | Tool logs |

---

## Limitations

### 1. No Automatic Address Assignment

WireGuard peers are static. Addresses must be configured in `.conf` files manually.

### 2. Single DNS Server Set

All interfaces use the same default DNS. Override per-interface via `setup --advanced` or `uci`.

### 3. No Key Rotation

Keys must be updated manually in `.conf` files when providers rotate.

### 4. Policy Routing Only

Full "all traffic through VPN" requires isolated network interfaces. Cannot be enabled automatically by tool.

### 5. Single Peer Per Interface

Each WireGuard interface supports one peer. Multiple peers require separate configs.

### 6. No Web UI

Tool is CLI-only. Integration with LuCI not (yet) available.

---

## Development

- **Language:** ash (POSIX shell)  
- Setup/teardown functions
- Tag-based config management
- Policy routing with iproute2
- Firewall zone integration

**No external dependencies (except `wireguard-tools`) beyond the core packages from OpenWrt.

~~**Debugging:** `/etc/libexec/wg-autoconf/logs/wg-autoconf.log` (persistent)~~ 

---

## License

MIT

---

For issues or feedback: https://github.com/alexandrglm/openwrt_wg-autoconf/issues
