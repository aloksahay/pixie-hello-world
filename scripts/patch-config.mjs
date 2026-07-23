#!/usr/bin/env node
// Patch main/poap-config.h to match a deployment.
// Usage: node scripts/patch-config.mjs --address 0x.. [--chain 84532] [--event 1]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const CONFIG = path.resolve(__dirname, "../main/poap-config.h");

const args = Object.fromEntries(
  process.argv.slice(2).reduce((acc, a, i, arr) => {
    if (a.startsWith("--")) acc.push([a.slice(2), arr[i + 1]]);
    return acc;
  }, [])
);

if (!args.address) {
  console.error("usage: node scripts/patch-config.mjs --address 0x.. [--chain N] [--event N]");
  process.exit(1);
}

const addr = args.address.toLowerCase().replace(/^0x/, "");
if (!/^[0-9a-f]{40}$/.test(addr)) {
  console.error("invalid address:", args.address);
  process.exit(1);
}

let src = fs.readFileSync(CONFIG, "utf8");

// Rebuild the 20-byte array, 10 bytes per line to match the existing style.
const bytes = addr.match(/../g).map((b) => "0x" + b);
const rows = [bytes.slice(0, 10), bytes.slice(10, 20)].map((r) => "    " + r.join(", "));
const block = `#define POAP_CONTRACT_ADDRESS { \\\n${rows.join(", \\\n")} }`;
src = src.replace(
  /#define POAP_CONTRACT_ADDRESS \{[\s\S]*?\}/,
  block
);

if (args.chain) {
  src = src.replace(/#define POAP_CHAIN_ID\s+\d+ULL/, `#define POAP_CHAIN_ID        ${args.chain}ULL`);
}
if (args.event) {
  src = src.replace(/#define POAP_EVENT_ID\s+\d+ULL/, `#define POAP_EVENT_ID        ${args.event}ULL`);
}

fs.writeFileSync(CONFIG, src);
console.log(`patched ${path.relative(process.cwd(), CONFIG)}`);
console.log(`  contract = 0x${addr}`);
if (args.chain) console.log(`  chainId  = ${args.chain}`);
if (args.event) console.log(`  eventId  = ${args.event}`);
console.log("\nNow rebuild + flash so the device signs for this domain:");
console.log("  idf.py build flash monitor");
