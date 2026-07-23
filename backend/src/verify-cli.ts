import { parseClaimURI, claimDigest, recoverSigner } from "./claim.js";

// Usage: npm run verify -- "fireflypoap://claim?c=...&a=0x...&e=...&n=0x...&s=0x..."
const uri = process.argv[2];
if (!uri) {
  console.error('usage: npm run verify -- "<fireflypoap://claim?...>"');
  process.exit(1);
}

const claim = parseClaimURI(uri);
console.log("chainId:  ", claim.chainId);
console.log("contract: ", claim.contract);
console.log("eventId:  ", claim.eventId.toString());
console.log("nonce:    ", claim.nonce);
console.log("digest:   ", claimDigest(claim));
console.log("signer:   ", recoverSigner(claim));
console.log("\nThis 'signer' must equal the Firefly attestor address and the");
console.log("contract's attestor() for the mint to succeed.");
