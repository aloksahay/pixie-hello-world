import fs from "node:fs";
import path from "node:path";

// Minimal .env loader so we don't pull in an extra dependency. Existing
// process.env values win over the file.
const envPath = path.resolve(process.cwd(), ".env");
if (fs.existsSync(envPath)) {
  for (const line of fs.readFileSync(envPath, "utf8").split("\n")) {
    const m = line.match(/^\s*([A-Z0-9_]+)\s*=\s*(.*?)\s*$/);
    if (m && !(m[1] in process.env)) {
      process.env[m[1]] = m[2].replace(/^["']|["']$/g, "");
    }
  }
}

const port = Number(process.env.PORT ?? 8787);

export const config = {
  port,
  rpcUrl: process.env.RPC_URL ?? "",
  relayerKey: process.env.RELAYER_PRIVATE_KEY ?? "",
  contract: process.env.CONTRACT_ADDRESS ?? "",
  chainId: Number(process.env.CHAIN_ID ?? 84532),
  eventId: BigInt(process.env.EVENT_ID ?? 1),
  baseUrl: process.env.BASE_URL ?? `http://localhost:${port}`,
};

/** True when enough is configured to submit on-chain transactions. */
export function canRelay(): boolean {
  return Boolean(config.rpcUrl && config.relayerKey && config.contract);
}
