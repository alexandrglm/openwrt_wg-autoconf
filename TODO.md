### 0.0.1-r7 -> 1.0.0 r1
- [X] - Fix CLI-UI:   `ui_lines`, CLI colours, for a better CLI - UI
- [X] - `list_configs`, `list_configs_full`: Improve usability
- [X] - `nuke_all`, nuke, clean funcs:  Refactor in one master-func, which dispatches "nuke mode", or "clean <iface> mode" straightforward; also emergency cleanups hardcoded logic
- [X] - Review and fix every `verbose`, `debug_write`, `debug`, callbacks. Also clear all `debug_validate` checks on cleaning pre-build stage
- [X] - Unified syntax:  Ternaries, Arrays destructuring, `echo` and `read -p` VS `printf`, POSIX --> Test on many different Busybox-Ash shells
- [X] - Add IPv6 `--advanced` support
- [X] - Add DNS resolv pre-checks for auto and `--advanced` mode
