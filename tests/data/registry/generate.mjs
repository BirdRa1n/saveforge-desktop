// Generates the registry test distributions used by tst_registry_channel.
//
//   node tests/data/registry/generate.mjs
//
// Requires Node.js 22.15+ (built-in zstd). Signs with the PUBLIC test keys of
// saveforge-specs (registry/test-vectors/signing.json); never trust them.
// Output is deterministic, so regenerating without changes produces no diff.
//
//   basic/    root v1 (root-1, online-1), channel "test-channel" with 3 profiles:
//             one valid, one with an unsafe path, one revoked by the index
//   rotated/  same channel, plus roots/2.json (signed by root-1) that replaces
//             both keys with "unrelated"; the index is signed by "unrelated"

import { createHash, createPrivateKey, createPublicKey, sign } from "node:crypto";
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { constants, zstdCompressSync } from "node:zlib";

const here = dirname(fileURLToPath(import.meta.url));
const specs = join(here, "..", "..", "..", "external", "specs");
const vectors = JSON.parse(readFileSync(join(specs, "registry", "test-vectors", "signing.json"), "utf8"));
const PKCS8_PREFIX = Buffer.from("302e020100300506032b657004220420", "hex");

function key(label) {
  const seed = Buffer.from(vectors.keys.find((k) => k.label === label).seed, "hex");
  const privateKey = createPrivateKey({ key: Buffer.concat([PKCS8_PREFIX, seed]), format: "der", type: "pkcs8" });
  const raw = createPublicKey(privateKey).export({ format: "der", type: "spki" }).subarray(-32);
  return { privateKey, publicKey: raw.toString("base64"), keyId: createHash("sha256").update(raw).digest("hex").slice(0, 16) };
}

const json = (value) => Buffer.from(JSON.stringify(value, null, 2) + "\n");
const pub = (k) => ({ keyId: k.keyId, algorithm: "ed25519", publicKey: k.publicKey });
const sig = (k, bytes) =>
  json({ signatures: [{ keyId: k.keyId, signature: sign(null, bytes, k.privateKey).toString("base64") }] });

const rootKey = key("root-1");
const onlineKey = key("online-1");
const otherKey = key("unrelated");

const hollowKnight = JSON.parse(readFileSync(join(specs, "profile", "examples", "profile.v1", "valid", "hollow-knight.json"), "utf8"));
const profiles = [
  hollowKnight,
  { ...hollowKnight, id: "unsafe-game", name: "Unsafe Game", saves: [{ id: "save", tags: ["save"], paths: ["<home>/.ssh/id_rsa"] }] },
  { ...hollowKnight, id: "revoked-game", name: "Revoked Game", revision: 2 },
];

function writeDistribution(dir, { rotated }) {
  rmSync(dir, { recursive: true, force: true });
  const put = (relative, bytes) => {
    const file = join(dir, relative);
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file, bytes);
  };

  const root1 = json({
    schemaVersion: 1, type: "root", version: 1, expires: "2030-01-01T00:00:00Z", rootThreshold: 1,
    rootKeys: [pub(rootKey)], onlineKeys: [{ ...pub(onlineKey), channels: ["*"] }],
  });
  put("root.json", root1);
  put("root.json.sig", sig(rootKey, root1));
  put("roots/1.json", root1);
  put("roots/1.json.sig", sig(rootKey, root1));

  let indexKey = onlineKey;
  if (rotated) {
    const root2 = json({
      schemaVersion: 1, type: "root", version: 2, expires: "2030-01-01T00:00:00Z", rootThreshold: 1,
      rootKeys: [pub(otherKey)], onlineKeys: [{ ...pub(otherKey), channels: ["test-channel"] }],
    });
    put("roots/2.json", root2);
    put("roots/2.json.sig", sig(rootKey, root2)); // signed by the PREVIOUS root key
    indexKey = otherKey;
  }

  const sequence = 42;
  const bundle = Buffer.from(JSON.stringify({
    schemaVersion: 1, type: "bundle", channel: "test-channel", sequence, generatedAt: "2026-11-09T00:00:00Z",
    notices: [{ license: "CC-BY-SA-4.0", attribution: "SaveForge test fixture" }], profiles,
  }) + "\n");
  const compressed = zstdCompressSync(bundle, { params: { [constants.ZSTD_c_compressionLevel]: 3 } });
  const bundlePath = "bundles/test-channel-000042.json.zst";
  const index = json({
    schemaVersion: 1, type: "index", channel: "test-channel", sequence,
    publishedAt: "2026-11-09T00:00:00Z", expires: "2027-11-09T00:00:00Z", minAppVersion: "0.1.0",
    bundle: {
      path: bundlePath, sha256: createHash("sha256").update(compressed).digest("hex"),
      size: compressed.length, uncompressedSize: bundle.length, profileCount: profiles.length,
    },
    revoked: [{ id: "revoked-game", revision: 2, reason: "test fixture" }],
  });
  put("channels/test-channel/index.json", index);
  put("channels/test-channel/index.json.sig", sig(indexKey, index));
  put(`channels/test-channel/${bundlePath}`, compressed);
}

writeDistribution(join(here, "basic"), { rotated: false });
writeDistribution(join(here, "rotated"), { rotated: true });
console.log("Wrote tests/data/registry/{basic,rotated}");
