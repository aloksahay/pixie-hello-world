# Firefly POAP — Live Testnet Runbook (Base Sepolia)

The full pipeline is already proven locally against Anvil:

```bash
./scripts/e2e-local.sh
# deploy -> attest -> relay -> mint -> ownership -> replay/forgery rejected
```

This runbook is the same flow on a public testnet with the real device. The only
manual inputs are your funded keys, an RPC URL, and the Firefly on USB.

## Prerequisites
- Foundry (`forge`, `cast`, `anvil`), ESP-IDF v6.0.1 sourced, Node 20+.
- A deployer key funded with Base Sepolia ETH ([faucet](https://docs.base.org/tools/network-faucets)).
- A relayer key funded with Base Sepolia ETH (pays gas for mints).
- An RPC URL (`https://sepolia.base.org` or Alchemy/Infura).

## Step 1 — Read the device's attestor address
Flash the current firmware and read the address it derives (stable per device):

```bash
. $HOME/esp/esp-idf/export.sh
idf.py build flash monitor
# look for:  poap: attestor address = 0x....
```

Copy that address — it's `POAP_ATTESTOR`. (Requires a provisioned Firefly; the
dev key is derived from the device's own attestation, so it cannot be computed
off-device.)

## Step 2 — Deploy the contract + patch firmware config
```bash
export PRIVATE_KEY=0x...            # deployer (owner), funded
export POAP_ATTESTOR=0x...          # from Step 1
export BASE_SEPOLIA_RPC_URL=https://sepolia.base.org
export POAP_EVENT_ID=1
# optional: export ETHERSCAN_API_KEY=... to verify on Basescan

./scripts/deploy-testnet.sh
```

This deploys `FireflyPOAP` (with your device as the trusted attestor) and rewrites
`main/poap-config.h` with the contract address, chain id, and event id.

## Step 3 — Reflash so the device signs for this domain
```bash
idf.py build flash monitor
```
The EIP-712 domain includes the contract address, so the device must be reflashed
after deploy. Press **OK** on the device — it shows a QR claim.

## Step 4 — Run the backend relayer
```bash
cd backend
cp .env.example .env    # then edit:
#   CONTRACT_ADDRESS=<deployed address>
#   RPC_URL=https://sepolia.base.org
#   RELAYER_PRIVATE_KEY=0x<funded relayer>
#   CHAIN_ID=84532  EVENT_ID=1
npm install && npm start
```

## Step 5 — Claim end-to-end
- Open `http://localhost:8787/scan.html`, scan the Firefly's QR (or paste the URI).
- **Verify attestation** → the recovered `signer` must equal your device address.
- Enter a recipient (or leave blank to mint to the relayer) → **Mint POAP**.
- Confirm the tx on [Basescan](https://sepolia.basescan.org) and that `ownerOf`
  is the recipient.

You can also cross-check a scanned claim without the browser:
```bash
cd backend && npm run verify -- "fireflypoap://claim?c=84532&a=0x...&e=1&n=0x...&s=0x..."
```

## Negative checks (should all fail)
- Rescanning the same QR → `nonce already used` (409).
- A hand-edited/forged claim → `bad attestor` (400).
- A second mint to the same wallet → `AlreadyHolder` (unless `POAP_ALLOW_MULTIPLE=true`).

## Notes
- If you redeploy, either set `POAP_ATTESTOR` again at deploy time or call
  `setAttestor` (owner-only) with the device address via `cast`.
- Signing key is still the SDK **dev key** (`poap_getSigningKey`); swapping in a
  hardware-protected key is the "Production hardware key" phase.
