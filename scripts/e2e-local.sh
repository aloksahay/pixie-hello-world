#!/usr/bin/env bash
#
# Full local end-to-end for Firefly POAP against a local Anvil chain:
#   deploy contract -> run backend relayer -> simulate device claim -> mint ->
#   verify ownership -> replay & forgery negative cases.
#
# Proves the whole pipeline (real contract bytecode, real backend, real txs)
# without needing testnet funds or the physical device. Requires: anvil, forge,
# cast (foundry) and node/npx.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RPC="http://127.0.0.1:8545"
PORT=8787

# Well-known local keys. RELAYER = Anvil account #0 (funded). ATTESTOR simulates
# the Firefly device key. RECIPIENT = Anvil account #1 address (the attendee).
RELAYER_PK="0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
ATTESTOR_PK="0x1111111111111111111111111111111111111111111111111111111111111111"
RECIPIENT="0x70997970C51812dc3A010C7d01b50e0d17dc79C8"

ANVIL_PID=""
BACKEND_PID=""
cleanup() {
  [ -n "$BACKEND_PID" ] && kill "$BACKEND_PID" 2>/dev/null || true
  [ -n "$ANVIL_PID" ] && kill "$ANVIL_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "==> starting anvil"
anvil --silent &
ANVIL_PID=$!
until cast block-number --rpc-url "$RPC" >/dev/null 2>&1; do sleep 0.3; done

ATTESTOR_ADDR="$(cast wallet address "$ATTESTOR_PK")"
echo "==> deploying FireflyPOAP (attestor=$ATTESTOR_ADDR)"
cd "$ROOT/contracts"
CREATE_OUT="$(forge create src/FireflyPOAP.sol:FireflyPOAP \
  --rpc-url "$RPC" --private-key "$RELAYER_PK" --broadcast \
  --constructor-args "Firefly POAP" "FPOAP" "$ATTESTOR_ADDR" 1 \
    "http://localhost:${PORT}/metadata/" 0 false)"
CONTRACT="$(echo "$CREATE_OUT" | grep -i "Deployed to:" | awk '{print $NF}')"
[ -n "$CONTRACT" ] || { echo "deploy failed:"; echo "$CREATE_OUT"; exit 1; }
echo "    contract: $CONTRACT"

echo "==> starting backend relayer"
cd "$ROOT/backend"
RPC_URL="$RPC" CHAIN_ID=31337 EVENT_ID=1 CONTRACT_ADDRESS="$CONTRACT" \
  RELAYER_PRIVATE_KEY="$RELAYER_PK" PORT="$PORT" \
  npx tsx src/server.ts >/tmp/poap-e2e-backend.log 2>&1 &
BACKEND_PID=$!
until curl -sf "http://localhost:${PORT}/health" >/dev/null 2>&1; do sleep 0.3; done

echo "==> running end-to-end client"
ATTESTOR_PK="$ATTESTOR_PK" CONTRACT="$CONTRACT" CHAIN_ID=31337 \
  RECIPIENT="$RECIPIENT" RPC_URL="$RPC" BACKEND="http://localhost:${PORT}" \
  npx tsx src/devclient.ts

echo "==> done"
