#include <stdio.h>
#include <string.h>

#include "esp_random.h"

#include "poap-claim.h"
#include "poap-config.h"

#include "firefly-hash.h"
#include "firefly-hollows.h"   // ffx_deviceTestPrivkey, ffx_logData

static const uint8_t contractAddr[20] = POAP_CONTRACT_ADDRESS;

// ---- small helpers ---------------------------------------------------------

static void keccakStr(uint8_t out[32], const char *s) {
    ffx_hash_keccak256(out, (const uint8_t *)s, strlen(s));
}

// Encode a uint64 as a 32-byte big-endian ABI word (high bytes zero).
static void encodeUint(uint8_t out[32], uint64_t v) {
    memset(out, 0, 32);
    for (int i = 0; i < 8; i++) {
        out[31 - i] = (uint8_t)(v >> (8 * i));
    }
}

static size_t toHex(char *out, const uint8_t *data, size_t len) {
    static const char H[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[2 * i] = H[data[i] >> 4];
        out[2 * i + 1] = H[data[i] & 0x0f];
    }
    return len * 2;
}

// Print "tag: 0x<hex>\n" for up to 65 bytes.
static void printHex(const char *tag, const uint8_t *data, size_t len) {
    char hex[2 * 65 + 1];
    if (len > 65) { len = 65; }
    toHex(hex, data, len);
    hex[2 * len] = '\0';
    printf("%s: 0x%s\n", tag, hex);
}

// ---- EIP-712 domain separator (computed once) ------------------------------

static bool domainReady = false;
static uint8_t domainSeparator[32];

static void computeDomainSeparator(void) {
    if (domainReady) { return; }

    // abi.encode(typehash, keccak(name), keccak(version), chainId, verifyingContract)
    uint8_t buf[160];
    keccakStr(&buf[0],
      "EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)");
    keccakStr(&buf[32], POAP_DOMAIN_NAME);
    keccakStr(&buf[64], POAP_DOMAIN_VERSION);
    encodeUint(&buf[96], POAP_CHAIN_ID);
    memset(&buf[128], 0, 12);              // address left-padded to 32 bytes
    memcpy(&buf[140], contractAddr, 20);

    ffx_hash_keccak256(domainSeparator, buf, sizeof(buf));
    domainReady = true;
}

// ---- public API ------------------------------------------------------------

bool poap_getSigningKey(FfxEcPrivkey *privkeyOut) {
    // DEV key — see header. Replace with a hardware-protected key for production.
    return ffx_deviceTestPrivkey(privkeyOut, 0);
}

bool poap_attestorAddress(FfxChecksumAddress *out) {
    FfxEcPrivkey pk;
    if (!poap_getSigningKey(&pk)) { return false; }

    FfxEcPubkey pub;
    bool ok = ffx_ec_computePubkey(&pub, &pk);
    memset(&pk, 0, sizeof(pk));
    if (!ok) { return false; }

    FfxAddress addr = ffx_eth_getAddress(&pub);
    *out = ffx_eth_checksumAddress(&addr);
    return true;
}

void poap_newNonce(uint8_t nonce[32]) {
    esp_fill_random(nonce, 32);
}

void poap_buildDigest(FfxEcDigest *digestOut, uint64_t eventId,
  const uint8_t nonce[32]) {

    computeDomainSeparator();

    // structHash = keccak(abi.encode(CLAIM_TYPEHASH, eventId, nonce))
    uint8_t structBuf[96];
    keccakStr(&structBuf[0], "Claim(uint256 eventId,bytes32 nonce)");
    encodeUint(&structBuf[32], eventId);
    memcpy(&structBuf[64], nonce, 32);

    uint8_t structHash[32];
    ffx_hash_keccak256(structHash, structBuf, sizeof(structBuf));

    // digest = keccak(0x1901 || domainSeparator || structHash)
    uint8_t digestBuf[66];
    digestBuf[0] = 0x19;
    digestBuf[1] = 0x01;
    memcpy(&digestBuf[2], domainSeparator, 32);
    memcpy(&digestBuf[34], structHash, 32);

    ffx_hash_keccak256(digestOut->data, digestBuf, sizeof(digestBuf));
}

bool poap_sign(FfxEcSignature *sigOut, const FfxEcDigest *digest) {
    FfxEcPrivkey pk;
    if (!poap_getSigningKey(&pk)) { return false; }
    bool ok = ffx_ec_signDigest(sigOut, &pk, digest);
    memset(&pk, 0, sizeof(pk));
    return ok;
}

size_t poap_makeClaimURI(char *uriOut, size_t uriLen, uint8_t nonceOut[32]) {
    uint8_t nonce[32];
    poap_newNonce(nonce);
    if (nonceOut) { memcpy(nonceOut, nonce, 32); }

    FfxEcDigest digest;
    poap_buildDigest(&digest, POAP_EVENT_ID, nonce);

    FfxEcSignature sig;
    if (!poap_sign(&sig, &digest)) { return 0; }

    char contractHex[41], nonceHex[65], sigHex[131];
    toHex(contractHex, contractAddr, 20);   contractHex[40] = '\0';
    toHex(nonceHex, nonce, 32);              nonceHex[64] = '\0';
    toHex(sigHex, sig.data, 65);             sigHex[130] = '\0';

    int n = snprintf(uriOut, uriLen,
      "%s?c=%llu&a=0x%s&e=%llu&n=0x%s&s=0x%s",
      POAP_CLAIM_SCHEME, (unsigned long long)POAP_CHAIN_ID, contractHex,
      (unsigned long long)POAP_EVENT_ID, nonceHex, sigHex);

    if (n < 0 || (size_t)n >= uriLen) { return 0; }
    return (size_t)n;
}

void poap_debugDump(void) {
    FfxChecksumAddress addr;
    if (poap_attestorAddress(&addr)) {
        printf("poap: attestor address = %s\n", addr.text);
    } else {
        printf("poap: failed to derive attestor address\n");
    }

    // Fixed cross-check vector: nonce = 0x0011..eeff, eventId = POAP_EVENT_ID.
    uint8_t nonce[32];
    for (int i = 0; i < 32; i++) { nonce[i] = (uint8_t)((i % 16) * 0x11); }

    printHex("poap: xcheck nonce", nonce, 32);

    FfxEcDigest digest;
    poap_buildDigest(&digest, POAP_EVENT_ID, nonce);
    printHex("poap: xcheck digest", digest.data, sizeof(digest.data));

    FfxEcSignature sig;
    if (poap_sign(&sig, &digest)) {
        printHex("poap: xcheck sig", sig.data, sizeof(sig.data));
    }
}
