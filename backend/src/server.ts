import express from "express";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ethers } from "ethers";

import { config, canRelay } from "./config.js";
import { FIREFLY_POAP_ABI } from "./abi.js";
import { parseClaimURI, claimDigest, recoverSigner, type Claim } from "./claim.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const app = express();
app.use(express.json());
app.use(express.static(path.resolve(__dirname, "../public")));

// Lazily-built chain handles (only when relaying is configured).
let provider: ethers.JsonRpcProvider | null = null;
let relayer: ethers.Wallet | null = null;
let contract: ethers.Contract | null = null;

function chain() {
  if (!canRelay()) throw new Error("relaying not configured (set RPC_URL, RELAYER_PRIVATE_KEY, CONTRACT_ADDRESS)");
  if (!contract) {
    provider = new ethers.JsonRpcProvider(config.rpcUrl, config.chainId);
    relayer = new ethers.Wallet(config.relayerKey, provider);
    contract = new ethers.Contract(config.contract, FIREFLY_POAP_ABI, relayer);
  }
  return { provider: provider!, relayer: relayer!, contract: contract! };
}

// Accept either { claimUri } or the individual claim fields.
function claimFromBody(body: any): Claim {
  if (typeof body?.claimUri === "string") return parseClaimURI(body.claimUri);
  const { chainId, contract, eventId, nonce, sig } = body ?? {};
  if (chainId && contract && eventId != null && nonce && sig) {
    return {
      chainId: Number(chainId),
      contract: ethers.getAddress(contract),
      eventId: BigInt(eventId),
      nonce: String(nonce).toLowerCase(),
      sig: String(sig).toLowerCase(),
    };
  }
  throw new Error("provide { claimUri } or { chainId, contract, eventId, nonce, sig }");
}

app.get("/health", (_req, res) => {
  res.json({ ok: true, canRelay: canRelay(), chainId: config.chainId, eventId: config.eventId.toString() });
});

/**
 * Recompute the EIP-712 digest for a claim and recover its signer. No chain
 * access required. Used for the device<->contract cross-check.
 */
app.post("/verify", (req, res) => {
  try {
    const claim = claimFromBody(req.body);
    const digest = claimDigest(claim);
    const signer = recoverSigner(claim);
    res.json({
      digest,
      signer,
      claim: { ...claim, eventId: claim.eventId.toString() },
    });
  } catch (err: any) {
    res.status(400).json({ error: err.message });
  }
});

/**
 * Relay a claim on-chain: mint the POAP to `recipient` (or the relayer if
 * omitted). Stands in for the phone submitting the transaction.
 */
app.post("/mint", async (req, res) => {
  let claim: Claim;
  try {
    claim = claimFromBody(req.body);
  } catch (err: any) {
    return res.status(400).json({ error: err.message });
  }
  if (!canRelay()) {
    return res.status(501).json({ error: "relaying not configured on this server" });
  }

  try {
    const { relayer, contract } = chain();
    const recipient = req.body?.recipient
      ? ethers.getAddress(req.body.recipient)
      : relayer.address;

    // Fail fast with a friendly reason instead of an opaque revert.
    const signer = recoverSigner(claim);
    const onchainAttestor: string = await contract.attestor();
    if (signer.toLowerCase() !== onchainAttestor.toLowerCase()) {
      return res.status(400).json({ error: "bad attestor", signer, expected: onchainAttestor });
    }
    if (await contract.usedNonce(claim.nonce)) {
      return res.status(409).json({ error: "nonce already used" });
    }

    const tx = await contract.mintTo(recipient, claim.eventId, claim.nonce, claim.sig);
    const receipt = await tx.wait();

    // Pull tokenId out of the Claimed event.
    let tokenId: string | null = null;
    for (const log of receipt?.logs ?? []) {
      try {
        const parsed = contract.interface.parseLog(log);
        if (parsed?.name === "Claimed") tokenId = parsed.args.tokenId.toString();
      } catch {
        /* not our event */
      }
    }

    res.json({ txHash: tx.hash, to: recipient, tokenId });
  } catch (err: any) {
    res.status(400).json({ error: err.shortMessage ?? err.message });
  }
});

/** ERC-721 metadata for a token (all tokens share the event's art). */
app.get("/metadata/:id", (req, res) => {
  res.json({
    name: `Firefly POAP #${req.params.id}`,
    description:
      "Proof of attendance attested by a physical Firefly (Pixie) device. Only the Firefly's key could authorize this mint.",
    image: `${config.baseUrl}/poap.svg`,
    attributes: [
      { trait_type: "Event", value: "Firefly Meetup" },
      { trait_type: "Attestation", value: "Firefly device (secp256k1)" },
    ],
  });
});

app.listen(config.port, () => {
  console.log(`Firefly POAP backend on ${config.baseUrl}`);
  console.log(`  relaying: ${canRelay() ? "enabled" : "disabled (set RPC_URL, RELAYER_PRIVATE_KEY, CONTRACT_ADDRESS)"}`);
  console.log(`  scan page: ${config.baseUrl}/scan.html`);
});
