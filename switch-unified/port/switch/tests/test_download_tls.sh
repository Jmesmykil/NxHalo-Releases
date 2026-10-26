#!/bin/sh
# Native HTTPS smoke + deliberately untrusted local server. No trust overrides.
set -eu
cd "$(dirname "$0")/.."
fixture=$(mktemp -d /Volumes/Media/nxhalo-takeover-20260929/download-tls-XXXXXX)
server_pid=
openssl_bin=${OPENSSL_BIN:-/opt/homebrew/opt/openssl@3/bin/openssl}
cleanup() {
    if [ -n "$server_pid" ]; then kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true; fi
    rm -f "$fixture/key.pem" "$fixture/cert.pem" "$fixture/server.log"
    rmdir "$fixture"
}
trap cleanup EXIT INT TERM
"$openssl_bin" req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
    -keyout "$fixture/key.pem" -out "$fixture/cert.pem" 2>/dev/null
# Bind only loopback; failure to bind must fail the test rather than use a peer server.
"$openssl_bin" s_server -accept 127.0.0.1:19443 -cert "$fixture/cert.pem" \
    -key "$fixture/key.pem" -www >"$fixture/server.log" 2>&1 &
server_pid=$!
sleep 1
kill -0 "$server_pid"
tests/test_download https://example.com https://localhost:19443/
