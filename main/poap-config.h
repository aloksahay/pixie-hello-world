#ifndef __POAP_CONFIG_H__
#define __POAP_CONFIG_H__

// Compile-time configuration for the Firefly POAP kiosk. These MUST match the
// deployed FireflyPOAP contract (they feed the EIP-712 domain and Claim).

// EIP-712 domain
#define POAP_DOMAIN_NAME     "FireflyPOAP"
#define POAP_DOMAIN_VERSION  "1"

// Chain the contract is deployed on. Base Sepolia = 84532.
#define POAP_CHAIN_ID        84532ULL

// Event id issued by this kiosk; must equal the contract's `eventId`.
#define POAP_EVENT_ID        1ULL

// Deployed FireflyPOAP address (EIP-712 verifyingContract), 20 bytes big-endian.
// REPLACE the zeros with the real address after `forge script Deploy`.
#define POAP_CONTRACT_ADDRESS { \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// Custom URL scheme carried by the claim QR (lets the phone app deep-link).
#define POAP_CLAIM_SCHEME    "fireflypoap://claim"

// Text shown on the idle kiosk screen.
#define POAP_EVENT_TITLE     "Firefly Meetup"

#endif /* __POAP_CONFIG_H__ */
