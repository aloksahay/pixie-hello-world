import { test } from "node:test";
import assert from "node:assert/strict";
import { ethers } from "ethers";

import {
  parseClaimURI,
  claimDigest,
  manualDigest,
  recoverSigner,
  type ClaimCore,
} from "../src/claim.ts";

// The firmware's fixed cross-check vector (poap_debugDump): nonce[i] = (i%16)*0x11.
const FIXED_NONCE =
  "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";

// A valid EIP-55 checksummed placeholder contract address.
const CONTRACT = ethers.getAddress("0x00112233445566778899aabbccddeeff00112233");

const core: ClaimCore = {
  chainId: 84532,
  contract: CONTRACT,
  eventId: 1n,
  nonce: FIXED_NONCE,
};

// Proves the device's hand-rolled keccak encoding equals canonical EIP-712.
// The Foundry test `testKnownDigestVector` proves the contract also equals
// canonical, so: device == canonical == contract.
test("firmware digest encoding matches canonical EIP-712", () => {
  assert.equal(manualDigest(core), claimDigest(core));
});

test("digest changes with each field", () => {
  const base = claimDigest(core);
  assert.notEqual(base, claimDigest({ ...core, eventId: 2n }));
  assert.notEqual(base, claimDigest({ ...core, chainId: 1 }));
  assert.notEqual(
    base,
    claimDigest({ ...core, contract: ethers.ZeroAddress })
  );
  assert.notEqual(base, claimDigest({ ...core, nonce: ethers.ZeroHash }));
});

// A signature produced the way the device does it (r||s||v, v in {27,28}) must
// recover to the signer via the same path the contract uses.
test("recoverSigner round-trips a device-style signature", async () => {
  const wallet = ethers.Wallet.createRandom();
  const digest = claimDigest(core);
  const sig = wallet.signingKey.sign(digest); // yields r, s, v(27/28)
  const packed = ethers.concat([sig.r, sig.s, ethers.toBeHex(sig.v, 1)]);
  assert.equal(ethers.dataLength(packed), 65);

  const recovered = recoverSigner({ ...core, sig: packed });
  assert.equal(recovered, wallet.address);
});

test("parseClaimURI round-trips", () => {
  const uri = `fireflypoap://claim?c=84532&a=${core.contract}&e=1&n=${FIXED_NONCE}&s=0x${"11".repeat(65)}`;
  const c = parseClaimURI(uri);
  assert.equal(c.chainId, 84532);
  assert.equal(c.eventId, 1n);
  assert.equal(c.nonce, FIXED_NONCE);
  assert.equal(ethers.getAddress(c.contract), core.contract);
});

test("parseClaimURI rejects malformed input", () => {
  assert.throws(() => parseClaimURI("fireflypoap://claim?c=1&a=0x00"));
  assert.throws(() =>
    parseClaimURI(`fireflypoap://claim?c=1&a=${core.contract}&e=1&n=0x12&s=0x34`)
  );
});
