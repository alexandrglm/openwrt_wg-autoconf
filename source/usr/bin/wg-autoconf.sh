#!/bin/ash
# ###########################################################################################
#
# ██╗    ██╗ ██████╗    ███████╗██╗   ██╗████████╗ ██████╗  ██████╗ ██████╗ ███╗   ██╗███████╗
# ██║    ██║██╔════╝    ██╔══██║██║   ██║╚══██╔══╝██╔═══██╗██╔════╝██╔═══██╗████╗  ██║██╔════╝
# ██║ █╗ ██║██║  ███╗   ███████║██║   ██║   ██║   ██║   ██║██║     ██║   ██║██╔██╗ ██║█████╗
# ██║███╗██║██║   ██║ ═ ██╔══██║██║   ██║   ██║   ██║   ██║██║     ██║   ██║██║╚██╗██║██╔══╝
# ╚███╔███╔╝╚██████╔╝   ██║  ██║╚██████╔╝   ██║   ╚██████╔╝╚██████╗╚██████╔╝██║ ╚████║██║
#  ╚══╝╚══╝  ╚═════╝    ╚═╝  ╚═╝ ╚═════╝    ╚═╝    ╚═════╝  ╚═════╝ ╚═════╝ ╚═╝  ╚═══╝╚═╝
#
# ###########################################################################################
# Maintainer: Alexander Gomez <148530039+alexandrglm@users.noreply.github.com>
# pkgname=wg-autoconf
# pkgver=1.0.0
# pkgrel=3
# pkgdesc="WireGuard Auto-Configuration tool"
# url="https://github.com/alexandrglm/openwrt_wg-autoconf"
# arch="noarch"
# license="MIT"
# depends="wireguard-tools ip-tiny dnsmasq"
# options="!check !builddeps"
# source="wg-autoconf
# ###########################################################################################



#####################################################################
# IMPORTS
. /usr/libexec/wg-autoconf/debug.sh
#####################################################################
# GLOBALS
VERSION="v1.0.0r3"

WG_CONF_DIR="/etc/wireguard"
NETWORK_CONF="/etc/config/network"
DHCP_CONF="/etc/config/dhcp"
FIREWALL_CONF="/etc/config/firewall"
LOG_DIR="/etc/libexec/wg-autoconf/logs"

BACKUP_PATHS="$NETWORK_CONF $DHCP_CONF $FIREWALL_CONF"

#####################################################################
# ASH COLOUR TERM
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'
log() { echo -e "${BLUE}[wg-autoconf]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }


#####################################################################
# PROPERFUNCS
ui_lines() {

    lenght="$1"
    shift

    if [ -z "$1" ]; then

        printf '%*s\n' "$lenght" '' | tr ' ' '='

    else

        title="[ $* ]"
        base="==$title"
        padding=$((lenght - ${#base}))

        printf '%s' "$base"
        [ "$padding" -gt 0 ] && printf '%*s' "$padding" '' | tr ' ' '='
        printf '\n'
    fi
}


#####################################################################
# ██╗  ██╗███████╗██╗     ██████╗
# ██║  ██║██╔════╝██║     ██╔══██╗
# ███████║█████╗  ██║     ██████╔╝
# ██╔══██║██╔══╝  ██║     ██╔═══╝
# ██║  ██║███████╗███████╗██║
# ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝
#####################################################################
show_usage() {

    echo ""
    ui_lines 80 "WireGuard Auto-Config for OpenWrt $VERSION"
    echo "Usage: wg-autoconf <cmd> [args]"
    ui_lines 10
    echo "COMMANDS:"
    echo " "
    echo "  list                           - List all .conf files"
    echo "  list-full                      - List all .conf + all interfaces (detailed)"
    echo "  test <conf_name>               - Test .conf file"
    echo "  setup <conf_name>              - Setup from .conf (creates wg-<conf_name>)"
    echo "  setup --advanced               - Interactive setup (manual naming)"
    echo "  setup --advanced [conf]        - Advanced with .conf defaults"
    echo " "
    echo "  status                         - Show all WG interfaces"
    echo "  status <wg_iface>              - Show specific WG interface"
    echo "  up <wg_iface>                  - Enable WG interface"
    echo "  down <wg_iface>                - Disable WG interface"
    echo "  remove <wg_iface>              - Delete WG interface (unsetup)"
    echo " "
    echo "  routes show                    - Show all routes"
    echo "  routes show <wg_iface>         - Show routes for WG interface"
    echo "  routes set <wg_iface> <lan>    - Set routes: LAN -> WG"
    echo "  routes unset <wg_iface> <lan>  - Unset routes: LAN -> WG"
    echo " "
    echo "  backups show                   - List available backups"
    echo "  backups restore [name]         - Restore backup files"
    echo " "
    echo "  clean                          - Clean WG configs (interactive)"
    echo "  clean <wg_iface>               - Clean specific interface"
    echo "  nuke-all                       - DESTRUCTIVE: Remove ALL WG configs (no prompts)"
    echo "  cleanup emergency              - Manual interface selection"
    ui_lines 10
    echo "Examples:"
    echo "  wg-autoconf setup us21 / wg-autoconf setup --advanced"
    echo "  wg-autoconf up wg-us21 / wg-autoconf down wg-us21"
    echo "  wg-autoconf routes set wg-us21 lan4"
    echo "  wg-autoconf remove wg-us21"
    echo "  wg-autoconf clean / wg-autoconf clean wg-us21"
    ui_lines 80
    exit 1
}


#####################################################################
#
# ██╗     ██╗███████╗████████╗
# ██║     ██║██╔════╝╚══██╔══╝
# ██║     ██║███████╗   ██║
# ██║     ██║╚════██║   ██║
# ███████╗██║███████║   ██║
# ╚══════╝╚═╝╚══════╝   ╚═╝
#
#####################################################################
# 1   LIST
list_configs() {

    log ".conf files on $WG_CONF_DIR:"
    echo ""
    ui_lines 50

    for conf in "$WG_CONF_DIR"/*.conf; do

        [ -f "$conf" ] || continue

        name=$(basename "$conf" .conf)
        endpoint=$(get_conf_value "Endpoint" "$conf")
        address=$(get_conf_value "Address" "$conf")

        echo "  $name"
        [ -n "$address" ] && echo "    IP: $address"
        [ -n "$endpoint" ] && echo "    Endpoint: $endpoint"
        echo ""
    done
    ui_lines 50
}

#####################################################################
# 2.    LIST-FULL
list_configs_full() {

    log ".conf files + active interfaces (FULL):"
    echo ""

    ui_lines 50 "AVAILABLE CONFIG FILES"
    echo ""

    for conf in "$WG_CONF_DIR"/*.conf; do
        [ -f "$conf" ] || continue

        name=$(basename "$conf" .conf)
        endpoint=$(get_conf_value "Endpoint" "$conf")
        address=$(get_conf_value "Address" "$conf")

        echo "$name"
        [ -n "$address" ] && echo "  IP: $address"
        [ -n "$endpoint" ] && echo "  Endpoint: $endpoint"
    done

    echo ""
    ui_lines 50 "ACTIVE INTERFACES"
    local interfaces=$(get_all_wg_interfaces)

    if [ -z "$interfaces" ]; then
        echo "    No active WireGuard interfaces"
        ui_lines 50
        return
    fi

    for iface in $interfaces; do
        address=$(uci get "network.$iface.addresses" 2>/dev/null || echo "N/A")
        endpoint_host=$(uci get "network.$iface.option endpoint_host" 2>/dev/null || echo "N/A")
        endpoint_port=$(uci get "network.$iface.option endpoint_port" 2>/dev/null || echo "N/A")

        echo "$iface"
        echo "  Status: $(ip link show $iface 2>/dev/null | grep -q 'state UP' && echo 'UP' || echo 'DOWN')"
        echo "  IP: $address"
        echo "  Endpoint: $endpoint_host:$endpoint_port"

        if wg show "$iface" >/dev/null 2>&1; then
            latest=$(wg show "$iface" 2>/dev/null | grep "latest handshake" | head -1)
            [ -n "$latest" ] && echo "  $latest"

            transfer=$(wg show "$iface" 2>/dev/null | grep "transfer" | head -1)
            [ -n "$transfer" ] && echo "  $transfer"
        fi
        ui_lines 50
        echo ""
    done
}



#####################################################################
#
# ████████╗███████╗███████╗████████╗
# ╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝
#    ██║   █████╗  ███████╗   ██║
#    ██║   ██╔══╝  ╚════██║   ██║
#    ██║   ███████╗███████║   ██║
#    ╚═╝   ╚══════╝╚══════╝   ╚═╝
#
#####################################################################
# 3.    WG-AUTOCONF test
test_config() {

    local conf_name="$1"
    local conf_file="$WG_CONF_DIR/$conf_name.conf"

    [ -f "$conf_file" ] || error "[ERROR] $conf_file does not exist!"

    log "Testing $conf_name.conf..."

    private_key_raw=$(get_conf_value "PrivateKey" "$conf_file")
    public_key_raw=$(get_conf_value "PublicKey" "$conf_file")

    [ -z "$private_key_raw" ] && error "PrivateKey NOT FOUND!"
    [ -z "$public_key_raw" ] && error "PublicKey NOT FOUND!"

    private_key_clean=$(clean_wg_key "$private_key_raw")
    public_key_clean=$(clean_wg_key "$public_key_raw")

    echo ""
    ui_lines 80 "Key Validation"
    echo "Private Key: $private_key_clean"
    echo "Public Key: $public_key_clean"
    echo ""

    echo "Testing wg keys ..."
    echo ""

    if echo "$private_key_clean" | wg pubkey >/dev/null 2>&1; then

        success "VALID PrivateKey!"

    else

        error "Private key IS NOT VALID!!!"

    fi

    if echo "$public_key_clean" | wg pubkey >/dev/null 2>&1; then

        success "VALID PublicKey!"

    else

        warning "Public key IS NOT VALID!!!"

    fi
    ui_lines 80
}

#####################################################################
#
# ███████╗███████╗████████╗██╗   ██╗██████╗
# ██╔════╝██╔════╝╚══██╔══╝██║   ██║██╔══██╗
# ███████╗█████╗     ██║   ██║   ██║██████╔╝
# ╚════██║██╔══╝     ██║   ██║   ██║██╔═══╝
# ███████║███████╗   ██║   ╚██████╔╝██║
# ╚══════╝╚══════╝   ╚═╝    ╚═════╝ ╚═╝
#
#####################################################################
# WG-AUTOCONF setup NETWORK config
setup_network_config() {

    local iface_name="$1"
    local private_key="$2"
    local address="$3"
    local public_key="$4"
    local endpoint_host="$5"
    local endpoint_port="$6"
    local allowed_ips="$7"
    local dns="$8"

    log "Setting up NETWORK config for $iface_name..."

    local tag_id=$(get_next_tag_id "$NETWORK_CONF" "network")

    if grep -q "# wg-autoconf network start id $tag_id" "$NETWORK_CONF" 2>/dev/null; then
        warning "NETWORK tag with id $tag_id already exists. Skipping..."
        return 0
    fi

    # 3. FIX PROCESS ALLOWED IPS (split by comma or space)
    local allowed_ips_list=$(echo "$allowed_ips" | tr ' ' '\n' | sed 's/^[ \t]*//;s/[ \t]*$//' | grep -v '^$')

    cat >> "$NETWORK_CONF" << EOF
# wg-autoconf network start id $tag_id
config interface '$iface_name'
    option proto 'wireguard'
    option private_key '$private_key'
    option addresses '$address'
    option dns '$dns'

config wireguard_$iface_name '${iface_name}_peer'
    option public_key '$public_key'
    option endpoint_host '$endpoint_host'
    option endpoint_port '$endpoint_port'
    option route_allowed_ips '0'
    option persistent_keepalive '25'
EOF

    echo "$allowed_ips_list" | while read -r ip; do
        echo "    list allowed_ips '$ip'" >> "$NETWORK_CONF"
    done

    cat >> "$NETWORK_CONF" << EOF
# wg-autoconf network end id $tag_id
EOF

    success "Added NETWORK config for $iface_name (id: $tag_id)"
}

#####################################################################
# WG-AUTOCONF setup DHCP config
setup_dhcp_config() {

    local iface_name="$1"

    log "Setting up DHCP config for $iface_name..."

    local tag_id=$(get_next_tag_id "$DHCP_CONF" "dhcp")

    if grep -q "# wg-autoconf dhcp start id $tag_id" "$DHCP_CONF" 2>/dev/null; then
        warning "DHCP tag with id $tag_id already exists. Skipping..."
        return 0
    fi

    cat >> "$DHCP_CONF" << EOF
# wg-autoconf dhcp start id $tag_id
config dhcp '$iface_name'
    option interface '$iface_name'
    option ignore '1'
# wg-autoconf dhcp end id $tag_id
EOF

    success "Added DHCP config for $iface_name (id: $tag_id)"
}

#####################################################################
# WG-AUTOCONF setup Firewall config
setup_firewall_config() {

    local iface_name="$1"
    local endpoint_port="$2"

    log "Setting up Firewall config for $iface_name..."

    local tag_id=$(get_next_tag_id "$FIREWALL_CONF" "firewall")

    if grep -q "# wg-autoconf firewall start id $tag_id" "$FIREWALL_CONF" 2>/dev/null; then
        warning "Firewall tag with id $tag_id already exists. Skipping..."
        return 0
    fi

    cat >> "$FIREWALL_CONF" << EOF
# wg-autoconf firewall start id $tag_id
config zone
    option name '$iface_name'
    option input 'ACCEPT'
    option output 'ACCEPT'
    option forward 'ACCEPT'
    option masq '1'
    option mtu_fix '1'
    list network '$iface_name'

config rule
    option name 'Allow-WireGuard-$iface_name'
    option src 'wan'
    option dest_port '$endpoint_port'
    option proto 'udp'
    option target 'ACCEPT'

config forwarding
    option src '$iface_name'
    option dest 'wan'
# wg-autoconf firewall end id $tag_id
EOF

    # reload
    /etc/init.d/firewall reload 2> /dev/null

    success "Added Firewall config for $iface_name (id: $tag_id, port: $endpoint_port)"
}


#####################################################################
# WG-AUTOCONF SETUP CONF_NAME (AUTO MODE)
#
# Configures a WireGuard interface using a configuration file
# in automatic mode (minimal user intervention required).
#
# Workflow:
#   1. Extract values for CLI-UI and setup
#   2. Generate iface name from conf file name
#   3. VERIFY COLLISIONS FROM OTHER IFACES/ADDRS
#   4. CONFIG BACKUPS CALLBACK
#   5. LAST VERIFIES
#   6. APPLY-COMMITS
#   7. FIXES
#   8. CLI-UI RESULTS
#
# Params:
#   $1: Configuration name (without .conf extension)
#
# Exit Codes:
#   0 - Success
#   1 - Configuration error
#   0 - Cancelled (non-error exit for existing interface)
#
# Dependencies:
#   - get_conf_value()    - Extracts values from config file
#   - clean_wg_key()      - Sanitises WireGuard keys
#   - iface_exists()      - Checks if network interface exists
#   - address_exists()    - Checks for IP address collisions
#   - backup_config_file()- Creates backups of system configs
#   - process_allowed_ips()- Processes AllowedIPs entries
#   - setup_*_config()    - Configuration generators for each subsystem
#   - configure_dns_ipv6_filter() - Disables IPv6 for DNS
#   - ui_lines()          - Formatting helper for CLI output
#
#####################################################################
setup_wireguard() {


    # -------------------------------------------------------------------
    # 1. EXTRACT VALUES FOR CLI-UI AND SETUP
    # -------------------------------------------------------------------
    local conf_name="$1"
    local conf_file="$WG_CONF_DIR/$conf_name.conf"

    [ -f "$conf_file" ] || error "$conf_file is missing!!!"

    log "Setting up wg from $conf_name.conf..."

    private_key_raw=$(get_conf_value "PrivateKey" "$conf_file")
    public_key_raw=$(get_conf_value "PublicKey" "$conf_file")
    address=$(get_conf_value "Address" "$conf_file")
    endpoint=$(get_conf_value "Endpoint" "$conf_file")
    allowed_ips_raw=$(get_conf_value "AllowedIPs" "$conf_file")

    [ -z "$private_key_raw" ] && error "Missing PrivateKey"
    [ -z "$public_key_raw" ] && error "Missing PublicKey"
    [ -z "$address" ] && error "Missing Address"

    private_key=$(clean_wg_key "$private_key_raw")
    public_key=$(clean_wg_key "$public_key_raw")

    endpoint_host=$(echo "$endpoint" | cut -d: -f1)
    endpoint_port=$(echo "$endpoint" | cut -d: -f2)

    # -------------------------------------------------------------------
    # 2. GENERATE IFACE NAME FROM CONF FILE NAME
    # -------------------------------------------------------------------
    # IFACE_NAME for AUTO MODE "wg_" MMANDATORY
    # NOT ALLOWED BY wg due to kernel naming restrictions
    local iface_name="wg_$conf_name"

    # -------------------------------------------------------------------
    # 3. VERIFY COLLISIONS FROM OTHER IFACES/ADDRS
    # -------------------------------------------------------------------

    if iface_exists "$iface_name"; then

        warning "$iface_name already exists! Use --advanced mode!"
        log "WG-AUTOCONF setup CANCELLED!"
        exit 0
    fi


    local collision=$(address_exists "$address")
    if [ -n "$collision" ]; then
        error "Address $address already in use by $collision! Use --advanced mode to set it up with another interface name"
    fi


    # -------------------------------------------------------------------
    # 4. CONFIG BACKUPS CALLBACK
    # -------------------------------------------------------------------
    backup_config_file "$NETWORK_CONF"
    backup_config_file "$DHCP_CONF"
    backup_config_file "$FIREWALL_CONF"


    # -------------------------------------------------------------------
    # 5. LAST VERIFIES
    # -------------------------------------------------------------------
    if [ -n "$allowed_ips_raw" ]; then

        allowed_ips=$(process_allowed_ips "$allowed_ips_raw" | tr '\n' ' ')

    else
        # Default to full tunnel if no AllowedIPs specified
        allowed_ips="0.0.0.0/0"
    fi

    # -------------------------------------------------------------------
    # 6. APPLY-COMMITS
    # -------------------------------------------------------------------
    # Generate and apply configuration to all required subsystems
    setup_network_config "$iface_name" "$private_key" "$address" \
                         "$public_key" "$endpoint_host" "$endpoint_port" \
                         "$allowed_ips" "1.1.1.1 1.0.0.1"
    setup_dhcp_config "$iface_name"
    setup_firewall_config "$iface_name" "$endpoint_port"

    uci commit network
    uci commit dhcp
    uci commit firewall

    # -------------------------------------------------------------------
    # 7. FIXES
    # -------------------------------------------------------------------
    configure_dns_ipv6_filter

    # -------------------------------------------------------------------
    # 8. CLI-UI RESULTS
    # -------------------------------------------------------------------
    echo ""
    success "D0ne!"
    echo ""
    ui_lines 80 "WireGuard CONFIG"
    echo "Interface:           $iface_name"
    echo "Local IP:            $address"
    echo "Endpoint:            ${endpoint_host:-N/A}:${endpoint_port:-N/A}"
    echo "DNS:                 1.1.1.1 1.0.0.1"
    ui_lines 50 "FINISH SETUP"
    echo "1. ENABLE WG:         wg-autoconf up $iface_name"
    echo "2. VERIFY:            wg show $iface_name"
    echo "3. TEST WG:           ping -I $iface_name 8.8.8.8"
    echo "4. ROUTE LANs         wg-autoconf routes set $iface_name <lan_iface>"
    echo ""
    ui_lines 80
}

#####################################################################
# WG-AUTOCONF SETUP --advanced (MANUAL CONFIG)
#
# Interactive advanced WireGuard configuration with manual
# parameter input and optional .conf file loading.
#
# Workflow:
#   1.  Initialise configuration (optional file load)
#   2.  Interface name input with validation
#   3.  Private/Public key input with defaults
#   4.  Local address input with collision checking
#   5.  Endpoint host/port configuration
#   6.  Allowed IPs specification
#   7.  DNS servers configuration
#   8.  Optional configuration file save
#   9.  Configuration summary and confirmation
#   10. System backup and configuration generation
#   11. UCI commit operations
#   12. IPv6 leak prevention
#   13. Results display
#
# Params:
#   $1: Optional configuration filename (without .conf extension)
#       to load default values from
#
# Return Values:
#   0 - Success
#   1 - User cancelled or configuration error
#
# Notes:
#   - Default values from existing .conf files
#   - Input validation and collision checking
#   - Confirmation prompts
#
#####################################################################
setup_wireguard_advanced() {


    # -------------------------------------------------------------------
    # 1. INITIALISE CONFIGURATION (OPTIONAL FILE LOAD)
    # -------------------------------------------------------------------
    local conf_from_file="$1"
    local conf_file=""


    if [ -n "$conf_from_file" ]; then

        conf_file="$WG_CONF_DIR/$conf_from_file.conf"

        if [ ! -f "$conf_file" ]; then
            error "$conf_file does not exist!"
        fi
        l
        og "Loading defaults from $conf_from_file.conf..."
    fi

    log "Starting ADVANCED setup (manual configuration)..."
    echo ""

    ui_lines 80

    # -------------------------------------------------------------------
    # 2. INTERFACE NAME INPUT WITH VALIDATION
    # -------------------------------------------------------------------
    echo "1. WIREGUARD INTERFACE NAME (must start with 'wg')"
    default_iface="wg0"

    while true; do
        read -p "Enter interface name (default: $default_iface): " iface_name
        [ -z "$iface_name" ] && iface_name="$default_iface"

        # VALIDATE WG
        if ! echo "$iface_name" | grep -q "^wg"; then
            warning "Interface '$iface_name' name. Did you mean 'wg-$iface_name' ?"
            continue
        fi

        # Check for existing interface with same name
        if iface_exists "$iface_name"; then
            warning "Interface $iface_name already exists!"
            read -p "Try another name? (y/N): " retry
            case "$retry" in
                y|Y) continue ;;
                *) error "Interface name already in use!" ;;
            esac
        fi
        break
    done


    # -------------------------------------------------------------------
    # 3. PRIVATE/PUBLIC KEY INPUT WITH DEFAULTS
    # -------------------------------------------------------------------
    echo ""
    ui_lines 50
    echo "2. WIREGUARD KEYS"

    # PRIVATE INPUT OVERRIDE
    default_privkey=""

    if [ -n "$conf_file" ]; then

        default_privkey=$(get_conf_value "PrivateKey" "$conf_file")

    fi

    if [ -n "$default_privkey" ]; then

        echo "   [From $conf_from_file.conf: $(echo $default_privkey | cut -c1-10)...]"
        read -p "Enter PRIVATE key (press Enter to use default): " private_key_raw

        [ -z "$private_key_raw" ] && private_key_raw="$default_privkey"

    else

        read -p "Enter PRIVATE key: " private_key_raw
    fi

    [ -z "$private_key_raw" ] && error "Private key is required!"
    private_key=$(clean_wg_key "$private_key_raw")


    # PUBLIC INPUT OVERRIDE
    default_pubkey=""

    if [ -n "$conf_file" ]; then

        default_pubkey=$(get_conf_value "PublicKey" "$conf_file")

    fi


    if [ -n "$default_pubkey" ]; then

        echo "   [From $conf_from_file.conf: $(echo $default_pubkey | cut -c1-10)...]"
        read -p "Enter PUBLIC key (press Enter to use default): " public_key_raw

        [ -z "$public_key_raw" ] && public_key_raw="$default_pubkey"

    else
        read -p "Enter PUBLIC key: " public_key_raw
    fi

    [ -z "$public_key_raw" ] && error "Public key is required!"

    public_key=$(clean_wg_key "$public_key_raw")



    # -------------------------------------------------------------------
    # 4. LOCAL ADDRESS INPUT WITH COLLISION CHECKING
    # -------------------------------------------------------------------
    echo ""
    ui_lines 50
    echo "3. LOCAL ADDRESS"

    default_address=""
    if [ -n "$conf_file" ]; then
        default_address=$(get_conf_value "Address" "$conf_file")
    fi

    while true; do

        if [ -n "$default_address" ]; then

            read -p "Enter local IP address (default: $default_address): " address
            [ -z "$address" ] && address="$default_address"

        else

          read -p "Enter local IP address (e.g., 10.2.0.2/32): " address

        fi

        [ -z "$address" ] && error "Address is required!"


        # IP COLLISION
        local collision=$(address_exists "$address")

        if [ -n "$collision" ]; then

            warning "Address $address already in use by $collision!"
            read -p "Try another address? (y/N): " retry

            case "$retry" in
                y|Y) continue ;;
                *) error "Address already in use!" ;;
            esac
        fi

        break

    done

    # -------------------------------------------------------------------
    # 5. ENDPOINT HOST/PORT CONFIG
    # -------------------------------------------------------------------
    echo ""
    ui_lines 50
    echo "4. ENDPOINT"

    default_endpoint=""
    if [ -n "$conf_file" ]; then
        default_endpoint=$(get_conf_value "Endpoint" "$conf_file")
    fi

    # IP PARSER -d: -f1
    default_endpoint_host=""
    default_endpoint_port=""

    if [ -n "$default_endpoint" ]; then

        default_endpoint_host=$(echo "$default_endpoint" | cut -d: -f1)
        default_endpoint_port=$(echo "$default_endpoint" | cut -d: -f2)

    fi


    if [ -n "$default_endpoint_host" ]; then
        read -p "Enter endpoint host (default: $default_endpoint_host): " endpoint_host
        [ -z "$endpoint_host" ] && endpoint_host="$default_endpoint_host"
    else
        read -p "Enter endpoint host (IP or domain): " endpoint_host
    fi

    [ -z "$endpoint_host" ] && error "Endpoint host is required!"


    if [ -n "$default_endpoint_port" ]; then
        read -p "Enter endpoint port (default: $default_endpoint_port): " endpoint_port
        [ -z "$endpoint_port" ] && endpoint_port="$default_endpoint_port"
    else
        read -p "Enter endpoint port (default: 51820): " endpoint_port
    fi

    [ -z "$endpoint_port" ] && endpoint_port="51820"


    # -------------------------------------------------------------------
    # 6. ALLOWED IPS SPECIFICATION
    # -------------------------------------------------------------------
    echo ""
    ui_lines 50
    echo "5. ALLOWED IPs"
    echo "   (use space-separated values, e.g.: 0.0.0.0/0 ::/0)"

    default_allowed_ips=""

    if [ -n "$conf_file" ]; then

        default_allowed_ips=$(get_conf_value "AllowedIPs" "$conf_file")

    fi



    if [ -n "$default_allowed_ips" ]; then

        read -p "Enter allowed IPs (default: $default_allowed_ips): " allowed_ips
        [ -z "$allowed_ips" ] && allowed_ips="$default_allowed_ips"

    else

        read -p "Enter allowed IPs: " allowed_ips

    fi

    [ -z "$allowed_ips" ] && allowed_ips="0.0.0.0/0"



    # -------------------------------------------------------------------
    # 7. DNS SERVERS CONFIGURATION
    # -------------------------------------------------------------------
    echo ""
    ui_lines 50
    echo "6. DNS SERVERS"
    echo "   (use space-separated values, e.g.: 1.1.1.1 1.0.0.1)"

    read -p "Enter DNS servers: " dns
    [ -z "$dns" ] && dns="1.1.1.1 1.0.0.1"

    # TODO: Warn or block DNS <-> DNS/IP  collisions
    # for other_iface in $(get_all_wg_interfaces); do
    #     [ "$other_iface" = "$iface_name" ] && continue
    #     local other_addr=$(uci get "network.$other_iface.addresses" 2>/dev/null)
    #     if echo "$dns" | grep -q "$other_addr"; then
    #         warning "DNS contains Address from $other_iface"
    #     fi
    # done

    # -------------------------------------------------------------------
    # 8. OPTIONAL CONFIGURATION FILE SAVE
    # -------------------------------------------------------------------
    echo ""
    ui_lines 50
    echo "7. SAVE WIREGUARD CONFIGURATION FILE"

    read -p "Save config as .conf file in $WG_CONF_DIR? (y/N): " save_conf

    case "$save_conf" in
        y|Y|yes|YES|Yes)
            read -p "Enter config filename (without .conf extension): " conf_filename
            [ -z "$conf_filename" ] && conf_filename="advanced-$(date +%s)"

            save_wireguard_conf "$conf_filename" "$private_key" "$public_key" \
                                "$address" "$endpoint_host" "$endpoint_port" \
                                "$allowed_ips"
            ;;
        *)
            log "Config NOT saved to file"
            ;;
    esac

    # -------------------------------------------------------------------
    # 9. CONFIGURATION SUMMARY AND CONFIRMATION
    # -------------------------------------------------------------------
    echo ""
    ui_lines 50 "CONFIG SUMMARY"
    echo "Interface:           $iface_name"
    echo "Private Key:         $(echo $private_key | cut -c1-10)..."
    echo "Public Key:          $(echo $public_key | cut -c1-10)..."
    echo "Local IP:            $address"
    echo "Endpoint:            $endpoint_host:$endpoint_port"
    echo "Allowed IPs:         $allowed_ips"
    echo "DNS:                 $dns"
    ui_lines 50
    echo ""

    read -p "Confirm setup? (y/N): " response

    case "$response" in
        y|Y|yes|YES|Yes)
            ;;
        *)
            log "Setup cancelled!"
            return 1
            ;;
    esac

    # -------------------------------------------------------------------
    # 10. SYSTEM BACKUP AND CONFIGURATION GENERATION
    # -------------------------------------------------------------------
    backup_config_file "$NETWORK_CONF"
    backup_config_file "$DHCP_CONF"
    backup_config_file "$FIREWALL_CONF"

    setup_network_config "$iface_name" "$private_key" "$address" \
                         "$public_key" "$endpoint_host" "$endpoint_port" \
                         "$allowed_ips" "$dns"
    setup_dhcp_config "$iface_name"
    setup_firewall_config "$iface_name" "$endpoint_port"

    # -------------------------------------------------------------------
    # 11. UCI COMMIT OPERATIONS
    # -------------------------------------------------------------------
    uci commit network
    uci commit dhcp
    uci commit firewall

    # -------------------------------------------------------------------
    # 12. IPV6 LEAK PREVENTION
    # -------------------------------------------------------------------
    configure_dns_ipv6_filter

    # -------------------------------------------------------------------
    # 13. RESULTS DISPLAY
    # -------------------------------------------------------------------
    echo ""
    success "D0ne!"
    echo ""
    ui_lines 80 "WireGuard MANUAL CONFIG"
    echo "Interface:           $iface_name"
    echo "Local IP:            $address"
    echo "Endpoint:            $endpoint_host:$endpoint_port"
    echo "DNS:                 $dns"
    ui_lines 50 "FINISH SETUP"
    echo "1. ENABLE WG:         wg-autoconf up $iface_name"
    echo "2. VERIFY:            wg show $iface_name"
    echo "3. TEST WG:           ping -I $iface_name 8.8.8.8"
    echo "4. ROUTE LANs         wg-autoconf routes set $iface_name <lan_iface>"
    echo ""
    ui_lines 80

    return 0
}

#####################################################################
# WG-AUTOCONF save wireguard config file
save_wireguard_conf() {

    local conf_filename="$1"
    local private_key="$2"
    local public_key="$3"
    local address="$4"
    local endpoint_host="$5"
    local endpoint_port="$6"
    local allowed_ips="$7"

    local conf_file="$WG_CONF_DIR/$conf_filename.conf"

    if [ -f "$conf_file" ]; then

        warning "Config file already exists: $conf_file"

        read -p "Overwrite? (y/N): " response
        case "$response" in
            y|Y|yes|YES|Yes)
                ;;
            *)
                log "Config NOT saved"
                return 1
                ;;
        esac
    fi

    cat > "$conf_file" << EOF
# $conf_file
[Interface]
PrivateKey = $private_key
Address = $address
DNS = 1.1.1.1 1.0.0.1

[Peer]
PublicKey = $public_key
AllowedIPs = $allowed_ips
Endpoint = $endpoint_host:$endpoint_port
PersistentKeepalive = 25
EOF

    success "Config saved to: $conf_file"
    log "You can now use: wg-autoconf setup $conf_filename"
}




#####################################################################
# WG-AUTOCONF setup IPv6 disabling (dnsmasq->ipv6())
configure_dns_ipv6_filter() {

    if uci get dhcp.@dnsmasq[0] >/dev/null 2>&1; then

        current_filter=$(uci get dhcp.@dnsmasq[0].filter_aaaa 2>/dev/null || echo "0")

        if [ "$current_filter" != "1" ]; then

            log "Setting up dnsmasq filtering-IPv6-AAAA records ..."

            uci set dhcp.@dnsmasq[0].filter_aaaa='1'
            uci commit dhcp

            if /etc/init.d/dnsmasq restart >/dev/null 2>&1; then

                success "DNS configured: Filtering IPv6"

            else

                warning "Could not restart dnsmasq (may be OK)"

            fi
        fi
    else

        warning "dnsmasq NOT FOUND! Please verify requirements!"

    fi
}


#####################################################################
# WG-AUTOCONF up iface
activate_interface() {
    local iface="$1"

    [ -z "$iface" ] && error "Interface name required!"

    uci get "network.$iface" >/dev/null 2>&1 || error "$iface DOES NOT exist"

    validate_iface_name "$iface"

    log "Enabling $iface..."

    if ifup "$iface" >/dev/null 2>&1; then

        success "$iface enabled!"
        echo ""
        echo "Test commands:"
        echo "  wg show $iface"
    else
        error "Errors found enabling $iface. Check logread | tail -20"
    fi
}




#####################################################################
# WG-AUTOCONF down iface
deactivate_interface() {
    local iface="$1"

    [ -z "$iface" ] && error "Interface name required"

    uci get "network.$iface" >/dev/null 2>&1 || error "$iface DOES NOT exist"

    validate_iface_name "$iface"

    log "Disabling $iface..."

    ifdown "$iface" >/dev/null 2>&1 && success "$iface disabled!" || error "$iface was NOT disabled! Check logs!"
}

#####################################################################
# WG-AUTOCONF REMOVE IFACE (WITH TAG ID EXTRACTION)
#
# Removes a WireGuard interface configuration from all system
# configuration files using tagged block identification.
#
# Workflow:
#   1. Interface validation and existence checking
#   2. Interface deactivation (ifdown)
#   3. Tag ID extraction and validation for each subsystem:
#       a. Network configuration block
#       b. DHCP configuration block
#       c. Firewall configuration block
#   4. Tagged block removal via remove_tagged_block()
#   5. UCI commit operations
#   6. Success notification (unless silent mode)
#
# Params:
#   $1: Interface name to remove (e.g., wg0, wg_vpn)
#   $2: Optional mode flag:
#       - "silent": Suppresses output messages
#       - Any other value or omitted: Normal output mode
#
# Return Values:
#   0 - Success
#   1 - Interface does not exist or validation failed
#
# Tagging System:
#   Uses tagged comment blocks to identify configuration sections:
#   - # wg-autoconf [network|dhcp|firewall] start id <ID>
#   - # wg-autoconf [network|dhcp|firewall] end id <ID>
#   This ensures only wg-autoconf managed configurations are removed.
#
# Safety Features:
#   - Validates interface name format
#   - Verifies interface exists before removal
#   - Confirms tag end exists before block removal
#   - Graceful handling of missing configurations
#   - Silent mode for scripted/automated removal
#
#####################################################################
remove_wireguard() {


    # -------------------------------------------------------------------
    # 1. INTERFACE VALIDATION AND EXISTENCE CHECKING
    # -------------------------------------------------------------------
    local iface="$1"
    local silent="${2:-no}"

    validate_iface_name "$iface"

    [ "$silent" != "silent" ] && log "Removing $iface..."

    uci get "network.$iface" >/dev/null 2>&1 || {
        [ "$silent" != "silent" ] && error "$iface DOES NOT exist!"
        return 1
    }

    ifdown "$iface" 2>/dev/null



    # -------------------------------------------------------------------
    # 2. TAG ID EXTRACTION AND VALIDATION FOR EACH SUBSYSTEM
    # -------------------------------------------------------------------

    # NETWORK CONFIGURATION BLOCK
    # WORKFLOW FOR ALL CONF FILES:
    # A)    Find line number of config interface, then search upward for tag start
    local net_line=$(grep -n "config interface '$iface'" "$NETWORK_CONF" 2>/dev/null | cut -d: -f1)

    if [ -n "$net_line" ]; then

       # b) Extract network tag ID from start marker above the configuration
        local network_tag_id=$(sed -n "1,${net_line}p" "$NETWORK_CONF" | grep "# wg-autoconf network start id" | tail -1 | sed 's/.*id //g')

        # C)    Verify tag end exists before removal (safety check)
        if [ -n "$network_tag_id" ] && grep -q "# wg-autoconf network end id $network_tag_id" "$NETWORK_CONF"; then
            remove_tagged_block "$NETWORK_CONF" "network" "$network_tag_id"
        fi
    fi



    # DHCP CONFIGURATION BLOCK
    local dhcp_line=$(grep -n "config dhcp '$iface'" "$DHCP_CONF" 2>/dev/null | cut -d: -f1)

    if [ -n "$dhcp_line" ]; then

        local dhcp_tag_id=$(sed -n "1,${dhcp_line}p" "$DHCP_CONF" | grep "# wg-autoconf dhcp start id" | tail -1 | sed 's/.*id //g')
        if [ -n "$dhcp_tag_id" ] && grep -q "# wg-autoconf dhcp end id $dhcp_tag_id" "$DHCP_CONF"; then
            remove_tagged_block "$DHCP_CONF" "dhcp" "$dhcp_tag_id"
        fi
    fi



    # FIREWALL CONFIGURATION BLOCK
    local fw_line=$(grep -n "option name '$iface'" "$FIREWALL_CONF" 2>/dev/null | head -1 |
    cut -d: -f1)
    if [ -n "$fw_line" ]; then

        local firewall_tag_id=$(sed -n "1,${fw_line}p" "$FIREWALL_CONF" | grep "# wg-autoconf firewall start id" | tail -1 | sed 's/.*id //g')

        if [ -n "$firewall_tag_id" ] && grep -q "# wg-autoconf firewall end id $firewall_tag_id" "$FIREWALL_CONF"; then
            remove_tagged_block "$FIREWALL_CONF" "firewall" "$firewall_tag_id"
        fi
    fi

    # -------------------------------------------------------------------
    # 3. UCI COMMIT OPERATIONS
    # -------------------------------------------------------------------
    uci commit network 2>/dev/null
    uci commit dhcp 2>/dev/null
    uci commit firewall 2>/dev/null


    [ "$silent" != "silent" ] && success "$iface removed!"
}


#####################################################################
# ███████╗████████╗ █████╗ ████████╗██╗   ██╗███████╗
# ██╔════╝╚══██╔══╝██╔══██╗╚══██╔══╝██║   ██║██╔════╝
# ███████╗   ██║   ███████║   ██║   ██║   ██║███████╗
# ╚════██║   ██║   ██╔══██║   ██║   ██║   ██║╚════██║
# ███████║   ██║   ██║  ██║   ██║   ╚██████╔╝███████║
# ╚══════╝   ╚═╝   ╚═╝  ╚═╝   ╚═╝    ╚═════╝ ╚══════╝
#####################################################################

#####################################################################
# WG-AUTOCONF STATUS
#
# Displays the operational status and configuration details
# of WireGuard interfaces managed by wg-autoconf.
#
# Workflow:
#   1. Initialise interface filtering (if specified)
#   2. Retrieve list of active WireGuard interfaces
#   3. Check for active interfaces and handle empty state
#   4. Iterate through interfaces with optional filtering
#   5. Display interface configuration details:
#       a. Interface name
#       b. IP address from UCI configuration
#       c. Interface link status (UP/DOWN)
#       d. Connection details (endpoint, handshake, transfer)
#   6. Format output with consistent indentation
#
# Params:
#   $1: Optional interface name filter
#       - If provided: Shows status only for the specified interface
#       - If omitted or empty: Shows status for all active interfaces
#
# Output Format:
#   Interface Name
#     IP: <configured_address>
#     Status: <UP|DOWN>
#     endpoint: <peer_endpoint> (if interface is UP)
#     latest handshake: <timestamp> (if interface is UP and connected)
#     transfer: <data_transferred> (if interface is UP)
#
# Dependencies:
#   - wg show interfaces: Retrieves list of WireGuard interfaces
#   - uci get network.<iface>.addresses: Retrieves configured IP address
#   - ip link show: Checks interface link state
#   - wg show <iface>: Retrieves detailed WireGuard connection information
#
#
#####################################################################
show_status() {


    # -------------------------------------------------------------------
    # 1. INITIALISE INTERFACE FILTERING (IF SPECIFIED)
    # -------------------------------------------------------------------
    local filter_iface="$1"

    log "WireGuard status:"
    echo ""

    # -------------------------------------------------------------------
    # 2. RETRIEVE LIST OF ACTIVE WIREGUARD INTERFACES
    # -------------------------------------------------------------------
    local interfaces=$(wg show interfaces 2>/dev/null)

    # -------------------------------------------------------------------
    # 3. CHECK FOR ACTIVE INTERFACES AND HANDLE EMPTY STATE
    # -------------------------------------------------------------------
    if [ -z "$interfaces" ]; then

        echo "No active WireGuard interfaces found"
        return

    fi

    # -------------------------------------------------------------------
    # 4. ITERATE THROUGH INTERFACES WITH OPTIONAL FILTERING
    # -------------------------------------------------------------------
    for iface in $interfaces; do

        # FILTER
        if [ -n "$filter_iface" ] && [ "$iface" != "$filter_iface" ]; then

            continue

        fi


        # -------------------------------------------------------------------
        # 5. DISPLAY INTERFACE CONFIGURATION DETAILS
        # -------------------------------------------------------------------

        # 5a. Display interface name
        echo "$iface"

        # 5b. Retrieve and display configured IP address
        local address=$(uci get "network.$iface.addresses" 2>/dev/null || echo "N/A")
        echo "  IP: $address"

        # 5c. Check interface link status and display UP/DOWN
        if ip link show "$iface" >/dev/null 2>&1; then

            echo "  Status: UP"

            # Retrieve detailed WireGuard connection information
            local peer_info=$(wg show "$iface" 2>/dev/null)

            # 5d. Extract and display endpoint (peer connection details)
            local endpoint=$(echo "$peer_info" | grep "endpoint:" | head -1)
            [ -n "$endpoint" ] && echo "  $endpoint"

            # 5d. Extract and display latest handshake timestamp
            local handshake=$(echo "$peer_info" | grep "latest handshake:" | head -1)
            [ -n "$handshake" ] && echo "  $handshake"

            # 5d. Extract and display data transfer statistics
            local transfer=$(echo "$peer_info" | grep "transfer:" | head -1)
            [ -n "$transfer" ] && echo "  $transfer"

        else

           # Interface exists in configuration but link is down
            echo "  Status: DOWN"

        fi


        echo ""
    done
}


#####################################################################
#
#  ██████╗██╗     ███████╗ █████╗ ███╗   ██╗██╗   ██╗██████╗
# ██╔════╝██║     ██╔════╝██╔══██╗████╗  ██║██║   ██║██╔══██╗
# ██║     ██║     █████╗  ███████║██╔██╗ ██║██║   ██║██████╔╝
# ██║     ██║     ██╔══╝  ██╔══██║██║╚██╗██║██║   ██║██╔═══╝
# ╚██████╗███████╗███████╗██║  ██║██║ ╚████║╚██████╔╝██║
#  ╚═════╝╚══════╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝
#
#####################################################################

#####################################################################
# WG-AUTOCONF clean - INTERACTIVE MODE
clean_wireguard_interactive() {

    local filter_iface="$1"

    if [ -n "$filter_iface" ]; then
        # CLEAN SPECIFIC INTERFACE
        validate_iface_name "$filter_iface"

        if ! iface_exists "$filter_iface"; then
            error "$filter_iface does not exist!"
        fi

        log "Cleaning interface: $filter_iface"
        echo ""

        # SHOW CONFIG
        local address=$(uci get "network.$filter_iface.addresses" 2>/dev/null || echo "N/A")
        local endpoint=$(uci get "network.${filter_iface}_peer.endpoint_host" 2>/dev/null || echo "N/A")

        echo "Interface details:"
        echo "  Name: $filter_iface"
        echo "  Address: $address"
        echo "  Endpoint: $endpoint"
        echo ""

        printf "Remove this interface? (y/N): "
        read -r confirm

        case "$confirm" in
            y|Y|yes|YES|Yes)
                remove_wireguard "$filter_iface"
                cleanup_wireguard_routes
                success "$filter_iface cleaned!"
                ;;
            *)
                log "Clean cancelled"
                return 1
                ;;
        esac

    else

        # INTERACTIVE: LIST ALL + SELECT
        local interfaces=$(get_all_wg_interfaces)

        if [ -z "$interfaces" ]; then

            warning "No WireGuard interfaces found"
            return 1

        fi


        log "Available WireGuard interfaces:"
        echo ""

        echo "Select interface to clean:"
        local count=0
        for iface in $interfaces; do
            count=$((count + 1))
            local address=$(uci get "network.$iface.addresses" 2>/dev/null || echo "N/A")
            echo "  $count) $iface [$address]"
        done

        echo ""
        printf "Enter number (or press Enter to cancel): "
        read -r selection

        [ -z "$selection" ] && return 0

        local count=0
        for iface in $interfaces; do
            count=$((count + 1))
            if [ "$count" = "$selection" ]; then
                # SHOW CONFIG + CONFIRM
                local address=$(uci get "network.$iface.addresses" 2>/dev/null || echo "N/A")
                local endpoint=$(uci get "network.${iface}_peer.endpoint_host" 2>/dev/null || echo "N/A")

                echo ""
                echo "Will remove:"
                echo ""
                ui_lines 15
                echo "  Interface: $iface"
                echo "  Address: $address"
                echo "  Endpoint: $endpoint"
                ui_lines 15
                echo ""

                printf "Confirm removal? (y/N): "
                read -r confirm

                case "$confirm" in
                    y|Y|yes|YES|Yes)
                        remove_wireguard "$iface"
                        cleanup_wireguard_routes
                        success "$iface cleaned!"
                        ;;
                    *)
                        log "Clean cancelled"
                        return 1
                        ;;
                esac
                return 0
            fi
        done

        warning "Invalid selection"
        return 1
    fi
}

#####################################################################
# WG-AUTOCONF CLEANUP --> ROUTES
#
# Comprehensive cleanup of WireGuard interfaces, routing tables,
# and firewall rules to restore system to pre-WireGuard state.
#
# Workflow:
#   1. Remove all WireGuard network interfaces (wg*)
#   2. Remove all VPN routing tables from rt_tables
#   3. Flush all IP rules related to VPN routing
#
# Safety Features:
#   - Only removes interfaces matching wg* pattern
#   - Preserves existing rt_tables entries not containing "_vpn_"
#   - Uses temporary file for safe rt_tables modification
#   - Explicit rule number extraction for precise rule deletion
#
# Impact:
#   - Removes WireGuard kernel interfaces (ip link delete)
#   - Modifies /etc/iproute2/rt_tables
#   - Alters IP routing policy database (ip rule)
#
# Dependencies:
#   - ip link: Interface management
#   - ip rule: Routing policy management
#   - grep/sed/awk: Text processing for rule extraction
#
#####################################################################
cleanup_wireguard_routes() {
    log "Cleaning up WireGuard routing tables and interfaces..."

    # -------------------------------------------------------------------
    # 1. REMOVE ALL wg* INTERFACES
    # -------------------------------------------------------------------
    # Extract all interface names starting with "wg" from ip link output
    for iface in $(ip link show 2>/dev/null | grep -oE "wg[0-9a-zA-Z-]*" | sort -u); do

        ip link delete "$iface" 2>/dev/null
        success "Deleted interface: $iface"

    done

    # -------------------------------------------------------------------
    # 2. REMOVE ALL _vpn TABLES FROM rt_tables
    # -------------------------------------------------------------------
    # CHECK IF TABLES EXIST
    if [ -f /etc/iproute2/rt_tables ]; then

        # MATCH AND REMOVE "_vpn_"
        grep -v "_vpn_" /etc/iproute2/rt_tables > /tmp/rt_tables.tmp 2>/dev/null

        # Replace original file with filtered version
        mv /tmp/rt_tables.tmp /etc/iproute2/rt_tables

        success "VPN routing tables removed!"
    fi

    # -------------------------------------------------------------------
    # 3. FLUSH ALL IP RULES RELATED TO wg/vpn
    # -------------------------------------------------------------------
    # Extract and delete all IP rules containing "_vpn_" pattern
    ip rule show 2>/dev/null | grep "_vpn_" | while read -r line; do

        # EEXTRACT PRIORITY ID
        rule_num=$(echo "$line" | awk '{print $1}' | sed 's/:$//')

        # DELETE RULE
        [ -n "$rule_num" ] && ip rule del prio "$rule_num" 2>/dev/null
    done

    success "WireGuard routing rules flushed!"
}

#####################################################################
# WG-AUTOCONF CLEANUP CONFIG FILES
#
# Removes all wg-autoconf managed configurations from UCI
# files and optionally restores from backup.
#
# Workflow:
#   1. Remove all tagged configuration blocks from UCI files
#   2. Commit UCI changes to persist removal
#   3. Optionally restore configurations from backup files
#       a. Automatic mode (force_yes=1): Restore without confirmation
#       b. Interactive mode: Prompt user for confirmation
#
# Params:
#   $1: Force mode flag (optional, default: 0)
#       - 0: Interactive mode with user prompts
#       - 1: Automatic mode, restore backups without confirmation
#
# Tag System:
#   Uses tagged comment blocks to identify wg-autoconf managed sections:
#   - Network: # wg-autoconf network start id <ID>
#   - DHCP:    # wg-autoconf dhcp start id <ID>
#   - Firewall: # wg-autoconf firewall start id <ID>
#
# Backup Files:
#   Backup files are named with .BACKUP_PRE_WIREGUARD suffix and contain
#   original configurations before wg-autoconf modifications.
#   They are NOT really needed, as cleanup/nuke-all can hanble this, but
#   for recovery reasons (in case of system malfunction/garbage after uninstalling)
#
# Safety:
#   - Only removes tagged blocks (prevents accidental deletion)
#   - Preserves backup files for recovery
#
# Dependencies:
#   - remove_tagged_block(): Core tag removal function
#   - uci commit: Configuration persistence
#   - cleanup_backup_files(): Backup file cleanup
#
#
#####################################################################
cleanup_config_files() {
    local force_yes="${1:-0}"

    log "Cleaning up configuration files..."

    # -------------------------------------------------------------------
    # 1. REMOVE ALL TAGGED CONFIGURATION BLOCKS
    # -------------------------------------------------------------------
    # GET and EXTRACT
    local all_network_ids=$(grep "# wg-autoconf network start id" "$NETWORK_CONF" 2>/dev/null | sed 's/.*id //g')
    local all_dhcp_ids=$(grep "# wg-autoconf dhcp start id" "$DHCP_CONF" 2>/dev/null | sed 's/.*id //g')
    local all_firewall_ids=$(grep "# wg-autoconf firewall start id" "$FIREWALL_CONF" 2>/dev/null | sed 's/.*id //g')

    # Clean exact TAG-ID blocks for network
    for id in $all_network_ids; do
        remove_tagged_block "$NETWORK_CONF" "network" "$id"
    done

    # Same for DHCP
    for id in $all_dhcp_ids; do
        remove_tagged_block "$DHCP_CONF" "dhcp" "$id"
    done

    # Same for Firewall
    for id in $all_firewall_ids; do
        remove_tagged_block "$FIREWALL_CONF" "firewall" "$id"
    done

    # -------------------------------------------------------------------
    # 2. UCI COMMIT LIGHT & D0NE & NEXT
    # -------------------------------------------------------------------
    uci commit firewall 2>/dev/null
    uci commit dhcp 2>/dev/null
    uci commit network 2>/dev/null

    success "Configuration files cleaned"

    # -------------------------------------------------------------------
    # 3. OPTIONAL RESTORATION FROM BACKUPS
    # -------------------------------------------------------------------
    echo ""

    if [ $force_yes -eq 1 ]; then
        # -------------------------------------------------------------------
        # 3a. AUTOMATIC MODE: RESTORE WITHOUT CONFIRMATION
        # -------------------------------------------------------------------
        log "Restoring from backups (automatic mode)..."

        # FIND ALL BACKS
        for path in $BACKUP_PATHS; do

            backup_file="${path}.BACKUP_PRE_WIREGUARD"
            config_name=$(basename "$path")

            # RESTORE
            if [ -f "$backup_file" ]; then
                cp "$backup_file" "$path" 2>/dev/null
                success "Restored $config_name"
            fi
        done

        # COMMIT LIGHT
        uci commit network 2>/dev/null
        uci commit dhcp 2>/dev/null
        uci commit firewall 2>/dev/null

        success "Backups restored and committed"

        cleanup_backup_files

    else
        # -------------------------------------------------------------------
        # 3b. INTERACTIVE MODE: PROMPT USER FOR CONFIRMATION
        # -------------------------------------------------------------------
        read -p "Restore configs from backups? (y/N): " restore_choice

        case "$restore_choice" in
            y|Y|yes|YES|Yes)
                log "Restoring from backups..."

                # FIND ALL BACKS
                for path in $BACKUP_PATHS; do

                    backup_file="${path}.BACKUP_PRE_WIREGUARD"
                    config_name=$(basename "$path")

                    # RESTORE
                    if [ -f "$backup_file" ]; then
                        cp "$backup_file" "$path" 2>/dev/null
                        success "Restored $config_name"
                    fi
                done

                # COMMIT LIGHT
                uci commit network 2>/dev/null
                uci commit dhcp 2>/dev/null
                uci commit firewall 2>/dev/null

                success "Backups restored and committed"
                cleanup_backup_files
                ;;
            *)

                log "Backups NOT restored. You can do it manually with: wg-autoconf backups restore"
                ;;
        esac
    fi
}

#####################################################################
# WG-AUTOCONF NUKE-ALL
#
# Performs comprehensive, non-interactive removal of all
# WireGuard configurations, interfaces, and routing rules.
# Needed for pre_deinstall() lifecycle.
#
# Workflow:
#   1.  Interface enumeration and deactivation
#   2.  Removal of all tagged configuration blocks
#   3.  UCI configuration commit
#   4.  Routing table and rule cleanup
#   5.  Automatic backup restoration
#   6.  Backup file cleanup
#   7.  Completion notification
#
# Dependencies:
#   - get_all_wg_interfaces(): Returns all wg-autoconf managed interfaces
#   - remove_tagged_block(): Removes tagged configuration sections
#   - cleanup_wireguard_routes(): Cleans routing tables and rules
#   - cleanup_backup_files(): Removes backup files after restoration
#
#####################################################################
nuke_all() {
    log "NUKE CLEANUP MODE - Removing ALL WireGuard configurations without asking"
    echo ""
    echo ""

    # -------------------------------------------------------------------
    # 1. GET IFACES AND DESTROY
    # -------------------------------------------------------------------
    # Retrieve all wg-autoconf managed interfaces
    local interfaces=$(get_all_wg_interfaces)

    if [ -n "$interfaces" ]; then

        log "Taking down all WireGuard interfaces..."
        for iface in $interfaces; do

            # IFDOWN
            ifdown "$iface" 2>/dev/null
            success "Downed: $iface"

        done
        echo ""
    fi

    # -------------------------------------------------------------------
    # 2. REMOVAL OF ALL TAGGED CONFIGURATION BLOCKS
    # -------------------------------------------------------------------
    log "Removing configuration blocks..."

    # FIND-EXTRACT TAG-ID
    local all_network_ids=$(grep "# wg-autoconf network start id" "$NETWORK_CONF" 2>/dev/null | sed 's/.*id //g')
    local all_dhcp_ids=$(grep "# wg-autoconf dhcp start id" "$DHCP_CONF" 2>/dev/null | sed 's/.*id //g')
    local all_firewall_ids=$(grep "# wg-autoconf firewall start id" "$FIREWALL_CONF" 2>/dev/null | sed 's/.*id //g')

    # REMOVE ITS EXACT BLOCK ON NETWORK
    for id in $all_network_ids; do
        remove_tagged_block "$NETWORK_CONF" "network" "$id"
    done

    # SAME FOR DHCP
    for id in $all_dhcp_ids; do
        remove_tagged_block "$DHCP_CONF" "dhcp" "$id"
    done

    # SAME FOR FIREWALL
    for id in $all_firewall_ids; do

        remove_tagged_block "$FIREWALL_CONF" "firewall" "$id"

    done

    # -------------------------------------------------------------------
    # 3. UCI CONFIGURATION LIGHT COMMIT
    # -------------------------------------------------------------------
    uci commit network 2>/dev/null
    uci commit dhcp 2>/dev/null
    uci commit firewall 2>/dev/null

    success "Configuration files cleaned"
    echo ""

    # -------------------------------------------------------------------
    # 4. ROUTING TABLE AND RULE CLEANUP
    # -------------------------------------------------------------------
    log "Cleaning routing tables and rules..."

    cleanup_wireguard_routes

    echo ""

    # -------------------------------------------------------------------
    # 5. AUTOMATIC BACKUP RESTORATION
    # -------------------------------------------------------------------
    log "Restoring from backups..."

    # RESTORE
    for path in $BACKUP_PATHS; do

        backup_file="${path}.BACKUP_PRE_WIREGUARD"
        config_name=$(basename "$path")

        # CHECK BACKS
        if [ -f "$backup_file" ]; then

            cp "$backup_file" "$path" 2>/dev/null
            success "Restored $config_name"
        fi

    done

    # COMMIT LIGHT
    uci commit network 2>/dev/null
    uci commit dhcp 2>/dev/null
    uci commit firewall 2>/dev/null

    success "Backups restored and committed"
    echo ""

    cleanup_backup_files



    echo ""
    success "NUCLEAR CLEANUP COMPLETED - System ready for new setup!"
}

#####################################################################
#
# ██████╗  ██████╗ ██╗   ██╗████████╗███████╗███████╗
# ██╔══██╗██╔═══██╗██║   ██║╚══██╔══╝██╔════╝██╔════╝
# ██████╔╝██║   ██║██║   ██║   ██║   █████╗  ███████╗
# ██╔══██╗██║   ██║██║   ██║   ██║   ██╔══╝  ╚════██║
# ██║  ██║╚██████╔╝╚██████╔╝   ██║   ███████╗███████║
# ╚═╝  ╚═╝ ╚═════╝  ╚═════╝    ╚═╝   ╚══════╝╚══════╝
#
#####################################################################

#####################################################################
# WG-AUTOCONF ROUTES SHOW
#
# Displays WireGuard routing configuration including routing
# Tables, IP rules, and route entries for VPN traffic.
#
# Workflow:
#   1. Validate existence of routing tables configuration file
#   2. Display routes based on filtering criteria:
#       a. Filtered mode: Show routes for specific WireGuard interface
#       b. Unfiltered mode: Show all WireGuard routes
#   3. Handle empty state when no routes are configured
#
# Params:
#   $1: Optional WireGuard interface name filter
#       - If provided: Shows only routes associated with this interface
#       - If omitted or empty: Shows all WireGuard routes
#
# Output Format (Filtered Mode):
#   lan_interface_name:
#     Table: _vpn_wgX_lanY (ID: table_id)
#     Rule: <priority>: from <source> lookup <table_name>
#     Route: <destination> via <gateway> dev <interface>
#
# Output Format (Unfiltered Mode):
#   _vpn_wgX_lanY (ID: table_id):
#     Rule: <priority>: from <source> lookup <table_name>
#     Route: <destination> via <gateway> dev <interface>
#
# Table Naming Convention:
#   _vpn_<wg_interface>_<lan_interface>
#   Example: _vpn_wg0_lan - Routes LAN traffic through wg0 interface
#
# Data Sources:
#   - /etc/iproute2/rt_tables: Routing table names and IDs
#   - ip rule show: Routing policy rules
#   - ip route show table <name>: Route entries per table
#
# Error Handling:
#   - Validates existence of rt_tables configuration file
#   - Gracefully handles missing routing tables
#   - Provides clear "no routes" message when appropriate
#   - Silently handles non-existent table queries
#
# Filtering Logic:
#   Filtered mode extracts LAN interface name from table name:
#   table_name="_vpn_wg0_lan" -> lan_iface="lan"
#   This provides more user-friendly output for specific WG interfaces.
#
#####################################################################
show_routes() {


    local filter_wg_iface="$1"

    log "WireGuard routes:"
    echo ""

    # -------------------------------------------------------------------
    # 1. VALIDATE ROUTING TABLES CONFIGURATION FILE
    # -------------------------------------------------------------------
    # Check if routing tables configuration file exists
    if [ ! -f /etc/iproute2/rt_tables ]; then

        error "/etc/iproute2/rt_tables not found"

    fi

    # -------------------------------------------------------------------
    # 2. DISPLAY ROUTES BASED ON FILTERING CRITERIA
    # -------------------------------------------------------------------
    if [ -n "$filter_wg_iface" ]; then


        # -------------------------------------------------------------------
        # 2a. FILTERED MODE: SHOW ROUTES FOR SPECIFIC WIREGUARD INTERFACE
        # -------------------------------------------------------------------
        log "Routes for $filter_wg_iface:"
        echo ""

        # Find all routing tables associated with the specified WireGuard interface
        grep "_vpn_${filter_wg_iface}" /etc/iproute2/rt_tables 2>/dev/null | while read -r line; do

            # GET TABLES
            local table_id=$(echo "$line" | awk '{print $1}')
            local table_name=$(echo "$line" | awk '{print $2}')

            # EXTRACT LAN IFACE NAME FROM TABLE
            local lan_iface=$(echo "$table_name" | sed "s/_vpn_${filter_wg_iface}//g")

            # TABLE SECTION HEADER
            echo "$lan_iface:"
            echo "  Table: $table_name (ID: $table_id)"

            # SHOW RULES FOR THIS TABLE
            ip rule show | grep "lookup $table_name" | sed 's/^/  Rule: /'

            # SHOW ALL ROUTES
            ip route show table "$table_name" 2>/dev/null | while read -r route; do

                echo "  Route: $route"
            done || echo "  (no routes found)"

            echo ""
        done

    else
    
        # -------------------------------------------------------------------
        # 2b. UNFILTERED MODE: SHOW ALL WIREGUARD ROUTES
        # -------------------------------------------------------------------
        log "All routes:"
        echo ""

        # Find all VPN routing tables WITH "_vpn_" pattern
        grep "_vpn_" /etc/iproute2/rt_tables 2>/dev/null | while read -r line; do

            # GET TABLES
            local table_id=$(echo "$line" | awk '{print $1}')
            local table_name=$(echo "$line" | awk '{print $2}')

            # TABLE SECTION HEADER
            echo "$table_name (ID: $table_id):"

            # SHOW RULES FOR THIS TABLE
            ip rule show | grep "lookup $table_name" | sed 's/^/  Rule: /'

            # SHOW ALL ROUTES
            ip route show table "$table_name" 2>/dev/null | while read -r route; do

                echo "  Route: $route"

            done || echo "  (no routes)"

            echo ""
        done
    fi

    # -------------------------------------------------------------------
    # 3. HANDLE EMPTY STATE WHEN NO ROUTES ARE CONFIGURED
    # -------------------------------------------------------------------
    # CHECK PREOVIOUS VPN TABLES
    [ -z "$(grep '_vpn_' /etc/iproute2/rt_tables 2>/dev/null)" ] && echo "No WireGuard routes configured"
}


#####################################################################
# WG-AUTOCONF ROUTES SET
#
#   Configures policy-based routing to route traffic from a
#  specific LAN interface through a WireGuard VPN interface.
#
# Workflow:
#   1.  Validate existence of both WireGuard and LAN interfaces
#   2.  Retrieve LAN interface IP address and netmask
#   3.  Convert netmask to CIDR notation and calculate subnet
#   4.  Create routing table if it doesn't exist
#   5.  Configure routing rules and routes:
#       a. Clean previous configurations
#       b. Add policy routing rules
#       c. Add local subnet route
#       d. Add default route through WireGuard interface
#   6.  Configure firewall forwarding rules between interfaces
#   7.  Commit firewall changes and reload service
#   8.  Display configuration summary
#
# Params:
#   $1: WireGuard interface name (e.g., wg0, wg_vpn)
#   $2: LAN interface name (e.g., lan, lan2)
#
#
# Policy Routing Configuration:
#   - Traffic FROM LAN subnet -> Use custom routing table
#   - Traffic FROM LAN subnet TO LAN subnet -> Use main table (local traffic)
#   - All other traffic FROM LAN subnet -> Use WireGuard interface
#
# Firewall Configuration:
#   Adds bidirectional forwarding rules between LAN and WireGuard interfaces
#   to allow traffic flow between the two network zones.
#
# Error Handling:
#   - Validates interfaces/netconfs existence in UCI configuration
#   - Checks WireGuard interface link state before adding default route
#   - Verifies where to exactly add the firewall rules by tag system
#
# Dependencies:
#   - iface_exists(): Validates interface configuration
#   - netmask_to_cidr(): Converts netmask to CIDR notation
#   - ip_to_network(): Calculates network address from IP and CIDR
#
#####################################################################
set_lan_routes() {


    local wg_iface="$1"
    local lan_iface="$2"

    # -------------------------------------------------------------------
    # 1. VALIDATE / COLLISIONS
    # -------------------------------------------------------------------
    # VERIFY PREVIOUS WG IFACES
    if ! iface_exists "$wg_iface"; then
        error "$wg_iface interface NOT FOUND at /etc/config/network"
    fi

    # VERIFY PREVIOUS LANs
    if ! iface_exists "$lan_iface"; then
        error "$lan_iface interface NOT FOUND at /etc/config/network"
    fi

    # -------------------------------------------------------------------
    # 2. GET PREVIOUS LAN CONFIG
    # -------------------------------------------------------------------
    local lan_ip=$(uci get "network.$lan_iface.ipaddr" 2>/dev/null)
    local lan_netmask=$(uci get "network.$lan_iface.netmask" 2>/dev/null)

    [ -z "$lan_ip" ] && error "Desired iface, $lan_iface IP not found!"

    # -------------------------------------------------------------------
    # 3. IP/NMETMAKS 2 CIDR
    # -------------------------------------------------------------------
    local cidr
    local subnet

    if [ -n "$lan_netmask" ]; then
        cidr=$(netmask_to_cidr "$lan_netmask")
    else
        cidr=24
        warning "$lan_iface netmask not found, assuming /24"
    fi

    subnet="$(ip_to_network "$lan_ip" "$cidr")/$cidr"

    # CREATE
    local table_name="${lan_iface}_vpn_${wg_iface}"

    log "Setting up routes for $lan_iface ($subnet) -> $wg_iface..."

    # -------------------------------------------------------------------
    # 4. CREATE ROUTING TABLE IF IT DOESN'T EXIST
    # -------------------------------------------------------------------
    # CHECK PREVIOUS TABLES
    if ! grep -q "$table_name" /etc/iproute2/rt_tables 2>/dev/null; then

        # TODO: IDX TO TABLENAME
        echo "150 $table_name" >> /etc/iproute2/rt_tables
        success "Table:: $table_name"

    fi

    # -------------------------------------------------------------------
    # 5. CONFIGURE ROUTING RULES AND ROUTES
    # -------------------------------------------------------------------

    # 5.1. RULE DEL&FLUSH
    ip rule del from "$subnet" lookup "$table_name" 2>/dev/null
    ip route flush table "$table_name" 2>/dev/null

    # 5.2. ADD POLICY ROUTING RULES FROM LAN TO
    ip rule add from "$subnet" lookup "$table_name"
    ip rule add from "$subnet" to "$subnet" lookup main

    # 5.3. ADD LOCAL SUBNET ROUTE
    ip route add "$subnet" dev "$lan_iface" table "$table_name"

    # 5.4. ADD DEFAULT ROUTE THROUGH WIREGUARD IFACE
    if ip link show "$wg_iface" >/dev/null 2>&1; then

        # ROUTE ADD WG
        ip route add default dev "$wg_iface" table "$table_name"
        success "Route conf done!: $lan_iface through $wg_iface"
        
        echo "   ANY device from $subnet will use WireGuard ($wg_iface)"

    else
        
        warning "$wg_iface does NOT exist yet!"
        
        echo "   $lan_iface WONT work unless $wg_iface is active!"
        echo "   To activate: wg-autoconf up $wg_iface"

    fi

    # -------------------------------------------------------------------
    # 6. CONFIGURE FIREWALL FORWARDING RULES
    # -------------------------------------------------------------------
    log "Adding firewall forwarding rules..."

    # FIND FIREWALL TAG ID
    local fw_line=$(grep -n "option name '$wg_iface'" "$FIREWALL_CONF" 2>/dev/null | head -1 | cut -d: -f1)

    if [ -n "$fw_line" ]; then

    
    # EXTRACT ITS OWN FIREWALL TAG ID
        
        local fw_tag_id=$(sed -n "1,${fw_line}p" "$FIREWALL_CONF" | grep "# wg-autoconf firewall start id" | tail -1 | sed 's/.*id //g')

        if [ -n "$fw_tag_id" ] && grep -q "# wg-autoconf firewall end id $fw_tag_id" "$FIREWALL_CONF"; then

            # CHECK MATCH
            if ! grep -q "option src '$lan_iface'" "$FIREWALL_CONF" 2>/dev/null | grep -q "option dest '$wg_iface'" 2>/dev/null; then

            # THEN, ON LAST NUMBER
                local fw_end_line=$(grep -n "# wg-autoconf firewall end id $fw_tag_id" "$FIREWALL_CONF" | cut -d: -f1)

            # INSERT BIDI FORWARD TAGS
                sed -i "${fw_end_line}i\\

config forwarding\\
    option src '$lan_iface'\\
    option dest '$wg_iface'\\
\\
config forwarding\\
    option src '$wg_iface'\\
    option dest '$lan_iface'\\
" "$FIREWALL_CONF"

                success "Firewall forwarding rules added"
            else
                warning "Forwarding rules already exist for $lan_iface <-> $wg_iface"
            fi
        else
            warning "Firewall tag not found for $wg_iface"
        fi
    else
        warning "Could not find firewall configuration for $wg_iface"
    fi


    # -------------------------------------------------------------------
    # 7. COMMIT FIREWALL AND SRESULTS
    # -------------------------------------------------------------------
    uci commit firewall 2>/dev/null
    /etc/init.d/firewall reload 2> /dev/null

    echo ""
    ui_lines 50 "WireGuard Routes"
    ip rule show | grep -E "(${table_name}|${lan_iface})" || echo "No rules found"
    ip route show table "$table_name" 2>/dev/null || echo "$table_name is EMPTY!"
    ui_lines 50
}

#####################################################################
# WG-AUTOCONF ROUTES UNSET
#
# Workflow:
#   1.  Calculate LAN subnet using IP and netmask
#   2.  Remove routing rules and flush routing table
#   3.  Remove firewall forwarding rules between interfaces
#   4.  Commit firewall changes
#
# Params:
#   $1: WireGuard interface name (e.g., wg0, wg_vpn)
#   $2: LAN interface name (e.g., lan, lan2)
#
# Cleanup Operations:
#   - Removes IP policy rules for the LAN subnet
#   - Flushes all routes from the custom routing table
#   - Deletes bidirectional firewall forwarding rules
#   - Cleans up empty lines in firewall configuration
#
# Dependencies:
#   - netmask_to_cidr(): Converts netmask to CIDR notation
#   - ip_to_network(): Calculates network address from IP and CIDR
#
#####################################################################
unset_lan_routes() {

    local wg_iface="$1"
    local lan_iface="$2"

    local table_name="${lan_iface}_vpn_${wg_iface}"

    log "Removing routes for $lan_iface (via $wg_iface)..."

    # -------------------------------------------------------------------
    # 1. CALCULATE LAN SUBNET WITH PROPER CIDR
    # -------------------------------------------------------------------
    # GET LAN CONFIG
    local lan_ip=$(uci get "network.$lan_iface.ipaddr" 2>/dev/null)
    local lan_netmask=$(uci get "network.$lan_iface.netmask" 2>/dev/null)
    local cidr

    if [ -n "$lan_netmask" ]; then

        cidr=$(netmask_to_cidr "$lan_netmask")

    else
        # FAILSAFING /24
        cidr=24

    fi

    local subnet="$(ip_to_network "$lan_ip" "$cidr")/$cidr"

    # -------------------------------------------------------------------
    # 2. CLEAN ROUTING RULES AND ROUTES
    # -------------------------------------------------------------------
    # DEL RULES
    ip rule del from "$subnet" lookup "$table_name" 2>/dev/null
    ip rule del from "$subnet" to "$subnet" lookup main 2>/dev/null

    # FLUSH
    ip route flush table "$table_name" 2>/dev/null

    # -------------------------------------------------------------------
    # 3. REMOVE FIREWALL FORWARDING RULES
    # -------------------------------------------------------------------
    log "Removing firewall forwarding rules..."

    sed -i "/config forwarding/,/option dest '$wg_iface'/{/option src '$lan_iface'/,/option dest '$wg_iface'/d;}" "$FIREWALL_CONF"

    sed -i "/config forwarding/,/option dest '$lan_iface'/{/option src '$wg_iface'/,/option dest '$lan_iface'/d;}" "$FIREWALL_CONF"

    sed -i '/^[[:space:]]*$/N;/^\n$/!P;D' "$FIREWALL_CONF"

    # -------------------------------------------------------------------
    # 4. COMMIT FIREWALL CHANGES
    # -------------------------------------------------------------------
    uci commit firewall 2> /dev/null
    /etc/init.d/firewall reload 2> /dev/null

    success "$lan_iface routes (via $wg_iface) successfully removed!"
}

###########################################################
# ███╗   ███╗ █████╗ ██╗███╗   ██╗
# ████╗ ████║██╔══██╗██║████╗  ██║
# ██╔████╔██║███████║██║██╔██╗ ██║
# ██║╚██╔╝██║██╔══██║██║██║╚██╗██║
# ██║ ╚═╝ ██║██║  ██║██║██║ ╚████║
# ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝
###########################################################

[ $# -eq 0 ] && show_usage

case "$1" in
    list)
        list_configs
        ;;
    list-full)
        list_configs_full
        ;;
    setup)

        if [ "$2" = "--advanced" ]; then
            advanced_mode=1
            conf_name="$3"
        elif [ "$3" = "--advanced" ]; then
            advanced_mode=1
            conf_name="$2"
        elif [ -n "$2" ] && [ "$2" != "--advanced" ]; then
            conf_name="$2"
        fi

        if [ $advanced_mode -eq 1 ]; then
            setup_wireguard_advanced "$conf_name"
        elif [ -n "$conf_name" ]; then
            setup_wireguard "$conf_name"
        else
            show_usage
        fi
        ;;
    up)
        [ -n "$2" ] && activate_interface "$2" || show_usage
        ;;
    down)
        [ -n "$2" ] && deactivate_interface "$2" || show_usage
        ;;
    remove)
        [ -n "$2" ] && remove_wireguard "$2" || show_usage
        ;;
    status)
        show_status "$2"
        ;;
    test)
        [ -n "$2" ] && test_config "$2" || show_usage
        ;;
    clean)
        clean_wireguard_interactive "$2"
        ;;
    nuke-all)
        nuke_all
        ;;
    cleanup)
        case "$2" in
            emergency)
                cleanup_emergency
                ;;
            *)
                cleanup_all "$2"
                ;;
        esac
        ;;
    backups)
        case "$2" in
            show)
                show_backups
                ;;
            restore)
                restore_backups "$3"
                ;;
            *)
                show_usage
                ;;
        esac
        ;;
    routes)
        case "$2" in
            set)
                configure_routes "set" "$3" "$4"
                ;;
            unset)
                configure_routes "unset" "$3" "$4"
                ;;
            show)
                show_routes "$3"
                ;;
            *)
                show_usage
                ;;
        esac
        ;;
    *)
        show_usage
        ;;
esac
