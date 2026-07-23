#!/usr/bin/env bash
#
# Deploy FireflyPOAP to Base Sepolia and patch main/poap-config.h with the
# resulting address. Reads config from the environment (or contracts/.env).
#
# Required:
#   PRIVATE_KEY            deployer key (becomes contract owner), funded on Base Sepolia
#   POAP_ATTESTOR          the device's attestor address (from the monitor log:
#                          "poap: attestor address = 0x..")
#   BASE_SEPOLIA_RPC_URL   e.g. https://sepolia.base.org or an Alchemy/Infura URL
# Optional:
#   POAP_EVENT_ID (default 1), POAP_BASE_URI, POAP_MAX_SUPPLY, POAP_ALLOW_MULTIPLE
#   ETHERSCAN_API_KEY      set to also verify on Basescan
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT/contracts"
[ -f .env ] && set -a && . ./.env && set +a

: "${PRIVATE_KEY:?set PRIVATE_KEY}"
: "${POAP_ATTESTOR:?set POAP_ATTESTOR (device address from the monitor log)}"
: "${BASE_SEPOLIA_RPC_URL:?set BASE_SEPOLIA_RPC_URL}"
export POAP_EVENT_ID="${POAP_EVENT_ID:-1}"

VERIFY=""
[ -n "${ETHERSCAN_API_KEY:-}" ] && VERIFY="--verify"

echo "==> deploying FireflyPOAP to Base Sepolia (attestor=$POAP_ATTESTOR, event=$POAP_EVENT_ID)"
forge script script/Deploy.s.sol --rpc-url base_sepolia --broadcast $VERIFY

CONTRACT="$(node -e '
  const fs=require("fs");
  const j=JSON.parse(fs.readFileSync("broadcast/Deploy.s.sol/84532/run-latest.json","utf8"));
  const tx=j.transactions.find(t=>t.contractName==="FireflyPOAP");
  process.stdout.write(tx.contractAddress);
')"
echo "==> deployed at $CONTRACT"

node "$ROOT/scripts/patch-config.mjs" --address "$CONTRACT" --chain 84532 --event "$POAP_EVENT_ID"

echo
echo "Next:"
echo "  1. Rebuild + flash the device:   idf.py build flash monitor"
echo "  2. Point the backend at it:      set CONTRACT_ADDRESS=$CONTRACT in backend/.env"
echo "  3. Fund the relayer, then:       cd backend && npm start  ->  open /scan.html"
