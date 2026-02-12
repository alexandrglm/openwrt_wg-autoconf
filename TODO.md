### 0.0.1-r7 -> 1.0.0 r1

- [ ] - Migrate dependences (**and syntax**,  lot of breaking changes) to the basis, 'kmod-wireguard', and 'ip-tiny' for the ip implemntation; as a consecuence, implement new state flag for "available bins" for some "advanced" options (`wireguard-tools` for status and a plausible WG Server feature; `ip-full` for IPv6 routing support) and also add logic to check bins/flags on each "advanced" options (`if [ ! "$IS_IP-FULL" = "0" ] && ADVANCED OPERATION LOGIc `). 
Or retain `wireguard-tools`as the root dependence. 
- [ ] - Add wg servers setup/etc.. support. 

- [X] - Fix APKBUILD/Makefile dependencies to 'kmod-wireguard'
- [X] - Fix CLI-UI:   `ui_lines`, CLI colours, for a better CLI - UI
- [X] - `list_configs`, `list_configs_full`: Improve usability
- [X] - `nuke_all`, nuke, clean funcs:  Refactor in one master-func, which dispatches "nuke mode", or "clean <iface> mode" straightforward; also emergency cleanups hardcoded logic
- [X] - Review and fix every `verbose`, `debug_write`, `debug`, callbacks. Also clear all `debug_validate` checks on cleaning pre-build stage
- [X] - Unified syntax:  Ternaries, Arrays destructuring, `echo` and `read -p` VS `printf`, POSIX --> Test on many different Busybox-Ash shells
- [X] - Add IPv6 `--advanced` support
- [X] - Add DNS resolv pre-checks for auto and `--advanced` mode
- [X] - Add User Settings (enable/disable CLI colours, change DNS/AllowedIPs defaults, ...) CLI menu support
