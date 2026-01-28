# rel3 debugging
debug_log() {
    
    local msg="$1"
    local log_dir="/etc/libexec/wg-autoconf/logs"
    
    if [ ! -d "$log_dir" ]; then
    
        mkdir -p "$log_dir" 2>/dev/null || {

            log_dir="/tmp/wg-autoconf-logs"
            mkdir -p "$log_dir" 2>/dev/null || return 1
        }
    fi    
    local log_file="$log_dir/wg-autoconf.log"
    
    echo "[$(date +%Y-%m-%d\ %H:%M:%S)] $msg" >> "$log_file" 2>/dev/null
}
