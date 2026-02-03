## From 1.0.0-r6
- [ ] **Downgrade versions to `0.0.1-rX` until the final version for inclusion in the OpenWRT repositories is finalised, with that latter being `1.0.0-r1`**
- [X] - Added a flag-method to replace APK lifecycle methods. Logic to prevent "disasters" is now more robust, depending on the binary itself rather than `apk add`/`apk del`/`apk add --upgrade` actions.
- [X] - Refactored cleanup and "nuke" functions into a single centralised function
- [X] - Refactored the `setup` functions, merging "auto" and `--advanced` modes into one
- [X] - Cleaned up other `debug_*` tags than the enabled debug function itself
- [ ] - `list_configs`, `list_configs_full`: Improve usability
- [ ] - Unified syntax:  Ternaries, Arrays destructuring, `echo` and `read -p` VS `printf`, POSIX/REGEX --> Test on many different Busybox-Ash shells
- [ ] - Add IPv6 `--advanced` support
- [ ] - Add DNS resolv pre-checks for auto and `--advanced` mode
- [ ] - Fix CLI-UI:   `ui_lines`, CLI colours, for a better CLI - UI



### From 1.0.0-r5
- Migrated APK build SDK to `docker:alpine:latest`, improving build reproducibility and isolation.
- Added debugging capabilities via dedicated `debug` commands.
- Fixed issues in APK lifecycle scripts (pre_inst, post_inst, pre_rm hooks).
- Improved backup and restore of `etc/config/...` by adding checksum and tag validation.


### From 1.0.0.-r4
- Fixed `unset_lan_routes()` logic: previous implementation did not correctly clean routing tables assigned in `/etc/iproute2/rt_tables`.
- Improved overall verbosity `--verbose` mode.
- Moved APK lifecycle methods (pre/post install/remove) from `APKBUILD` / `Makefile` into dedicated scripts, simplifying packaging and repository maintenance.
