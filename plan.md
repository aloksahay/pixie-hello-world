# Firefly POAP — Hardware-Attested NFT Minting

## Context

Emulate the POAP ("Proof of Attendance Protocol") experience — an attendee leaves
an event with a commemorative NFT — with one hard security property: **only the
private key inside a physical Firefly (Pixie) device can attest that a POAP may
be minted.** No server, shared secret, or web form can authorize a mint; the
on-chain contract mints only when it sees a signature produced by the Firefly's
key.

The Firefly is an **unattended kiosk**. An attendee presses **OK**; the device
issues a fresh, single-use, EIP-712-signed claim rendered as a **QR code** (the
device has a screen but *no camera*, so attestations flow *out* as QR). The claim
is scanned and submitted to an ERC-721 contract that verifies the Firefly's
signature via `ecrecover` and mints the POAP.

### Scope for this build
- **Firmware** — Firefly kiosk app (this repo, `main/`).
- **Contract** — `FireflyPOAP` ERC-721 (Foundry, `contracts/`).
- **Backend** — relayer + metadata + a browser scan page that stands in for the
  phone app for now (`backend/`).
- **iOS app — deferred.** The backend's scan page exercises the same
  scan → mint path in the meantime.

Locked decisions: single Firefly = event attestor; attendee presses OK (no
operator); secp256k1 / EIP-712 verified on-chain with `ecrecover`; QR transport
(BLE deferred); testnet demo.

### Security model & honest limitations
- Trust anchor is the Firefly's secp256k1 key. This build uses the SDK **dev key**
  `ffx_deviceTestPrivkey(&pk, 0)` (`components/firefly-hollows/include/firefly-hollows.h:450`)
  — explicitly *not secure*, fine for testnet. Key access is isolated behind one
  function (`poap_getSigningKey`) so a hardware-protected key drops in later. The
  device genuineness proof (`attestProof`, RSA DS peripheral) is used *out of band*
  to authorize the device address on the contract, not on the hot path.
- An unattended kiosk can be spammed (anyone can press OK for a valid code). Supply
  is bounded **in the contract** (one POAP per wallet + optional max supply), not
  the device. The device adds a soft debounce only.
- Claims are **bearer** tokens (no recipient bound, since the device can't read an
  address without camera/BLE): single-use nonce, first to mint wins. Matches classic
  POAP mint-links; acceptable for the demo. BLE address-bound claims = upgrade path.

## Shared contract: EIP-712 `Claim`
Identical on device, contract, and backend:
- Domain: `{ name:"FireflyPOAP", version:"1", chainId:<testnet>, verifyingContract:<addr> }`
- Struct: `Claim(uint256 eventId,bytes32 nonce)`
- Digest: `keccak256(0x1901 ‖ domainSeparator ‖ keccak256(typeHash ‖ eventId ‖ nonce))`
- Claim QR payload: `fireflypoap://claim?c=<chainId>&a=0x<contract>&e=<eventId>&n=0x<nonce>&s=0x<sig>`

---

## Component 1 — Contract `FireflyPOAP` (`contracts/`)
Foundry, OpenZeppelin `ERC721` + `EIP712` + `ECDSA`.
- State: `attestor`, `eventId`, `usedNonce` map, `nextTokenId`, `baseArtURI`,
  `allowMultiplePerWallet`, optional `maxSupply`.
- `mint(uint256 eventId_, bytes32 nonce, bytes signature)`: check event → recover
  signer, `require(signer == attestor)` → `require(!usedNonce[nonce])` →
  one-per-wallet → mark used → `_safeMint(msg.sender, nextTokenId++)`.
- `setAttestor` (owner), `tokenURI` → metadata endpoint.
- Foundry tests: valid mint, replay, wrong signer, wrong event, double-mint.
- Deploy script (Base Sepolia).

## Component 2 — Firmware kiosk (`main/`)
Follows the existing Panel pattern (`main/panel-menu.c`).
- `main/main.c` — push `pushPanelKiosk()`, keep background + `ffx_init`.
- `main/poap-config.h` — `POAP_CHAIN_ID`, `POAP_CONTRACT`, `POAP_EVENT_ID`, domain.
- `main/poap-claim.c/.h` — `poap_getSigningKey` (wraps `ffx_deviceTestPrivkey`),
  `poap_attestorAddress`, `poap_newNonce` (`esp_fill_random`), `poap_buildDigest`
  (EIP-712 via `ffx_hash_keccak256`), `poap_sign` (`ffx_ec_signDigest`, normalize
  v→27/28), `poap_encodeClaimURI`.
- `main/panel-kiosk.c/.h` — idle screen; OK → nonce/digest/sign/encode → QR panel;
  soft debounce.
- `main/panel-claim-qr.c/.h` — full-screen QR (`ffx_scene_createQRData`, module size
  to fill 240×240), green "issued" accent, auto-pop on timeout/Cancel.
- Update `main/CMakeLists.txt`; retire `panel-menu.*`/`panel-text.*`.

## Component 3 — Backend (`backend/`)
Node + TypeScript + ethers.
- `/mint` — relayer: accepts a parsed claim + recipient, submits the mint tx from a
  funded wallet, returns tx hash. (Stands in for the phone submitting the tx.)
- `/metadata/:id` — tokenURI JSON (name, description, image) for the event POAP.
- `/verify` — recompute the EIP-712 digest from a claim and return the recovered
  signer; used for the device↔contract cross-check.
- `public/scan.html` — minimal webcam QR scanner that parses `fireflypoap://claim`
  and POSTs to `/mint`. Temporary stand-in for the iOS app.

---

## Build order & verification
1. **Contract** — write, `forge test`, deploy to Base Sepolia, record address/chainId.
2. **Firmware** — fill `poap-config.h`, build & flash
   (`. $HOME/esp/esp-idf/export.sh && idf.py set-target esp32c3 build flash monitor`).
   Log `poap_attestorAddress()`; `setAttestor` to it.
3. **Cross-check** — device signs a *fixed* `(eventId,nonce)`; `/verify` (or a Foundry
   test) recomputes the digest and asserts `ecrecover == attestor`. Do not proceed
   until it matches — validates keccak/domain encoding + r‖s‖v layout.
4. **Backend end-to-end** — open `scan.html`, scan the Firefly QR, confirm `/mint`
   lands the tx and the POAP is minted.
5. **Negative tests** — replayed QR fails (`used`), non-Firefly signature fails
   (`bad attestor`), second mint per wallet fails.

## Deferred
- iOS app (native, baked-in wallet) — **separate repo**; backend scan page stands
  in for now.
- BLE address-bound claims via the FSP CBOR channel (`task-ble.c`).
- Claim expiry once a trusted time source exists (nonce-only single-use today).
- Production key hardening — see below.

---

## Production hardening (deferred)

The contract's `ecrecover == attestor` gate is already production-grade. The gap is
entirely **device-side**, and it has two independent parts that defend against two
different attackers. **Both A and B are required for production.**

Today the attestor key comes from `poap_getSigningKey` → `ffx_deviceTestPrivkey(.,0)`
(`device-info.c:603`), which the SDK explicitly labels *"Testing ONLY … not secure"*
(`firefly-hollows.h:443`). It is device-unique and hardware-rooted in its
*derivation* (DS peripheral + eFuse), which is fine — but:
- the raw secp256k1 private key is **materialized in firmware RAM** every time we
  sign (extractable via bugs, debug ports, or modified firmware);
- a debug flag (`showMnemonic`, `device-info.c:638`) can print the mnemonic;
- the attestor address is **not bound to any genuine-Firefly proof** — `setAttestor`
  trusts a bare address read off the serial log, which anyone (dev board, emulator,
  random keypair) can produce.

### A — Non-extractable signing key
*Stops theft of a genuine device's key (remote minting without the device).*
- **Preferred:** secure-element secp256k1 signer (device advertises
  `FfxDeviceOptionSecureElement`) so the key never enters firmware RAM and
  `ecrecover` stays cheap. **Blocked** on SDK exposing a production secp256k1
  signer (only the dev key exists today).
- **Reachable now (config, not code):** provision **Secure Boot v2 + Flash
  Encryption** (already enabled in `sdkconfig`). Only signed firmware runs and
  flash is encrypted at rest, closing "malicious firmware extracts the key" and
  "dump the flash." Residual risk: a bug in our own signed firmware, or physical
  eFuse/DS attacks.
- **Strongest but awkward:** sign with the DS-peripheral RSA-3072 key
  (`ffx_deviceAttest`, key inaccessible even to firmware) — but on-chain RSA
  verify (modexp + large calldata) is heavy for an ERC-721 mint.

### B — Genuineness binding of the signer
*Stops a fake attestor being trusted (impersonating a Firefly).*
- Before trusting an address, have the device's **hardware RSA key** sign
  "secp256k1 address X is my POAP attestor" and present `attestProof` (factory
  proof chaining to Firefly's manufacturing CA).
- Verify both at `setAttestor` time (off-chain, or on-chain) so X is trusted only
  if a *provably genuine* Firefly vouched for it. Closes the "bare address" gap.

### Realistic production ladder
1. **B + provision Secure Boot / Flash Encryption** — mostly reachable in this
   repo; keeps the cheap secp256k1/ecrecover design and closes both attacker
   classes to a strong degree.
2. **Full A (secure-element secp256k1)** — when the SDK exposes it, for
   hardware-guaranteed non-extractability.

All key access stays behind the single `poap_getSigningKey` choke point, so the
swap is localized.
