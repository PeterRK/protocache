import {
  copyFileSync,
  existsSync,
  mkdirSync,
} from "node:fs";
import { delimiter, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const build = join(root, ".wasm-build", "wasm");
const dist = join(root, "dist");
const localEmsdk = join(root, ".tools", "emsdk");
const emsdk = process.env.EMSDK ?? (
  existsSync(join(localEmsdk, "upstream", "emscripten", "emcmake"))
    ? localEmsdk
    : undefined
);
const emcmake = process.env.EMCMAKE ??
  (emsdk === undefined ? "emcmake" : join(emsdk, "upstream", "emscripten", "emcmake"));
const extraPath = emsdk === undefined
  ? process.env.PATH
  : [
      emsdk,
      join(emsdk, "upstream", "emscripten"),
      process.env.PATH,
    ].filter(Boolean).join(delimiter);
const environment = {
  ...process.env,
  ...(emsdk === undefined
    ? {}
    : {
        EMSDK: emsdk,
        EM_CONFIG: process.env.EM_CONFIG ?? join(emsdk, ".emscripten"),
      }),
  PATH: extraPath,
};

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: root,
    env: environment,
    encoding: "utf8",
    stdio: "inherit",
  });
  if (result.error !== undefined) {
    throw result.error;
  }
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

if (
  emsdk === undefined &&
  !existsSync(emcmake) &&
  spawnSync(emcmake, ["--help"], { env: environment }).error !== undefined
) {
  throw new Error(
    "Emscripten not found; set EMSDK or EMCMAKE before running build:wasm",
  );
}

mkdirSync(build, { recursive: true });
run(emcmake, [
  "cmake",
  "-S",
  "wasm",
  "-B",
  build,
  "-DCMAKE_BUILD_TYPE=Release",
]);
run("cmake", ["--build", build, "--target", "protocache-wasm"]);

const artifact = join(build, "protocache.wasm");
if (!existsSync(artifact)) {
  throw new Error(`WASM build did not produce ${artifact}`);
}
mkdirSync(dist, { recursive: true });
copyFileSync(artifact, join(dist, "protocache.wasm"));
