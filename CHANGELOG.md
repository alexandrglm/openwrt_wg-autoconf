### From 1.0.0-r5
- Migrated APK build SDK to `docker:alpine:latest`, improving build reproducibility and isolation.
- Added debugging capabilities via dedicated `debug` commands.
- Fixed issues in APK lifecycle scripts (pre_inst, post_inst, pre_rm hooks).
- Improved backup and restore of `etc/config/...` by adding checksum and tag validation.


### From 1.0.0.-r4
- Fixed `unset_lan_routes()` logic: previous implementation did not correctly clean routing tables assigned in `/etc/iproute2/rt_tables`.
- Improved overall verbosity `--verbose` mode.
- Moved APK lifecycle methods (pre/post install/remove) from `APKBUILD` / `Makefile` into dedicated scripts, simplifying packaging and repository maintenance.
