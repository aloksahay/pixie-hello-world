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
- iOS app (native, baked-in wallet) — backend scan page stands in for now.
- BLE address-bound claims via the FSP CBOR channel (`task-ble.c`).
- Production key (secure element / RSA-DS attestation); claim expiry once a trusted
  time source exists.
