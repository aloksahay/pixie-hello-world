import { ethers } from "ethers";

export interface Claim {
  chainId: number;
  contract: string;
  eventId: bigint;
  nonce: string; // 0x + 64 hex
  sig: string; // 0x + 130 hex
}

export type ClaimCore = Pick<Claim, "chainId" | "contract" | "eventId" | "nonce">;

/**
 * Parse the QR payload emitted by the Firefly:
 *   fireflypoap://claim?c=<chainId>&a=0x<contract>&e=<eventId>&n=0x<nonce>&s=0x<sig>
 */
export function parseClaimURI(uri: string): Claim {
  const q = uri.includes("?") ? uri.slice(uri.indexOf("?") + 1) : uri;
  const p = new URLSearchParams(q);

  const c = p.get("c");
  const a = p.get("a");
  const e = p.get("e");
  const n = p.get("n");
  const s = p.get("s");
  if (!c || !a || !e || !n || !s) {
    throw new Error("claim URI missing fields (need c, a, e, n, s)");
  }

  const contract = ethers.getAddress(a);
  const nonce = n.toLowerCase();
  const sig = s.toLowerCase();
  if (!ethers.isHexString(nonce, 32)) throw new Error("nonce must be 32 bytes");
  if (!ethers.isHexString(sig, 65)) throw new Error("signature must be 65 bytes");

  return { chainId: Number(c), contract, eventId: BigInt(e), nonce, sig };
}

export function domain(chainId: number, contract: string) {
  return { name: "FireflyPOAP", version: "1", chainId, verifyingContract: contract };
}

const TYPES = {
  Claim: [
    { name: "eventId", type: "uint256" },
    { name: "nonce", type: "bytes32" },
  ],
};

/** Canonical EIP-712 digest (ethers). This is the authority. */
export function claimDigest(c: ClaimCore): string {
  return ethers.TypedDataEncoder.hash(domain(c.chainId, c.contract), TYPES, {
    eventId: c.eventId,
    nonce: c.nonce,
  });
}

/**
 * Byte-for-byte replica of the firmware's poap_buildDigest, built from raw
 * keccak/concat. Used in tests to prove the device's hand-rolled encoding
 * equals canonical EIP-712 (and therefore the contract).
 */
export function manualDigest(c: ClaimCore): string {
  const domainTypehash = ethers.keccak256(
    ethers.toUtf8Bytes(
      "EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)"
    )
  );
  const domainSep = ethers.keccak256(
    ethers.concat([
      domainTypehash,
      ethers.keccak256(ethers.toUtf8Bytes("FireflyPOAP")),
      ethers.keccak256(ethers.toUtf8Bytes("1")),
      ethers.zeroPadValue(ethers.toBeHex(c.chainId), 32),
      ethers.zeroPadValue(c.contract, 32),
    ])
  );
  const claimTypehash = ethers.keccak256(
    ethers.toUtf8Bytes("Claim(uint256 eventId,bytes32 nonce)")
  );
  const structHash = ethers.keccak256(
    ethers.concat([
      claimTypehash,
      ethers.zeroPadValue(ethers.toBeHex(c.eventId), 32),
      c.nonce,
    ])
  );
  return ethers.keccak256(ethers.concat(["0x1901", domainSep, structHash]));
}

/** Recover the signer address of a claim (should equal the Firefly attestor). */
export function recoverSigner(c: Claim): string {
  return ethers.recoverAddress(claimDigest(c), c.sig);
}
