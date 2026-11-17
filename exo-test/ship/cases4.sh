# Part 4 cases: exo-agentmail (stub HTTP server), exo-console (golden export),
# ports (URL matrix). Sourced after driver.sh. Appends to RESULTS.txt.

# --- exo-agentmail ---
fresh exo-agentmail; : > $C/stdin
mkdir -p $C/port/root/etc
printf 'email=test@x.io\nname=Test User\n' > $C/port/root/etc/mirrorborn.phext
cat > $C/server.py <<'PYEOF'
import http.server, json, sys
log = open(sys.argv[2], 'a')
class H(http.server.BaseHTTPRequestHandler):
    def _r(self):
        n = int(self.headers.get('Content-Length') or 0)
        body = self.rfile.read(n).decode() if n else ''
        log.write(json.dumps({"m": self.command, "p": self.path,
          "auth": self.headers.get('Authorization'), "body": body}) + "\n"); log.flush()
        b = b'{"id":"inbox_1","email":"test@x.io"}' if self.command == 'GET' else b'{"id":"msg_1"}'
        self.send_response(200); self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)
    do_GET = _r; do_POST = _r
    def log_message(self, *a): pass
http.server.HTTPServer(('127.0.0.1', int(sys.argv[1])), H).serve_forever()
PYEOF
: > $C/requests.log
python3 $C/server.py 18745 $C/requests.log & SRVPID=$!
sleep 1
PORTENV=(AGENTMAIL_BASE_URL=http://127.0.0.1:18745/v0 AGENTMAIL_API_KEY=testkey123)
run_port exo-agentmail
echo "=== exo-agentmail (no args) === rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
cat $C/port.out >> $T/RESULTS.txt
run_port exo-agentmail --recipient a@b.c --subject Hi --message "Body text"
echo "=== exo-agentmail (send) === rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
cat $C/port.out >> $T/RESULTS.txt
run_port exo-agentmail --help
echo "=== exo-agentmail (--help) === rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
head -2 $C/port.out >> $T/RESULTS.txt
kill $SRVPID 2>/dev/null
echo "--- captured requests:" >> $T/RESULTS.txt; cat $C/requests.log >> $T/RESULTS.txt
PORTENV=()
echo "orig exo-agentmail.py on this node: $(python3 -c 'import agentmail' 2>&1 | tail -1)" >> $T/RESULTS.txt

# --- exo-console ---
fresh exo-console
NOW=$(date -u -d "2026-11-07 21:00:00" +%s)
PORTENV=(EXO_CONSOLE_NOW=$NOW)
printf '' > $C/stdin
run_port exo-console log --category dev --persona Emi "Built the thing"
run_port exo-console log --category ops "Shipped it"
cp $C/port.out $C/log.out
run_port exo-console list
echo "=== exo-console list ===" >> $T/RESULTS.txt; cat $C/port.out >> $T/RESULTS.txt
run_port exo-console distribution
echo "=== exo-console distribution ===" >> $T/RESULTS.txt; cat $C/port.out >> $T/RESULTS.txt
run_port exo-console tessera
echo "=== exo-console tessera === $(cat $C/port.out)" >> $T/RESULTS.txt
run_port exo-console slot
echo "=== exo-console slot === $(cat $C/port.out)" >> $T/RESULTS.txt
run_port exo-console week
echo "=== exo-console week ===" >> $T/RESULTS.txt; cat $C/port.out >> $T/RESULTS.txt
run_port exo-console export
cp $C/port.out $C/export.md
TESSERA=$(( ( $NOW - $(date -u -d "2023-03-26" +%s) + 86399 ) / 86400 ))
cat > $C/golden.md <<EOF
# Exocortex 2130 - Nightly Log

**Date:** 2026-11-07
**Tessera Day:** $TESSERA

## Summary

- **Total Logs:** 2
- **DEV:** 1
- **OPS:** 1
- **SALES:** 0

## Logs

### 15:00 [DEV]

Built the thing

*Personas: Emi*

### 15:00 [OPS]

Shipped it

---
*Build the future from 2130*
EOF
if diff $C/golden.md $C/export.md > $C/export.diff; then echo "exo-console export: GOLDEN MATCH" >> $T/RESULTS.txt; else echo "exo-console export: DIFF" >> $T/RESULTS.txt; cat $C/export.diff >> $T/RESULTS.txt; fi
node -e "JSON.parse(require('fs').readFileSync(process.argv[1],'utf8')); console.log('store parses under node (V8): OK')" $C/port/home/.exocortex-2130.json >> $T/RESULTS.txt
PORTENV=()

# --- ports ---
fresh ports; : > $C/stdin
run_port ports list
echo "=== ports list === lines=$(wc -l < $C/port.out) (expect 109: 2 header lines + 6 hosts + 101 personas)" >> $T/RESULTS.txt
grep -E '^(aletheia-core|luma) ' $C/port.out >> $T/RESULTS.txt
grep -E '^(Emi|Jael) ' $C/port.out | head -3 >> $T/RESULTS.txt
exp1=$(python3 -c "import urllib.parse; print('http://aletheia-core:11012/api/v2/select?p=index&c='+urllib.parse.quote_plus('1.1.1/1.1.1/1.1.1')+'&s=')")
run_port ports url aletheia-core 12
echo "ports url 1: got=$(cat $C/port.out) expect=$exp1" >> $T/RESULTS.txt
exp2=$(python3 -c "import urllib.parse; print('http://luma:11101/api/v2/select?p='+urllib.parse.quote_plus('structure')+'&c='+urllib.parse.quote_plus('2.3.4/5.6.7/8.9.10')+'&s='+urllib.parse.quote_plus('hello world'))")
run_port ports url luma 101 structure "2.3.4/5.6.7/8.9.10" "hello world"
echo "ports url 2: got=$(cat $C/port.out) expect=$exp2" >> $T/RESULTS.txt
run_port ports url nope-host 5
echo "ports url bad host: rc=$(cat $C/port.rc) out=$(cat $C/port.out) err=$(cat $C/port.err)" >> $T/RESULTS.txt

echo "PART4 DONE" >> $T/RESULTS.txt
