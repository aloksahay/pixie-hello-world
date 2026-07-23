#ifndef __POAP_CLAIM_H__
#define __POAP_CLAIM_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "firefly-ecc.h"
#include "firefly-eth.h"

// Upper bound on the claim URI length (scheme + hex-encoded fields + NUL).
#define POAP_CLAIM_URI_MAX   320

/**
 *  Load the device signing key.
 *
 *  This is the single choke point for key access. Today it wraps the SDK DEV
 *  key (`ffx_deviceTestPrivkey`, account 0), which is NOT secure. A production
 *  hardware-protected key (secure element, or a secp256k1 key certified by the
 *  device RSA attestation) drops in here without touching callers.
 */
bool poap_getSigningKey(FfxEcPrivkey *privkeyOut);

/**
 *  Populate %%out%% with the device's EIP-55 checksummed Ethereum address. This
 *  is the `attestor` the FireflyPOAP contract must trust (`setAttestor`).
 */
bool poap_attestorAddress(FfxChecksumAddress *out);

/**
 *  Fill %%nonce%% (32 bytes) with cryptographic randomness.
 */
void poap_newNonce(uint8_t nonce[32]);

/**
 *  Compute the EIP-712 `Claim(uint256 eventId,bytes32 nonce)` digest, using the
 *  domain configured in poap-config.h.
 */
void poap_buildDigest(FfxEcDigest *digestOut, uint64_t eventId,
  const uint8_t nonce[32]);

/**
 *  Sign %%digest%% with the device key. %%sigOut%% is 65 bytes: r || s || v,
 *  with v in {27, 28} (directly consumable by Solidity ECDSA.recover).
 */
bool poap_sign(FfxEcSignature *sigOut, const FfxEcDigest *digest);

/**
 *  Generate a fresh single-use claim and render it as the QR URI string:
 *
 *    fireflypoap://claim?c=<chainId>&a=0x<contract>&e=<eventId>&n=0x<nonce>&s=0x<sig>
 *
 *  If %%nonceOut%% is non-NULL it receives the 32-byte nonce. Returns the URI
 *  length (excluding NUL), or 0 on failure.
 */
size_t poap_makeClaimURI(char *uriOut, size_t uriLen, uint8_t nonceOut[32]);

/**
 *  Log the attestor address and a fixed-vector (eventId, nonce) digest +
 *  signature to the console, for the device<->contract cross-check.
 */
void poap_debugDump(void);

#endif /* __POAP_CLAIM_H__ */
