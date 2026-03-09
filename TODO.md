### 0.0.1-r7 -> 1.0.0 r1

### PENDING
- [ ] -> Create Wiki. Create Wiki self-hosted/portfolio
- [ ] - Re-implement [`websocket-shell-react`](https://github.com/alexandrglm/websocket-shell-react) to provide a 'live test demo`**


### Session 15 2026-03-09

- [X] - **Refactor NFT subsystem** 
    Migrated all manual NFT rules to proper include files in `/usr/share/nftables.d/` following the fw4 standard. Rules are now persistent and fully integrated

- [X] **Add selective IP leak blocking (WITHOUT REQUIRING a `/etc/init.d/network restart`!!!)**
    System automatically analyses whether the WG peer is IPv4-only, IPv6-only or dual-stack, based on dest interface address + the configured endpoint address for select4ed WG peer:
    - **IPv4-only**: Complete IPv6 blocking on the target interface (input/forward/output chains) plus  `/proc/sys/net/ipv6/conf/$iface/disable_ipv6` and flush.
    - **IPv6-only**: IPv4 blocking in forward chain (**BUT preserving local IPv4 access to the router**)
    - **Dual-stack**: No blocking applied, both protocols traverse the VPN

    As an example, when blocking IPv6 for IPv4-only peers, the system now:
        - Disables IPv6 at kernel level via sysctl
        - Flushes existing IPv6 routes and addresses
        - Uses `ubus` to reload only the affected interface (leaving PPPoE and other connections untouched)
        - **Properly restores IPv6 when routes are removed, including link-local address regeneration and RA announcements**


### Session 14 2026-02-22

- [X] - Proceed with a deeper staging, tests EVERY error case, validation, multiple scenario
- [X] - Test multi-routing behaviour with every possible usecase-scenario
- [X] - Create a full docs/tl;dr
    

### Session 13 2026-02-19~21

- [X] - Add WG (multiple) Servers x Multiple Peer Clients support, fully working.

- [X] - Refactorise `fw4`, `nft`, setup/removal behaviour
- [X] - Fix NFT CHAIN remnants when deactivate_interface when multiple ifaces enabled & routes (CMD_LOG_3...)

- [X] - Fix Multiple WG Clients x Multiple Routing

- [X] - Remove non-mandatory atomic operations, improved performance ((cleanup(), setup_, remove_, activate_ deactivate_))

- [X] - Fix Disable colours setting

---

### Session 12 2026-02-16


- [x] - Add new typo/colour functions:
        + ITALIC, BOLD, UNDERLINE, REVERSE, STRIKE, DIM, BLINK
        + MAGENTA, CYAN, WHITE, GREY
- [X] - Add a more detailed help/docs vía `wg-autoconf docs`

---

#### Session 11 FULL CHANGELOG: 2026-02-12/17

##### Parser & Helpers
- [X] - Update User Settings:
        + Colours CLI ON/OFF
        + Verbosity Always ON/OFF -> Upgrade verbose() to handle modes
        + Default DNS, Allowed IPs, UDP Port

- [X] - Update Debug functions: show, live, tables, chains, nft, state machine
- [X] -  Fixed `get_uci_list()` to properly parse multiple UCI list values
- [X] - Fixed `parse_endpoint()` with full IPv6 [bracket:style:ip]:: support
- [X] - Re-written `process_allowed_ips()` to support-maintain-retain COMMA-SEPARATED / SPACE-SEPARATED as required
- [X] - Created new helpers for setups/removal processes:
     +  `add_routing_table()`
     +  `del_routing_table()`
     +  `add_ip_rule()`
     +  `del_ip_rules_by_table()`
     -  [X] - **PENDING: UPDATE DOCUMENTATION**
     
##### State Machine & System
- [X] - Complete refactor of state management system (`state_safety()`, atomic operations, improved)
- [X] - Added system capabilities detection (`check_bins()` for ip-tiny, ip-full, nftables, iptables, wireguard-tools)
    -  [X] - **PENDING: Use them; expand switch/bypass cases; create new states catergories from checked bin availability**

    
##### Setup/Remove, Up/Down
- [X] - Rewritten `activate_interface()` to manually create/config WireGuard interfaces (**no `ifup` broken usage/dependency**)
- [X] - Fixed IP assignment and local route insertion for tunnel Address values
- [X] - Added proper peer cleanup in `deactivate_interface()`
- [X] - Implemented routing table cleanup (wg* tables) on interface deactivation


##### Routes, Rules, NFT
- [X] - Complete redesign of `set_lan_routes()` with dynamic priorities (table_id x **10)
- [X] - Added NFTables chain creation order (UCI -> Commit -> reload -> Add rules)
- [X] - Added proper return rules for tunnel IPs (to $wg_ip lookup $table_name)
- [X] - Re-implemented multi-interface routing with no conflicts. **PENDING: More staging**


##### Cleanup & Nuke
- [X] - Complete overhaul of `cleanup()` with a proactive orphan detection
- [X] - Added global cleanup for orphaned routing tables (by name+ by ID)
- [X] - Added global cleanup for orphaned IP rules (by priority + by table ID)
- [X] - Added orphaned NFT chains removal
- [X] - Fixed order of operations (`ip rules` fisrt, then `rt_tables`)


  
##### Boot Cleanup & Lifecycles Methods
- [X] - Updated boot cleanup script & pre_deinstall method with complete/updated nuke-like fallback
- [X] - Added orphaned table detection and flushing by ID


##### Bug Fixes
- [X] - Fixed DNS leakage detection for tunnel-range DNS servers
- [X] - Corrected `allowed_ips` format for UCI (multiple list entries)
- [X] - Fixed routing table broken persistence after nuke
- [X] - Resolved IP rule orphanage when tables are deleted (retained by kernel)
- [X] - Fixed many REGEX-POSIX

----


- [ ] - Migrate dependences ~~(**and syntax**,  lot of breaking changes) to the basis, 'kmod-wireguard', and 'ip-tiny' for the ip implemntation; as a consecuence, implement new state flag for "available bins" for some "advanced" options (`wireguard-tools` for status and a plausible WG Server feature; `ip-full` for IPv6 routing support) and also add logic to check bins/flags on each "advanced" options (`if [ ! "$IS_IP-FULL" = "0" ] && ADVANCED OPERATION LOGIc `).~~  
Or retain `wireguard-tools`as the root dependence. **Future viable and realistic options:**
  - 1. Create a custom implementation in C to remove the need for wireguard-tools dependency (it's a brilliant&needed tool, but aiming for "the fewer dependencies, the better")
  - 2. Use the native kmod-wireguard implementation and its calls, as for e.g.
      `wg_set_peer(iface, pubkey, endpoint, allowed_ips, dns, ......)` via netlink:
          1. Open netlink socket
          2. Construct WG_CMD_SET_DEVICE message
          3. Send it to the kernel
          4. Await response



- [X] - Fix APKBUILD/Makefile dependencies to 'kmod-wireguard'
- [X] - Fix CLI-UI:   `ui_lines`, CLI colours, for a better CLI - UI
- [X] - `list_configs`, `list_configs_full`: Improve usability
- [X] - `nuke_all`, nuke, clean funcs:  Refactor in one master-func, which dispatches "nuke mode", or "clean <iface> mode" straightforward; also emergency cleanups hardcoded logic
- [X] - Review and fix every `verbose`, `debug_write`, `debug`, callbacks. Also clear all `debug_validate` checks on cleaning pre-build stage
- [X] - Unified syntax:  Ternaries, Arrays destructuring, `echo` and `read -p` VS `printf`, POSIX --> Test on many different Busybox-Ash shells
- [X] - Add IPv6 `--advanced` support
- [X] - Add DNS resolv pre-checks for auto and `--advanced` mode
- [X] - Add User Settings (enable/disable CLI colours, change DNS/AllowedIPs defaults, ...) CLI menu support
