// Minimal FireflyPOAP ABI (only what the backend uses).
export const FIREFLY_POAP_ABI = [
  "function attestor() view returns (address)",
  "function eventId() view returns (uint256)",
  "function usedNonce(bytes32) view returns (bool)",
  "function claimDigest(uint256 eventId, bytes32 nonce) view returns (bytes32)",
  "function recoverClaim(uint256 eventId, bytes32 nonce, bytes signature) view returns (address)",
  "function mint(uint256 eventId, bytes32 nonce, bytes signature) returns (uint256)",
  "function mintTo(address to, uint256 eventId, bytes32 nonce, bytes signature) returns (uint256)",
  "function tokenURI(uint256 tokenId) view returns (string)",
  "event Claimed(address indexed to, uint256 indexed tokenId, bytes32 nonce)",
] as const;
