# openwrt_wg-autoconf
Ash CLI tool for automating WireGuard setup on OpenWrt routers, easily.
---

## Overview
wg-autoconf is a command-line tool for automating WireGuard configuration on OpenWrt systems, by simplyfying the process of setting up WireGuard connections, managing network interfaces, and configuring routing policies.

## Requirements

#### 1. Dependences
```
ash, grep, awk, iproute2 (included by default)
wireguard-tools (and its requirements via 'apk add wireguard-tools' or 'opkg install wireguard-tools)
```

#### 2. WG .conf files:
Place WireGuard .conf files in `/etc/wireguard/`. The tool expects standard WireGuard config format:
```
[Interface]
PrivateKey = <privKey>
Address = <ip_addr/mask>
DNS = <dns>

[Peer]
PublicKey = <publicKey>
AllowedIPs = <allowed_ips_ipv4>, <allowed_ips_ipv6>
Endpoint = <endpoint_ip:port>
```
---

#### 3. Run it!
1. Run: `wg-autoconf setup your-config-name`
2. Activate: `wg-autoconf up wg0`
3. Test wg0 interface connectivity:
```sh
ping -I wg0 8.8.8.8
ping -I wg0 google.com
nslookup google.com <WG tun ip, ex. 10.0.8.1>
```


#### 3.2 Available Commands
- `$ wg-autoconf list` - Show available WireGuard configuration files
- `$ wg-autoconf setup <name>` - Configure WireGuard from a .conf file
- 
- `$ wg-autoconf up wg0` - Activate the WireGuard interface
- `$ wg-autoconf down wg0` - Deactivate the WireGuard interface
- `$ wg-autoconf status` - Show current WireGuard status

- `$ wg-autoconf routes show` - Show all route traffic through ANY interface
- `$ wg-autoconf routes set lan4` - Route LAN traffic through WireGuard
- `$ wg-autoconf routes unset lan4` - Remove LAN routing rules


#### 3.3  Expected behaviour
- The tool creates backups in `/tmp/wg-backup/`
- Network/Firewall/Dnsmasq configs. expected at default paths ( `/etc/config/...` )

- LAN routing uses policy routing to avoid interfering with default/native WAN traffic.
**Important:** When using policy routing for `wg0`, OpenWrt will NOT automatically route LAN switch ports through WireGuard. In a default setup, LAN ports belong to a bridge device such as:
```
....
config device
        option name 'br-lan'
        option type 'bridge'
        list ports 'lan1'
        list ports 'lan2'
        list ports 'lan3'
        list ports 'lan4'
...
```

Any ports attached to `br-lan` (or whatever your LAN bridge is called) **will not be available for WireGuard policy routing**, since the bridge continues to route them via the default LAN/WAN path.  

If you want **dedicated LAN ports routed through `wg0`**, you must **remove them from the LAN bridge first** and reassign them accordingly in your network configuration.

- As a consequence, `/etc/config/firewall` and `/etc/config/dhcp` should also be reviewed in order to integrate the WireGuard interface (which is always named `wg0` for consistency).  

The following example explains on how to dedicate a specific physical LAN port to WireGuard routing:

### NETWORK
```
...
config device
        option name 'br-lan'
        option type 'bridge'
        list ports 'lan4'
        list ports 'lan3'
        list ports 'lan2'

config interface 'lan'
        option device 'br-lan'
        option proto 'static'
        option ipaddr '192.168.1.1'
        option netmask '255.255.255.0'
        option ip6assign '60'

config interface 'lan1'
        option device 'lan1'
        option proto 'static'
        option ipaddr '192.168.2.1'
        option netmask '255.255.255.0'
        option ip6assign '60'

...
```

### FIREWALL

Assuming you want to dedicate your physical LAN port **lan1** (mapped from `lan1@eth0`) for WireGuard traffic only:

```
## A)  Zone configuration for <INTERFACE_NAME> dedicated port
config zone
    option name '<INTERFACE_NAME>'
    option input 'ACCEPT'
    option output 'ACCEPT'
    option forward 'REJECT'
    list network '<INTERFACE_NAME>'

## B)  Route traffic from <INTERFACE_NAME> -> WireGuard only
config forwarding
    option src '<INTERFACE_NAME>'
    option dest 'wg'

## C)  MANDATORY WireGuard Zone (wg0)
config zone
    option name 'wg'
    option input 'ACCEPT'
    option output 'ACCEPT'
    option forward 'ACCEPT'
    option masq '1'
    option mtu_fix '1'
    list network 'wg0'

# D)  Allow inbound WireGuard handshake from WAN
config rule
    option name 'Allow-WireGuard'
    option src 'wan'
    option dest_port '51820'
    option proto 'udp'
    option target 'ACCEPT'

# E)  FORWARDing for Internet (wg -> wan)
config forwarding
    option src 'wg'
    option dest 'wan'

# F)  Optional: LAN access (wg <-> lan)
config forwarding
    option src 'wg'
    option dest 'lan'

config forwarding
    option src 'lan'
    option dest 'wg'

# G)  Rule for wg -> lan traffic
config rule
    option name 'WG-to-LAN'
    option src 'wg'
    option dest 'lan'
    option target 'ACCEPT'
```

### DHCP / DNSMASQ
```
...

## A)  Dedicated iface for WG routing
config dhcp '<INTERFACE_NAME>'
    option interface '<INTERFACE_NAME>'
    option start '100'
    option limit '150'
    option leasetime '12h'

config dhcp '<INTERFACE_NAME> + wg' (Or wathever you want)
        option interface '<INTERFACE_NAME> + wg'
        option start '100'
        option limit '150'
        option leasetime '12h'
        option dhcpv6 'disabled'
        option ra 'disabled'

## B)   MANDATORY for wg0 default iface
config dhcp 'wg0'
        option interface 'wg0'
        option ignore '1'

config domain
        option name 'wg.local'
        option ip '10.8.0.254'

...
```

---

### What this achieves

- **`lan1` becomes an isolated port**, dedicated to VPN routing.
- **All traffic from `lan1` is forced through wg0**, not through WAN or regular LAN.
- **wg0 has its own firewall zone**, allowing NAT and route control without affecting default LAN/WAN behaviour.
- **WAN can accept WireGuard incoming UDP (example: `51820`)** for tunnel establishment.
- **LAN access from wg0 is optional**, depending on whether the tunnel should reach internal services.
- **DHCP assigns separate address spaces**, preventing collisions or unwanted LAN bridging.
- **wg0 ignores DHCP**, since WireGuard peers manage addressing internally.

These configurations are **indicative**, so always check how your network is currently structured (bridges, VLANs, DSA topology, and WAN setup) versus your goals.

---

### DNS for the WireGuard tunnel

Since some providers do not properly assign DNS servers (e.g., ProtonVPN free-tier), the current tunnel configuration applies Cloudflare IPv4 DNS (`1.1.1.1`) as a **hardcoded default by the tool**.

If you want to change it to your own DNS values, or to the provider’s DNS, adjust it via:
```
uci set ...
uci commit network
```

**DNS queries are always tunneled through WireGuard**; the native WAN path is **never** used for DNS resolution (prevents DNS leaks).

---

*2026/01/27*



