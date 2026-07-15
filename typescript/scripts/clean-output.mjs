import { rmSync } from "node:fs";

const target = process.argv[2];
if (target !== "dist" && target !== ".test-dist") {
  throw new TypeError("clean-output target must be dist or .test-dist");
}

rmSync(new URL(`../${target}/`, import.meta.url), {
  recursive: true,
  force: true,
});
