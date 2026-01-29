# wg-autoconf for OpenWRT

Automated WireGuard configuration tool for OpenWrt, making easy the interface setup, routing policies, firewall rules, even multiple simultaneous wireguard clients at the same time, via (ash) CLI.  

---

## Installation

> [!WARNING]
> **wg-autoconf uses the new APK package format** (Alpine Linux). Supported on OpenWrt 23.05+ with APK enabled. At the time no Opkg is offered for older systems (but planned).


### From Release (Recommended)

1. Download .apk and public key from releases
2. Copy to your OpenWrt device, then:
3. Add pub key to `/etc/apk/keys/`
4. `# apk update && apk add ./wg-autoconf-*.apk`


### From Source

Requires an Alpine-SDK 'abuild' setupa already compiled and working:

```bash
git clone https://github.com/alexandrglm/openwrt_wg-autoconf
cd openwrt_wg-autoconf

abuild -r
apk add ./packages/*/wg-autoconf-*.apk
```
- **You may prefer to use the included [building script](https://github.com/alexandrglm/easy_apk_abuild/blob/main/easy_abuild.sh), or easy builds via an interactive CLI.**


---

## Quick Start

### 1. Place Wireguard config files

- PATH: `/etc/wireguard/*.conf`:

- Example for config files:
```ini
[Interface]
PrivateKey = <key>
Address = 10.2.0.2/32
DNS = 1.1.1.1 1.0.0.1

[Peer]
PublicKey = <key>
AllowedIPs = 0.0.0.0/0, ::/0
Endpoint = vpn.example.com:51820
PersistentKeepalive = 25
```

### 2. Setup and activate

```bash
# Auto mode (creates wg_myconfig)
wg-autoconf setup myconfig
wg-autoconf up wg_myconfig

# Or manual mode
wg-autoconf setup --advanced
```

### 3. Route a LAN through VPN

- CMD: `wg-autoconf routes set <wg_iface> <desired_iface>

```bash
# wg-autoconf routes show
# wg-autoconf routes set wg_myconfig lan3
# wg-autoconf routes unset wg_myconfig lan4
```

### 4. Test Connectivity:

```bash
# wg-autoconf status
# ping -I wg_myconfig 8.8.8.8
# traceroute -i wg_myconfig openwrt.org
```

### 6. Need more WireGuard tunnels in parallel?
Just repeat the process.  

Manage each connection simultaneously, isolated from the others.   
Base configuration designed with security and isolation in mind.   

You can easily route a LAN port, a specific VLAN on a specific port to a specific bridge (or an entire bridge), and quickly roll back the changes.


### 7. Properly remove a single connection, all connections, or (safely) *nuke* everything

```bash
# wg-autoconf routes unset wg_myconfig lan3
# wg-autoconf down wg_myconfig
# wg-autoconf remove wg_myconfig
```

Or:  

```bash
# wg-autoconf clean
# wg-autoconf clean wg_interface
# wg-autoconf nuke-all (removes  & clean ALL ifaces, tables, rules with no confirmation)
```

---

## Commands

| Command | Purpose |
|---------|---------|
| `list` | Show available configs |
|||
| `setup <n>` | Create from config (auto: `wg_<n>`) |
| `setup --advanced` | Interactive setup with custom naming |
|||
| `up <iface>` | Activate interface |
| `down <iface>` | Deactivate interface |
| `remove <iface>` | Remove rules, tables, routes, interface |
|||
| `status [iface]` | Show interface status |
|||
| `routes show` | List all routes |
| `routes set <wg> <lan>` | Route LAN through WG |
| `routes unset <wg> <lan>` | Remove routing |
|||
| `clean [iface]` | Remove interface (interactive) |
| `nuke-all` | Remove ALL configs (no prompts) |
|||
| `backups show` | List available backups config files |
| `backups restore [name]` | Restore backup files |

Full help & usage: `wg-autoconf --help`

---

## Design

- **Policy-based routing:** Route only specific LANs through VPN
- **Multiple simultaneous tunnels. Each isolated with own firewall zone & configs.
- Automatic firewall/dnsmasq/network configuratios, bidirectional forwarding, no manual config needed.
- Safe add/removal configurations without breaking other configs
- **Address collision detection:** Prevents accidental conflicts

---

## Troubleshooting

**Interface won't come up?**
```bash
# logread | grep wg-autoconf
# uci show network.wg_myconfig
```

**Routes configured but no traffic?**
```bash
# ip route show table lan4_vpn_wg_myconfig
# wg show wg_myconfig
```

**More help:** See [TLDR.md](./TLDR.md) for detailed documentation.

---

## Links

- **GitHub:** https://github.com/alexandrglm/openwrt_wg-autoconf
- **Issues:** https://github.com/alexandrglm/openwrt_wg-autoconf/issues

## License

MIT

---

Made for OpenWrt with 🥰
