/**
 * Local end-to-end driver: simulates the Firefly device by signing claims with a
 * key we control, then exercises the real backend relay + contract. Used by
 * scripts/e2e-local.sh. Configured entirely via env.
 */
import { ethers } from "ethers";

const {
  ATTESTOR_PK = "",
  CONTRACT = "",
  CHAIN_ID = "31337",
  RECIPIENT = "",
  RPC_URL = "",
  BACKEND = "http://localhost:8787",
} = process.env;

function fail(msg: string): never {
  console.error("FAIL:", msg);
  process.exit(1);
}
const ok = (msg: string) => console.log("  ok:", msg);

const chainId = Number(CHAIN_ID);
const attestor = new ethers.Wallet(ATTESTOR_PK);
const contract = ethers.getAddress(CONTRACT);
const domain = { name: "FireflyPOAP", version: "1", chainId, verifyingContract: contract };
const types = { Claim: [{ name: "eventId", type: "uint256" }, { name: "nonce", type: "bytes32" }] };

// Reproduce exactly what the firmware emits: EIP-712 digest signed r||s||v (v 27/28).
function makeClaimURI(signer: ethers.BaseWallet, nonce: string): string {
  const digest = ethers.TypedDataEncoder.hash(domain, types, { eventId: 1n, nonce });
  const s = signer.signingKey.sign(digest);
  const sig = ethers.concat([s.r, s.s, ethers.toBeHex(s.v, 1)]);
  return `fireflypoap://claim?c=${chainId}&a=${contract}&e=1&n=${nonce}&s=${sig}`;
}

async function post(pathname: string, body: unknown) {
  const r = await fetch(BACKEND + pathname, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
  return { status: r.status, json: (await r.json()) as any };
}

const nonce = ethers.hexlify(ethers.randomBytes(32));
const uri = makeClaimURI(attestor, nonce);
console.log("simulated device attestor:", attestor.address);

// 1. Attestation verifies to the device address.
{
  const { json } = await post("/verify", { claimUri: uri });
  if (json.signer !== attestor.address) fail(`/verify signer ${json.signer} != ${attestor.address}`);
  ok("/verify recovered the device attestor");
}

// 2. Relay the mint to the attendee's address.
let tokenId: string;
{
  const { status, json } = await post("/mint", { claimUri: uri, recipient: RECIPIENT });
  if (status !== 200) fail(`/mint status ${status}: ${JSON.stringify(json)}`);
  tokenId = json.tokenId;
  ok(`/mint tx ${json.txHash} -> tokenId ${tokenId}`);
}

// 3. Confirm ownership on-chain.
{
  const provider = new ethers.JsonRpcProvider(RPC_URL, chainId);
  const c = new ethers.Contract(contract, ["function ownerOf(uint256) view returns (address)"], provider);
  const owner = await c.ownerOf(tokenId);
  if (ethers.getAddress(owner) !== ethers.getAddress(RECIPIENT)) fail(`ownerOf ${owner} != ${RECIPIENT}`);
  ok(`on-chain ownerOf(${tokenId}) == recipient`);
}

// 4. Replaying the same claim is rejected.
{
  const { status } = await post("/mint", { claimUri: uri, recipient: RECIPIENT });
  if (status !== 409) fail(`replay expected 409, got ${status}`);
  ok("replay rejected (nonce already used)");
}

// 5. A claim signed by a non-device key is rejected.
{
  const impostor = ethers.Wallet.createRandom();
  const badUri = makeClaimURI(impostor, ethers.hexlify(ethers.randomBytes(32)));
  const { status, json } = await post("/mint", { claimUri: badUri, recipient: RECIPIENT });
  if (status !== 400 || json.error !== "bad attestor") {
    fail(`forged claim expected 400 'bad attestor', got ${status} ${JSON.stringify(json)}`);
  }
  ok("forged claim rejected (bad attestor)");
}

console.log("\nPASS: deploy -> attest -> relay -> mint pipeline works end-to-end.");
