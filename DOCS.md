# wg-autoconf 1.0.0-r1 Documentation v1

Complete technical reference for `wg-autoconf`: architecture, internals, design decisions, and troubleshooting.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Execution Lifecyle](#execution-lifecyle)
3. [State Machine](#state-machine)
4. [Syntax/Naming Convention](#naming-convention)
5. [Routing System (Policy-Based)](#routing-system-policy-based)
6. [WireGuard Server Support](#wireguard-server-support)
7. [Firewall Integration](#firewall-integration)
8. [Atomic Operations & Backups](#atomic-operations--backups)
9. [NFTables Integration](#nftables-integration)
10. [Boot Cleanup Service](#boot-cleanup-service)
11. [Multi-Interface Handling](#multi-interface-handling)
12. [Design Decisions & DEBUG! Notes](#design-decisions--debug-notes)
13. [Performance Considerations](#performance-considerations)
14. [Advanced Troubleshooting](#advanced-troubleshooting)

---

## 1.   Architecture Overview

### System Components

![wg-autoconf main components](./DOCS/img/DIAGRAMS/components.png)



### 2.  Execution Lifecycle (Setup -> Activate -> Route)

![](./DOCS/img/DIAGRAMS/ESTADOS_PIPELINE.png)

***

##### wg-autoconf setup myconfig
![](./DOCS/img/DIAGRAMS/setup_myconfig.png)
1. Parse the given `/etc/wireguard/myconfig.conf`
2. Validate all fields (keys, IPs, endpoints)
3. Check for collisions
4. Create interface in UCI (`/etc/config/network`)
5. Write state: ID_X_NAME=myconfig
6. Commit

- Result: Interface "wg_myconfig" created but **DOWN**

***

##### wg-autoconf up wg_myconfig
![](./DOCS/img/DIAGRAMS/up_wg_iface.png)
1. Read state to get private key
2. Create WireGuard interface with `ip link add type wireguard`
3. Set private key with `wg set`
4. Assign IP address and bring UP
5. Wait for interface operational
6. Write state: IS_ACTIVE=1

- Result: Interface "wg_myconfig" is UP, can ping peer

***

##### wg-autoconf routes set wg_myconfig lan3
![](./DOCS/img/DIAGRAMS/ROUTES_SET.png)
1. Create routing table: _vpn_wg_myconfig_lan3
2. Add dynamic IP rules (priority = table_id * 10)
3. Add routes: subnet → lan3, default → wg_myconfig
4. Create firewall zone & forwarding rules
5. Reload firewall (triggers nftables rebuild)
6. Re-add nftables rules (critical workaround)
7. Write state: IS_RT_TABLES_IN_USE=1

- Result: All traffic from port3 subnet routes through VPN

---

## 3.   State Machine

### Purpose

Persistent storage to offload responsibilities of 'noticing the users'/actions in the .apk's own lifecycles. The binary itself takes care of its 'self-care' via an state machine where, in each operation, verifies the required states before performing it, and proceeds by adjusting states as needed.

![wg-autoconf State Machine, states per operations](./DOCS/img/DIAGRAMS/STATE_MACHINE.png)

Through the machine's keys and values, zero-failures zero-risk usage is ensured by testing against nearly all possible error scenarios to prevent disaster (*'Keep everything simple but safe', avoids hard-resets, allows to recover/self-heal before disaster may occur)"

### Format

Key-value pairs, one per line:

```
# Metadata
ID_1_NAME=myconfig
ID_1_CREATED=1708457600
ID_1_CONF_NAME=myconfig

# Configuration
ID_1_PRIVKEY=uEcbqUV3DpqVgoE...
ID_1_PUBKEY=X9DFBhm20MXz/f6H...
ID_1_ADDRESS=10.2.0.2/32
ID_1_ENDPOINT=vpn.example.com:51820
ID_1_PORT=51820

# State
ID_1_IS_CREATED=1
ID_1_IS_ACTIVE=0
ID_1_IS_RT_TABLES_IN_USE=0

# Backup references
ID_1_BACKUP_NETWORK=network.BACKUP_PRE_WIREGUARD_ID_1
ID_1_BACKUP_FIREWALL=firewall.BACKUP_PRE_WIREGUARD_ID_1
```

### File Location

```
/usr/libexec/wg-autoconf/states
```

### Read/Write Operations

```bash
# Write (creates or updates)
state_write "ID_1_NAME" "myconfig"

# Read
value=$(state_read "ID_1_NAME")

# Check existence
if state_read "ID_1_NAME" >/dev/null 2>&1; then
    # exists
fi

# Get interface ID from name
id=$(state_get_id "myconfig")

# List all interface names
state_list_ifaces  
# Returns: "1:myconfig 2:othervpn"
```

---

### Atomic File Operations

#### WHY ATOMIC: Prevents corruption if write fails mid-operation

```bash

state_write() {

    # 1. Create unique temp file (uses atomic counter)
    temp_file="${ATOMIC_PATHS}/states.write.${counter}.atomic"
    
    # 2. Read entire state file
    # 3. Find key and replace OR append
    # 4. Write to temp
    
    # 5. Verify temp is not empty (critical!)
    if [ ! -s "$temp_file" ]; then

        # Temp empty = write failed, return error
        return 1
    
    fi
    
    # 6. Atomic move (mv is atomic on same filesystem)
    mv "$temp_file" "$STATE_FILE"
}
```

**Decision:** While many file operations (as `sed`, `awk`) use to be 'atomics by default', a three-stages approach (read each write reads entire file, modifies in memory, writes atomically) prevents partial writes or corruption.

---

# 4.    Naming Convention

`wg-autoconf` pretends to use a high level and hard syntax for any usage, but nothing quite the opposite.

The syntax aims to be mnemonic and related to the binaries it uses. A simple, consistent naming convention across all components to ensure clarity, avoid conflicts, and enable dynamic management.


## By Design

1. **One name to rule them all:** The interface name (`wg_*`) is the only identifier the user needs to remember
2. **Derived naming:** All other names (peers, tables, zones, state files) derive automatically from the interface name
3. **No device names:** We work with interface names, not low-level device names
4. **Predictable patterns:** Names follow predictable patterns for easy scripting and debugging
5. **Collision avoidance:** Prefixes (`_vpn_`) and suffixes (`_portX`) prevent conflicts with system resources
6. **Self-documenting:** Names describe their purpose (e.g., `_vpn_wg_home_port3` clearly means "VPN routing for wg_home on port3")

This convention ensures that even with multiple tunnels and complex routing configurations, everything remains organised, discoverable, and manageable (and scalable, maintainable) through wg-autoconf's flow.

---

#### Interface Names

#### For SETUP (Auto Mode)
**UNDERSCORED**:  
```
wg_<conf filename>
```
#### For MANUAL
**NOT UNDERSCORED**:
```
wg<any name>
```
#### For SERVERS
```
wg_server_<any name>
``` 

- **Identifier:** Lowercase, alphanumeric, underscores allowed (**no hyphens allowed by WireGuard design!**)

- **Examples:** `wg_home`, `wg0`, `wg_server_yupi`, `wg_telegram`

The **interface** name is **the single source of truth**: **everything else derives from it**.

---

## Peer Names

```
<interface>_<peer_identifier>
```

- **Format:** Interface name + underscore + peer identifier
- **Peer identifier:** Usually based on peer description or public key suffix
- **Examples:**
  - `wg_home_phone`
  - `wg_server_yupi_yeye`
  - `wg_work_laptop`

---

## Routing Tables

```
_vpn_<interface>_<lan_interface>
```

- **Format:** `_vpn_` prefix + interface name + LAN interface
- **Examples:**
  - `_vpn_wg_home_port3`
  - `_vpn_wg_server_yupi_lan4`
  - `_vpn_wg_work_br_lan`

---

## IP Rule Priorities

```
<table_id> x10
```

- **Format:** Table ID multiplied by 10
- **Example:** Table ID 100 -> priority 1000
- **Reasoning:** Leaves gaps for manual rules (1001, 1002, etc.)

---

## Firewall Zones

```
wg_<interface>
```

- **Format:** Same as interface name
- **Examples:** `wg_home`, `wg_server_yupi`
- **Note:** Zones automatically created when routing is enabled

---

## State Files

```
/etc/wg-autoconf/state/<interface>.state
```

- **Format:** Interface name + `.state` extension
- **Content:** JSON-like key-value pairs
- **Examples:**
  - `wg_home.state`
  - `wg_server_yupi.state`

---

## Backup Files

```
wg-autoconf_<interface>_<timestamp>.backup
```

- **Format:** `wg-autoconf_` + interface + timestamp + `.backup`
- **Timestamp:** YYYYMMDD_HHMMSS
- **Example:** `wg-autoconf_wg_home_20250222_143015.backup`

---



---

## 5.   Routing System (Policy-Based)

### Why Policy-Based Routing?

Not full tunnel. Selective routing per LAN.  

In fact, each tunnel created is not even routed directly to the OpenWRT LAN bridge itself.

Once you have an active interface, its operation can be tested:

```bash
 $ traceroute -i wg_myconf openwrt.org

 $ curl --interface wg_myconf ifconfig.me

 $ ping -I wg_mytest 1.1.1.1
 
 $ wg-autoconf status
```


From there, you can 'assign' an exit to an interface directly. It is preferef to work with a unified name for everything (wg-autoconf handles the rest), only using an INTERFACE name (not device name) will work.

You can route `wg1` to `lan4`, unroute it and route it again to `lan2`, route another different WG interface `wg3` to `lan4` and also to lan1... You can move routes dynamically in a simple way."

```
Without routing:
  ┌─────┐         ┌──────────────┐         ┌──────────┐
  │LAN3 │ ────→   │ Default WAN  │ ────→   │ Internet │
  └─────┘         │192.168.1.1   │         └──────────┘
                  └──────────────┘

With wg-autoconf routes set wg0 lan3:
  ┌─────┐         ┌──────────────┐         ┌──────────┐
  │LAN3 │ ────→   │ wg0 (VPN GW) │ ────→   │ VPN      │
  └─────┘         │   10.2.0.2   │         │ Server   │
                  └──────────────┘         └──────────┘
  
  ┌─────┐         ┌──────────────┐         ┌──────────┐
  │LAN1 │ ────→   │ Default WAN  │ ────→   │ Internet │
  └─────┘         │  192.168.1.1 │         └──────────┘
                  └──────────────┘ 
```

  
### 5.  Dynamic Routing Table ID Allocation

**Challenge:** Multiple WG interfaces need unique routing tables. Table IDs in `/etc/iproute2/rt_tables` must be numeric and unique.  

**Solution:** Used table ID * 10 for IP rule priorities. Prevents collisions and leaves room for return rules (prio+3).  

```bash
# Example: 3 interfaces with tables 150, 151, 200

Table 150 (first LAN VPN):

  Priority 1500: from 192.168.3.0/24 to 192.168.3.0/24 lookup main
  Priority 1501: from 192.168.3.0/24 lookup _vpn_wg_myconfig_port3
  Priority 1503: from all to 192.168.3.0/24 lookup _vpn_wg_myconfig_port3

Table 151 (second LAN VPN):

  Priority 1510: from 192.168.4.0/24 to 192.168.4.0/24 lookup main
  Priority 1511: from 192.168.4.0/24 lookup _vpn_wg_myconfig_port4
  Priority 1513: from all to 192.168.4.0/24 lookup _vpn_wg_myconfig_port4

Table 200 (client-side routing for multiple servers):

  Priority 2000: from 10.2.0.2 lookup wg_myconfig
  Priority 2001: from all to 10.2.0.2 lookup wg_myconfig
```


### IP Rules Priority System

Each LAN route uses 3 rules:

1. **Local traffic** (priority = base): `from SUBNET to SUBNET lookup main`
   - Allows devices in subnet to communicate directly
   
2. **Outbound traffic** (priority = base+1): `from SUBNET lookup TABLE`
   - Routes traffic from LAN through VPN
   
3. **Return traffic** (prioroty = base+3): `from all to SUBNET lookup TABLE`
   - Ensures responses go back through same VPN



### Routing Table Persistence

```bash
# Tables stored in /etc/iproute2/rt_tables:
150 _vpn_wg_myconfig_lan3
151 _vpn_wg_myconfig_lan4
200 wg_myconfig
201 wg_othervpn
```

**Added on:** `routes set` command
**Removed on:** `routes unset` or `remove` commands
**Cleanup on:** Boot cleanup service (if enabled)

---

## 6.   WireGuard Server Support

### Architecture

Supports multiple independent WireGuard servers, each with multiple clients.

```
wg_server_myserver (10.99.0.0/24)
  |
  ├─→ client1 (10.99.0.2)
  ├─→ client2 (10.99.0.3)
  └─→ client3 (10.99.0.4)

wg_server_anotherServer (10.98.0.0/24)
  |
  ├─→ emp_user1 (10.98.0.2)
  └─→ emp_user2 (10.98.0.3)
```

---

### State Storage

Server metadata in state file:

```
SERVER_1_ID=1
SERVER_1_NAME=myserver
SERVER_1_SUBNET=10.99.0.0/24
SERVER_1_DNS=10.99.0.1
SERVER_1_PRIVKEY=<privkey>
SERVER_1_PUBKEY=<pubkey>
SERVER_1_LISTEN_PORT=51820
SERVER_1_ENDPOINT=develhost.mooo.com
SERVER_1_NEXT_IP=10.99.0.4
SERVER_1_USER_COUNT=3

SERVER_1_USER_1_NAME=client1
SERVER_1_USER_1_IP=10.99.0.2
SERVER_1_USER_1_PUBKEY=<pubkey>
SERVER_1_USER_1_PRIVKEY=<privkey>
SERVER_1_USER_1_CREATED=1708457600
SERVER_1_USER_1_LAST_HS=1708457650
SERVER_1_USER_1_BYTES_RX=102400
SERVER_1_USER_1_BYTES_TX=51200
```

### Workflow

```bash
# 1. CREATE SERVER (interactive)
wg-autoconf server create
    # Asks: name, subnet, DNS, port, endpoint
    # Generates: server keypair + state entries
    # Creates: UCI interface + firewall zone

# 2. ADD USER
wg-autoconf server add myserver client1
    # Generates: client keypair
    # Assigns: next available IP (10.99.0.2)
    # Adds peer to WireGuard (via wg set)
    # Generates: .conf file in /usr/libexec/wg-autoconf/configs/myserver/client1.conf

# 3. DISTRIBUTE CONFIG
    # User imports .conf to WireGuard app on device
    # Device connects → handshake with server

# 4. REVOKE USER (with confirmation)
wg-autoconf server revoke myserver client1
    # Removes peer from WireGuard
    # Deletes .conf file
    # Updates state

# 5. REMOVE SERVER (auto-revokes all users)
wg-autoconf server remove myserver
    # Revokes all users
    # Removes UCI interface + firewall zone
    # Cleans state entirely
```

### Server Manager Design Decisions

* **Single WireGuard per Server:** Each server is isolated, not combined into one interface:
    - Independent firewall zones
    - Ability to have different listen ports
    - Cleaner address space management
    - Easier debugging per server

* **Auto IP Assignment:** NEXT_IP tracked in state, auto-incremented.  
    - No manual IP coordination needed
    - Prevents IP collisions
    - Simple UX-UI (user just picks a name)

* **Stored Private Keys:** Both server and client privkeys kept in state file:
    - Can regenerate .conf files if lost
    - Client reconfiguration without re-running setup
    - Export configs later

* **Endpoint in State:** Server endpoint (hostname/IP) stored separately:
    - Can be different from server name
    - Supports DNS names (e.g., develhost.mooo.com)
    - Clients get correct endpoint even if renamed

---

## 7.   Firewall Integration

### Zone Creation

```bash
# When: routes set wg0 lan3
# Creates in /etc/config/firewall:

config zone 'zone_server_myserver'
    option name 'wg_server_myserver'
    option network 'wg_server_myserver'
    option input 'ACCEPT'
    option output 'ACCEPT'
    option forward 'ACCEPT'
    option masq '1'

config rule 'allow_wg_server_myserver'
    option name 'Allow-WireGuard-myserver'
    option src 'wan'
    option dest_port '51820'
    option proto 'udp'
    option target 'ACCEPT'

config forwarding 'fwd_wg_server_myserver_to_wan'
    option src 'wg_server_myserver'
    option dest 'wan'
```

### Why Bidirectional Forwarding?

```bash
config forwarding
    option src 'lan3'
    option dest 'wg_myconfig'

config forwarding
    option src 'wg_myconfig'
    option dest 'lan3'
```

1.  **First rule:** Allows packets from LAN3 to enter VPN (outbound)
2.   **Second rule:** Allows responses from VPN back to LAN3 (inbound)

Both required for bidirectional communication, even in a "defaults INPUT/FORWARD DROP/REJECT" scenario.


### NFTables Integration

wg-autoconf uses OpenWrt's native nftables include system for all custom firewall rules. Instead of adding rules directly with `nft add rule` (which would be lost on firewall reload), all the post-routing needed rules are written as persistent files in `/usr/share/nftables.d/`.

**Rule Locations:**

- **SNAT/Masquerade rules:** `/usr/share/nftables.d/chain-post/srcnat/95-vpn-<wg_iface>-<lan_iface>.nft`
- **Forwarding rules:** `/usr/share/nftables.d/chain-post/forward/95-vpn-<wg_iface>-<lan_iface>.nft`
- **Base accept rules:** `/usr/share/nftables.d/chain-post/input/90-wg-<wg_iface>.nft`

**How It Works:**

1. When `routes set` is called, wg-autoconf creates a file with the masquerade rule
2. When firewall reloads, `fw4` automatically includes all `.nft` files from these directories
3. Rules persist across reboots and firewall reloads
4. When `routes unset` is called, the file is deleted

**Example generated file:**
```nft
meta oifname "wg_us-vpn" masquerade
```

---

## 8.   Atomic Operations & Backups

### Why Atomic?

Prevents corrupted configs if operation fails mid-way (e.g., power loss, user interrupt).

```
Without atomic ops:

  wg-autoconf up wg0
    > Create interface
    > Set private key  <---- POWER LOSS HERE
    
        > [Never reaches] Set IP address
        > [Never reaches] Activate
  
  Result: Broken state, partial config in /etc/config/

With atomic ops:
  
  wg-autoconf up wg0
    > Create temp /etc/config/network.tmp
    > Read current /etc/config/network
    > Modify in memory
    > Write all at once to .tmp
    < Verify .tmp not empty
    > Atomic rename .tmp --> network (single OS call)
    > If any step fails, .tmp deleted, original untouched
```

### Tagging in-use blocks for /etc/config/ files

Each modification wrapped in comments:

```ini
# wg-autoconf network start id 1
config interface 'wg_myconfig'
    option proto 'wireguard'
    ...
    option private_key 'eMD4...'
# wg-autoconf network end id 1
```

- Surgical removal (only tagged blocks deleted)
- Multiple simultaneous setups (unique IDs per config)
- Manual edits survive cleanup
- Emergency recovery: `grep "wg-autoconf" /etc/config/*`



### Backup Files

```bash
# Before first modification to /etc/config/network:
/etc/config/network.BACKUP_PRE_WIREGUARD_ID_1

# Before first modification to /etc/config/firewall:
/etc/config/firewall.BACKUP_PRE_WIREGUARD_ID_1
```

**Naming:** `<config>.BACKUP_PRE_WIREGUARD_ID_<interface_id>`

**Content:** Original file + markers:

```
#  BACKUP START TAG wg-autoconf backup id X
original file contents...
#  BACKUP END wg-autoconf backup id X

# CHECKSUM: <sha256sum> value for THE CONTENT (not the file)
```

**Checksum:** Validates on restore. Notice the user if mistmatches occur. Retain validation-failed configs for evaluation (to avoid the disaster).

```bash
# To restore:
wg-autoconf backups restore
# → Finds latest BACKUP_PRE_WIREGUARD files
# → Removes markers
# → Restores to original location
```


---

## 9.   NFTables Integration

### The Challenge

OpenWrt uses NFTables for firewall. When you reload firewall config, it:
    1. Reads all `/etc/config/firewall` rules
    2. Generates new nftables ruleset
    3. **Clears all existing nftables chains**
    4. Applies new ruleset

Problem: Custom rules added by wg-autoconf are lost.

### The Previous Steps (before edging nft.d files as the best solution)
As explained before, changes made on commit "1.0.0-r1 Session 13.:
- Re-apply nftables rules after firewall reload

```bash
# In set_lan_routes() and fw_reload_with_wg_detection():

# 13. WORKAROUND NFTABLES
for wg_iface_loop in $(grep "^[0-9]\+ wg" /etc/iproute2/rt_tables | awk '{print $2}'); do
    
    # 13.1  Re-add accept rules
    nft add rule inet fw4 "accept_from_$wg_iface_loop" counter accept 2>/dev/null
    nft add rule inet fw4 "accept_to_$wg_iface_loop" counter accept 2>/dev/null
    
    # 13.2 Re-add srcnat jump (critical for masquerading)
    nft add rule inet fw4 srcnat oifname "$wg_iface_loop" jump "srcnat_${wg_iface_loop}" 2>/dev/null
done
```


### Multi-Interface Detection

**Critical for multiple WG clients + servers:**

```bash
# Detect from rt_tables (server interfaces)
grep "^[0-9]\+ wg" /etc/iproute2/rt_tables

# Detect from ip link (client interfaces)
ip link show | grep '^[0-9]*:.*wg'

# Combine both
all_wg_interfaces=$(...)
```

**Why both sources?** 
- `rt_tables` has all server tables
- `ip link` shows only active interfaces
- Combining ensures no missed interfaces


---

## 10.  Boot Cleanup Service

### Purpose

Remove stale WireGuard configs on boot. Prevents orphaned interfaces + broken routing.

### Behavior

On each boot (BEFORE network startup finishes):
    1. Read state file
    2. Find all WireGuard interfaces
    3. Remove UCI configs (network + firewall)
    4. Flush routing tables
    5. Remove rt_tables entries
    6. Reset state file

### Why Aggressive?

Even though it can be executed at any time with the nuke command, even that name poses no danger whatsoever.

By design, it is prefered to leave a clean system, with no garbage or remnants of old or unwanted configurations, **to avoid disaster (having to perform a hard brick).**   
In my experience, also after consulting with other colleagues, many applications that modify network parameters tend to leave systems unstable when errors occur, or fail to clean up their own shit.

Also, after power loss or crash:

    - Partial configs might exist
    - Routing tables might have stale rules
    - Firewall rules might be orphaned
    - DNS might be broken

For all possible disaster scenarios, it's safer to clear everything and let users manually recreate than to leave a broken state.

### Disabling

```bash
/etc/init.d/wg-autoconf_boot_cleanup disable
```

**Trade-off:** You become responsible for manual cleanup after crashes.

### Service Location

```
/etc/init.d/wg-autoconf_boot_cleanup
```

Symlinked from init.d system (when package installed).

---

## 11.  Multi-Interface Handling

### Multiple Clients + Multiple Routes

**Problem:** When routing two clients to different LANs, firewall reload would clear NFTables rules for first client, causing connectivity loss.  

**Root Cause:** `set_lan_routes()` only re-added nftables rules for the **current** interface, not all WG interfaces.  

**Solution:** After firewall reload, iterate **ALL wg* interfaces** (not just current) and re-add rules:  

```bash
# OLD (broken):
nft add rule inet fw4 srcnat oifname "$wg_iface" jump "srcnat_$wg_iface"

# NEW (fixed):
for wg_iface_loop in $(grep "^[0-9]\+ wg" /etc/iproute2/rt_tables | awk '{print $2}'); do
    
    nft add rule inet fw4 srcnat oifname "$wg_iface_loop" jump "srcnat_${wg_iface_loop}"

done
```

### Multi-Server Support

Each server is independent:

```bash
wg-autoconf server create  A         
wg-autoconf server create  B       
wg-autoconf up wg_server_A
wg-autoconf up wg_server_B

# Now both servers listening on different ports
# Clients can connect to either
```

No port conflicts because each server has unique LISTEN_PORT.

---


## 12.  Design Decisions & DEBUG! Notes

#### DEBUG! rel5 - CLI Colours

```bash
# DEBUG! rel5
# TODO: Test colours/escapes in older devices, different shells, etc
# REASON: Different ASNI colour interpretation for older shells
# POSSIBLE FIX?: Remove colours ... or try tput in openwrt or ... idk
```

**Decision:** Keep colours but make them configurable.

**Why?** ANSI codes can behave differently across busybox versions. Solution: `wg-autoconf settings set colours 0` to disable.

#### DEBUG! r7 - Ternaries & POSIX Compliance

```bash
# DEBUG! r7
# TODO: Unified syntax: Ternaries, Arrays destructuring, echo and read -p VS printf
# POSIX --> Test on many different Busybox-Ash shells
```

**Decision:** Used `&&` / `||` for ternaries instead of `$(... && echo ...)`.

```bash
# BAD (not POSIX)
result=$([ -z "$x" ] && echo "empty" || echo "full")

# GOOD (POSIX, works everywhere)
[ -z "$x" ] && result="empty" || result="full"
```

#### DEBUG! r7 - Atomic Operations

```bash
# Use atomic file handling to prevent corruption
# Each operation: read entire state → modify → write atomically
# Prevents partial writes if operation interrupted
```

**Trade-off:** Slightly slower (reads full file each time) but prevents corruption.

#### DEBUG! r7 - Boot Cleanup Lifecycle

```bash
# Boot cleanup is AGGRESSIVE
# Removes ALL WireGuard configs on each boot
# Trade-off: Prevents orphaned interfaces but requires manual recreation
```

**Why aggressive?** After crash, better safe than sorry.

#### DEBUG! r5 - Tagging Strategy

```bash
# Tagged config blocks for surgical removal
# Each wg-autoconf modification wrapped in:
# # wg-autoconf <type> start id <N>
# ... actual config ...
# # wg-autoconf <type> end id <N>
```

**Why?** Allows removal of specific configs without touching manual edits. Essential for multi-interface support.

---

## Performance Considerations

### Slow Operations

1. **Boot cleanup** (~2-5 seconds)
   - Iterates all interfaces
   - Removes configs
   - Flushes tables
   
2. **Firewall reload** (~1-2 seconds)
   - Rebuilds all NFTables chains
   - Can't be parallelised (system limitation)

3. **Setting up multiple routes** (seconds per route)
   - Each `routes set` causes firewall reload
   - Multiple `routes set` calls = multiple reloads

   
### **Batch operations**

```bash
# Slow (3 firewall reloads):
wg-autoconf routes set wg0 lan3
wg-autoconf routes set wg0 lan4
wg-autoconf routes set wg1 lan5

# Can't really parallelise firewall, but at least do in single pass internally
# (Current implementation reloads per route, will be optimised)
```

#### **Disable debug if not needed:**  

Debug functions were created for evaluations, being so much verbosed.  
Comes disabled by default.  
  
```bash
wg-autoconf debug off
# Debug logging adds overhead
```

#### **Disable boot cleanup if not needed:**

```bash
/etc/init.d/wg-autoconf_boot_cleanup disable
# Only if you won't be crashing and know/have manual cleanup
```

---

## 13.  Some Troubleshooting

### State File Corruption

**Symptoms:**  
- Commands fail with "state file not found" errors
- State reads don't work

**Diagnosis:**  
   
```bash
# Check file permissions
ls -la /usr/libexec/wg-autoconf/states

# Check contents
head -20 /usr/libexec/wg-autoconf/states

# Validate key-value format
grep "^[A-Z_]*=" /usr/libexec/wg-autoconf/states | wc -l
```
 
**Fix:**  

```bash
# Reset state
rm -f /usr/libexec/wg-autoconf/states
wg-autoconf status  # Recreates empty state
```

### Routing Table Overflow

**Symptoms:**  
- "Cannot allocate routing table ID" errors
- Can't add more VPN routes

**Diagnosis:**  

```bash
cat /etc/iproute2/rt_tables | wc -l
# System limit is usually 256 tables

# Check for duplicates
sort /etc/iproute2/rt_tables | uniq -d
```

**Fix:**  

```bash
# Remove unused tables manually
# Edit /etc/iproute2/rt_tables
# Delete lines for deleted interfaces

# Or nuclear option:
wg-autoconf nuke
# (Removes everything, clean slate)
```

### IP Rule Orphaning

**Symptoms:**  
- Firewall rules exist in nftables but `ip rule show` empty
- OR `ip rule show` has orphaned rules

**Diagnosis:**  

```bash
# View all rules
ip rule show

# Find orphaned rules (reference non-existent table)
for prio in $(ip rule show | grep -o '^[0-9]*:' | cut -d: -f1); do
    
    table=$(ip rule show from all prio "$prio" | grep -o 'lookup [^ ]*' | awk '{print $2}')
    
    [ -z "$table" ] && echo "Orphaned rule prio $prio"

    done
```

**Fix:**  

```bash
# Delete orphaned rules manually
ip rule del prio 1500
ip rule del prio 1501

# Or clean all (CAREFUL):
ip rule flush
# (Resets to system defaults, need to re-setup routes)
```

### NFTables Rules Missing

**Symptoms:**  
- Traffic stops after firewall reload
- `nft list ruleset` shows no wg-autoconf rules

**Diagnosis:**  
```bash
# Check if include files exist
ls -la /etc/nftables.d/chain-post/srcnat/*.nft
ls -la /etc/nftables.d/chain-post/forward/*.nft

# Check file contents
cat /etc/nftables.d/chain-post/srcnat/95-*.nft
```

**Fix:**  

```bash
# Recreate rules by re-running routes set
wg-autoconf routes set wg_myconfig lan3

# Or manually check if directories exist
mkdir -p /usr/share/nftables.d/chain-post/srcnat
mkdir -p /usr/share/nftables.d/chain-post/forward


# If files are missing but should exist, re-run setup
wg-autoconf remove wg_myconfig
wg-autoconf setup myconfig
wg-autoconf up wg_myconfig
wg-autoconf routes set wg_myconfig lan3
```

### DNS Issues on VPN  

**Symptoms:**  
- IP connectivity works but DNS queries fail
- `ping 8.8.8.8` works but `ping google.com` fails

**Diagnosis:**  

```bash
# Check if DNS set on interface
uci show network.wg_myconfig.dns

# Check if dnsmasq sees it
uci show dhcp | grep wg_myconfig

# Test with specific DNS
dig @1.1.1.1 google.com

# Check system resolver
cat /etc/resolv.conf
```

**Fix:**  

1. **Check your DNS provider if works!**.  
Even using a private/paid Wireguard provider, in many cases, the DNS route they provide NEVER WORKS (e.g. ProtonVPN assigns the tunnel gateway 10.x.0.1 IP as DNS ... almost never works, and must be changed!).

2. Set DNS explicitly
```bash
uci set network.wg_myconfig.dns='1.1.1.1 1.1.1.1'
uci commit network

ifup wg_myconfig

# Or globally in settings
wg-autoconf settings set dns "1.1.1.1, 1.0.0.1"
```

>**Be aware of when it MUST be COMMA-SEPARATED (.conf files) or space-separated (UCI commands).**
---

## Development Notes

TODO/CHANGELOG are available to proceed with a review

### States Diagram

![](./DOCS/img/DIAGRAMS/STATE_MACHINE.png)


### Code Organisation

```
wg-autoconf.source
│
├─── GLOBALS & CONFIGURATION
│    ├── VERSION & PATHS
│    │    ├── version
│    │    ├── global_config_file
│    │    ├── user_config_dir
│    │    ├── state_dir
│    │    ├── backup_dir
│    │    └── debug_log
│    │
│    └── DEFAULT_* VARIABLES
│         ├── DEFAULT_LAN_INTERFACE
│         ├── DEFAULT_PORT
│         ├── DEFAULT_DNS
│         ├── DEFAULT_MTU
│         ├── DEFAULT_PERSISTENT_KEEPALIVE
│         ├── DEFAULT_TABLE_PREFIX
│         ├── DEFAULT_RULE_PRIORITY_MULTIPLIER
│         └── DEFAULT_CONFIG_DIR
│
├─── USER MANAGEMENT
│    ├── create_default_user_settings()
│    ├── load_user_settings()
│    ├── save_user_settings()
│    ├── validate_user_settings()
│    └── reset_user_settings()
│
├─── UX/UI SYSTEM
│    ├── COLOR VARIABLES (set based on settings)
│    │    ├── COLOR_INFO
│    │    ├── COLOR_SUCCESS
│    │    ├── COLOR_WARNING
│    │    ├── COLOR_ERROR
│    │    ├── COLOR_DEBUG
│    │    └── COLOR_RESET
│    │
│    └── FORMATTING FUNCTIONS
│         ├── ui_lines()
│         ├── log()
│         ├── success()
│         ├── warning()
│         ├── error()
│         ├── debug()
│         ├── die()
│         ├── confirm_action()
│         └── show_progress()
│
├─── SYSTEM VALIDATION
│    ├── check_bins()
│    │    ├── check_wg()
│    │    ├── check_ip()
│    │    ├── check_nft()
│    │    ├── check_uci()
│    │    ├── check_jq()
│    │    └── check_iptables()
│    │
│    ├── check_kernel_modules()
│    ├── check_openwrt_version()
│    ├── check_dependencies()
│    └── verify_system_compatibility()
│
├─── HELPER FUNCTIONS
│    ├── CONFIG PARSING
│    │    ├── parse_endpoint()
│    │    ├── allowed_ips_to_uci()
│    │    ├── parse_wg_config()
│    │    ├── validate_interface_name()
│    │    ├── validate_ip_address()
│    │    ├── validate_cidr()
│    │    ├── validate_private_key()
│    │    ├── validate_public_key()
│    │    └── validate_preshared_key()
│    │
│    ├── NETWORK HELPERS
│    │    ├── netmask_to_cidr()
│    │    ├── cidr_to_netmask()
│    │    ├── ip_to_network()
│    │    ├── ip_in_subnet()
│    │    ├── get_interface_ip()
│    │    ├── get_interface_netmask()
│    │    ├── get_default_gateway()
│    │    ├── get_available_subnet()
│    │    ├── check_ip_collision()
│    │    └── find_free_subnet()
│    │
│    ├── ATOMIC OPERATIONS
│    │    ├── atomic_open()
│    │    ├── atomic_write()
│    │    ├── atomic_close()
│    │    ├── create_lock()
│    │    ├── release_lock()
│    │    └── wait_for_lock()
│    │
│    └── DNS MANAGEMENT
│         ├── dns_leakage_noticer()
│         ├── detect_dns_leak()
│         ├── fix_dns_leak()
│         ├── set_vpn_dns()
│         ├── restore_system_dns()
│         └── test_dns_resolution()
│
├─── UCI MANAGEMENT
│    ├── uci_add_network()
│    ├── uci_add_network_peer()
│    ├── uci_delete_interface()
│    ├── uci_delete_peer()
│    ├── uci_commit_safe()
│    ├── uci_rollback_on_failure()
│    ├── uci_get_interface_list()
│    ├── uci_get_peer_list()
│    ├── uci_interface_exists()
│    ├── uci_backup_config()
│    └── uci_restore_config()
│
├─── ROUTING MANAGEMENT
│    ├── add_routing_table()
│    ├── remove_routing_table()
│    ├── add_ip_rule()
│    ├── remove_ip_rule()
│    ├── list_ip_rules()
│    ├── add_default_route()
│    ├── remove_default_route()
│    ├── flush_routing_cache()
│    ├── get_table_id()
│    ├── table_name_to_id()
│    ├── id_to_table_name()
│    └── validate_routing_table()
│
├─── FIREWALL MANAGEMENT
│    ├── fw_create_zone()
│    ├── fw_delete_zone()
│    ├── fw_add_forwarding()
│    ├── fw_remove_forwarding()
│    ├── fw_add_rule()
│    ├── fw_delete_rule()
│    ├── fw_reload()
│    ├── fw_backup()
│    ├── fw_restore()
│    ├── fw_add_nftables_workaround()
│    └── fw_verify_rules()
│
├─── STATE MACHINE
│    ├── state_init()
│    ├── state_read()
│    ├── state_write()
│    ├── state_delete()
│    ├── state_get()
│    ├── state_set()
│    ├── state_unset()
│    ├── state_list_all()
│    ├── validate_state()
│    ├── migrate_state()
│    └── state_cleanup_stale()
│
├─── BACKUP SYSTEM
│    ├── backup_create()
│    │    ├── backup_network_config()
│    │    ├── backup_firewall_config()
│    │    ├── backup_state_files()
│    │    └── backup_wireguard_keys()
│    │
│    ├── backup_restore()
│    │    ├── restore_network_config()
│    │    ├── restore_firewall_config()
│    │    ├── restore_state_files()
│    │    └── restore_wireguard_keys()
│    │
│    ├── backup_list()
│    ├── backup_delete()
│    ├── backup_verify()
│    ├── backup_auto_prune()
│    └── backup_auto_backup()
│
├─── CORE OPERATIONS
│    ├── SETUP/REMOVE
│    │    ├── setup_wireguard()
│    │    │    ├── parse_config_file()
│    │    │    ├── validate_config()
│    │    │    ├── check_collisions()
│    │    │    ├── uci_create_interface()
│    │    │    ├── uci_add_peers()
│    │    │    └── state_write_setup()
│    │    │
│    │    ├── remove_wireguard()
│    │    │    ├── verify_interface_exists()
│    │    │    ├── prompt_confirmation()
│    │    │    ├── uci_delete_all()
│    │    │    ├── state_delete()
│    │    │    └── cleanup_residual_files()
│    │    │
│    │    ├── deactivate_interface()
│    │    │    ├── check_if_active()
│    │    │    ├── ip_link_down()
│    │    │    ├── ip_link_delete()
│    │    │    └── state_set_inactive()
│    │    │
│    │    └── activate_interface()
│    │         ├── read_state()
│    │         ├── ip_link_add()
│    │         ├── wg_set_private_key()
│    │         ├── ip_addr_add()
│    │         ├── ip_link_up()
│    │         ├── wait_for_operational()
│    │         └── state_set_active()
│    │
│    ├── ROUTES
│    │    ├── set_lan_routes()
│    │    │    ├── validate_interface_active()
│    │    │    ├── create_routing_table()
│    │    │    ├── add_ip_rules()
│    │    │    ├── add_routes()
│    │    │    ├── create_firewall_zone()
│    │    │    ├── fw_reload_with_workaround()
│    │    │    └── state_set_routed()
│    │    │
│    │    └── unset_lan_routes()
│    │         ├── validate_interface_routed()
│    │         ├── remove_ip_rules()
│    │         ├── remove_routing_table()
│    │         ├── delete_firewall_zone()
│    │         ├── fw_reload()
│    │         └── state_set_unrouted()
│    │
│    └── SERVER FUNCTIONS
│         ├── server_create()
│         │    ├── generate_server_keys()
│         │    ├── create_server_config()
│         │    ├── setup_server_interface()
│         │    └── add_default_server_peer()
│         │
│         ├── server_add_user()
│         │    ├── generate_client_keys()
│         │    ├── create_client_config()
│         │    ├── add_peer_to_server()
│         │    └── show_qr_code()
│         │
│         ├── server_remove_user()
│         │    ├── remove_peer_from_server()
│         │    └── delete_client_config()
│         │
│         ├── server_list_users()
│         ├── server_show_config()
│         └── server_qr()
│
├─── TESTING & DIAGNOSTICS
│    ├── test_connection()
│    │    ├── ping_peer()
│    │    ├── trace_route()
│    │    └── bandwidth_test()
│    │
│    ├── status()
│    │    ├── show_interface_status()
│    │    ├── show_peer_status()
│    │    ├── show_routing_status()
│    │    ├── show_firewall_status()
│    │    └── show_dns_status()
│    │
│    ├── diagnostics()
│    │    ├── run_connectivity_tests()
│    │    ├── check_dns_leaks()
│    │    ├── check_mtu_issues()
│    │    ├── check_handshake_status()
│    │    └── generate_report()
│    │
│    └── monitor()
│         ├── watch_interface()
│         ├── watch_handshakes()
│         └── watch_traffic()
│
├─── CLEANUP
│    ├── cleanup()
│    │    ├── remove_temp_files()
│    │    ├── release_locks()
│    │    ├── restore_traps()
│    │    └── final_log()
│    │
│    ├── cleanup_stale_interfaces()
│    ├── cleanup_stale_routes()
│    ├── cleanup_stale_firewall_rules()
│    ├── cleanup_stale_state_files()
│    └── emergency_cleanup()
│
├─── DEBUG SYSTEM
│    ├── debug_write()
│    ├── debug_handler()
│    ├── debug_set_level()
│    ├── debug_enable()
│    ├── debug_disable()
│    ├── debug_dump_state()
│    ├── debug_dump_config()
│    └── debug_trace()
│
├─── UPGRADE SYSTEM
│    ├── upgrade_check()
│    │    ├── check_version()
│    │    ├── fetch_latest_version()
│    │    └── compare_versions()
│    │
│    ├── upgrade_backup()
│    ├── upgrade_download()
│    ├── upgrade_install()
│    ├── upgrade_rollback()
│    └── upgrade_migrate_config()
│
├─── MENUS & HELP
│    ├── show_help()
│    ├── show_usage()
│    ├── show_version()
│    ├── interactive_menu()
│    ├── show_quickstart()
│    ├── show_examples()
│    ├── show_troubleshooting()
│    └── show_credits()
│
└─── MAIN DISPATCHER
     ├── parse_global_options()
     ├── validate_command()
     ├── case statement on $cmd
     │    ├── setup)
     │    ├── up)
     │    ├── down)
     │    ├── nuke)
     │    ├── routes)
     │    │    ├── routes_set)
     │    │    └── routes_clear)
     │    ├── status)
     │    ├── test)
     │    ├── server)
     │    │    ├── server_create)
     │    │    ├── server_add)
     │    │    ├── server_remove)
     │    │    └── server_list)
     │    ├── backup)
     │    │    ├── backup_create)
     │    │    ├── backup_restore)
     │    │    └── backup_list)
     │    ├── debug)
     │    ├── upgrade)
     │    └── help)
     │
     └── execute_command()
```

### Data Model in use

![](./DOCS/img/DIAGRAMS/DB_MODEL.png)

---

### Testing Strategy

Manual testing on:  
- OpenWrt 25.12+ with APK enabled
- Multiple device architectures (x86_64, aarch64, armv7)
- Different busybox versions (1.35.0+)


### Known Limitations

1. **Single Peer per WG Interface:** WireGuard limitation, not tool
2. **No Key Rotation:** Manual updates required
3. **No Web UI:** CLI only. **Pending a luci-proto-x module**
4. **APK Only:** No Opkg version

---

## Issues

Found issues? https://github.com/alexandrglm/openwrt_wg-autoconf/issues

---

## License

MIT

---

Made for OpenWrt with 🥰
