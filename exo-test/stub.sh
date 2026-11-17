#!/bin/bash
# Generic command stub for Exocortical A/B receipts.
# Logs "name args..." to $STUBLOG. Never performs the real action.
# Canned behaviour per command; per-case control via env vars.
name=$(basename "$0")
echo "$name $*" >> "$STUBLOG"
san=$(echo "$name" | tr -c 'a-zA-Z0-9' '_')
eval "defrc=\${STUBRC_${san}:-0}"

case "$name" in
  sudo)
    if [ "$1" = "tee" ]; then cat; exit 0; fi
    if [ "$1" = "-u" ]; then echo '<html><head></head><body>stub-dom</body></html>'; exit 0; fi
    if [ "$1" = "bash" ]; then cat > /dev/null; exit 0; fi
    for a in "$@"; do
      if [ "$a" = "journalctl" ] && [ -n "$JOURNAL_FILE" ]; then cat "$JOURNAL_FILE"; exit 0; fi
      if [ "$a" = "status" ] && [ "$3" = "apache2" ]; then
        echo "Unit apache2.service could not be found." >&2; exit 4
      fi
    done
    exit 0
    ;;
  date)
    case "$*" in
      *%Y%m%d%H%M%S*) echo 20261107210000 ;;
      *) echo "Sat Nov  7 21:00:00 CST 2026" ;;
    esac
    exit 0
    ;;
  hostname)
    case "$*" in
      *-I*) echo "192.168.86.50 10.53.4.1" ;;
      *) echo "aletheia-core" ;;
    esac
    exit 0
    ;;
  ping)
    echo "PING $3 (192.168.86.99) 56(84) bytes of data."
    echo "64 bytes from 192.168.86.99: icmp_seq=1 ttl=64 time=0.42 ms"
    echo ""
    echo "--- $3 ping statistics ---"
    echo "1 packets transmitted, 1 received, 0% packet loss, time 0ms"
    exit 0
    ;;
  avahi-browse)
    echo "+   eth0 IPv4 Exocortex Node Aletheia        _exocortex_exocortical._tcp"
    echo "+   eth0 IPv4 Exocortex Node Logos           _exocortex_exocortical._tcp"
    exit "$defrc"
    ;;
  systemctl)
    if [ "$1" = "is-active" ]; then exit "${OLLAMA_ACTIVE_RC:-1}"; fi
    exit 0
    ;;
  ollama)
    case "$1" in
      list) if [ -n "$OLLAMA_LIST_FILE" ]; then cat "$OLLAMA_LIST_FILE"; else
              printf 'NAME           ID              SIZE    MODIFIED\nqwen3.5:27b    abc123          17 GB   2 days ago\n'; fi ;;
      show) if [ -n "$OLLAMA_SHOW_FILE" ]; then cat "$OLLAMA_SHOW_FILE"; else echo "num_ctx 8192"; fi ;;
      -v) echo "ollama version is 0.40.11" ;;
    esac
    exit 0
    ;;
  curl)
    out=""; url=""; prev=""
    for a in "$@"; do
      if [ "$prev" = "-o" ]; then out="$a"; fi
      case "$a" in http*) url="$a" ;; esac
      prev="$a"
    done
    rc="$defrc"
    if [ -n "$CURL_FAIL_PATTERN" ] && [ -n "$url" ]; then
      case "$url" in *"$CURL_FAIL_PATTERN"*) rc="${CURL_FAIL_RC:-22}" ;; esac
    fi
    if [ "$rc" = "0" ]; then
      if [ -n "$out" ]; then
        target="$out"
        case "$target" in /*) [ -n "$STUBROOT" ] && target="$STUBROOT$target" ;; esac
        mkdir -p "$(dirname "$target")" 2>/dev/null
        printf 'STUB-DOWNLOAD\n' > "$target" 2>/dev/null || exit 23
      else
        printf 'STUB-BODY\n'
      fi
    fi
    exit "$rc"
    ;;
  wget)
    case "$*" in
      *" -O -"*|*"-qO -"*) : ;;
      *)
        # create the downloaded file in cwd, like wget does
        for a in "$@"; do case "$a" in http*) f=$(basename "$a") ;; esac; done
        [ -n "$f" ] && printf 'STUB-DOWNLOAD\n' > "$f" 2>/dev/null
        ;;
    esac
    exit "$defrc"
    ;;
  git)
    if [ "$1" = "status" ]; then printf 'On branch exo\nnothing to commit, working tree clean\n'; fi
    exit 0
    ;;
  squidclient)
    case "$*" in
      *mgr:5min*)
        printf 'client_http.requests = 1200/sec\nclient_http.hits = 900/sec\nclient_http.kbytes_in = 55/sec\nclient_http.kbytes_out = 77/sec\nother = 1\n' ;;
      *mgr:active_requests*)
        printf 'conn 1\nconn 2\nconn 3\nconn 4\nconn 5\nconn 6\n' ;;
      *mgr:mem*)
        printf 'Memory usage summary:\nTotal memory: 2048 MB\nFree memory: 512 MB\nCached objects: 42\nSomething else\nTotal swap: 100 MB\nExtra line\n' ;;
    esac
    exit 0
    ;;
  top)
    printf 'top - 21:00:00 up 1 day\nMiB Mem :  32768.0 total\nMiB Swap:   8192.0 total,   8192.0 free,      0.0 used\n'
    exit 0
    ;;
  du)
    printf '1.2G\t%s\n' "${@: -1}"
    exit 0
    ;;
  lspci)
    echo "03:00.0 VGA compatible controller [0300]: Advanced Micro Devices, Inc. [AMD/ATI] [1002:1900]"
    exit 0
    ;;
  ufw)
    if [ "$1" = "status" ]; then
      if [ "${UFW_ACTIVE:-0}" = "1" ]; then echo "Status: active"; else echo "Status: inactive"; fi
    fi
    exit 0
    ;;
  exportfs)
    if [ "$1" = "-v" ]; then printf '/srv/mirrorborn/models\n\t\t192.168.86.0/24(sync,w)\n'; fi
    exit 0
    ;;
  mountpoint)
    exit "${MOUNTPOINT_RC:-1}"
    ;;
  ip)
    if [ "$1 $2" = "link show" ]; then exit "${IP_RC:-0}"; fi
    exit 0
    ;;
  go)
    if [ $# -eq 0 ]; then printf 'Go is a tool for managing Go source code.\nusage...\n'; fi
    exit 0
    ;;
  gpg)
    cat > /dev/null; printf 'STUB-DEARMORED'
    exit 0
    ;;
  sleep) exit 0 ;;
  *) exit "$defrc" ;;
esac
