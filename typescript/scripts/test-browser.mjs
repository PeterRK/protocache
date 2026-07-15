import { spawn, spawnSync } from "node:child_process";
import { once } from "node:events";
import { createServer } from "node:http";
import { mkdtemp, readFile, rm, stat } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, extname, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const browser = findBrowser();
const profile = await mkdtemp(resolve(tmpdir(), "protocache-browser-"));

let settleResult;
let rejectResult;
const result = new Promise((resolveResult, reject) => {
  settleResult = resolveResult;
  rejectResult = reject;
});

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url ?? "/", "http://127.0.0.1");
    if (request.method === "POST" && url.pathname === "/__result") {
      const chunks = [];
      let length = 0;
      for await (const chunk of request) {
        length += chunk.length;
        if (length > 64 * 1024) throw new Error("browser result is too large");
        chunks.push(chunk);
      }
      const body = JSON.parse(Buffer.concat(chunks).toString("utf8"));
      response.writeHead(204).end();
      settleResult(body);
      return;
    }
    if (request.method !== "GET") {
      response.writeHead(405).end();
      return;
    }
    if (url.pathname === "/") {
      response.writeHead(200, { "content-type": "text/html; charset=utf-8" });
      response.end('<!doctype html><meta charset="utf-8"><script type="module" src="/test/platform/browser-entry.mjs"></script>');
      return;
    }
    const relative = decodeURIComponent(url.pathname).replace(/^\/+/, "");
    const filename = resolve(root, relative);
    if (!filename.startsWith(root + sep)) {
      response.writeHead(404).end();
      return;
    }
    let fileStatus;
    try {
      fileStatus = await stat(filename);
    } catch (error) {
      if (error !== null && typeof error === "object" && error.code === "ENOENT") {
        response.writeHead(404).end();
        return;
      }
      throw error;
    }
    if (!fileStatus.isFile()) {
      response.writeHead(404).end();
      return;
    }
    response.writeHead(200, { "content-type": contentType(filename) });
    response.end(await readFile(filename));
  } catch (error) {
    response.writeHead(500).end();
    rejectResult(error);
  }
});

server.listen(0, "127.0.0.1");
await once(server, "listening");
const address = server.address();
if (address === null || typeof address === "string") {
  throw new Error("browser test server did not bind a TCP port");
}

const page = `http://127.0.0.1:${address.port}/`;
const child = spawn(browser.command, browser.args(page, profile), {
  stdio: ["ignore", "pipe", "pipe"],
});
let output = "";
child.stdout.on("data", (chunk) => { output += chunk.toString(); });
child.stderr.on("data", (chunk) => { output += chunk.toString(); });
child.on("error", rejectResult);
child.on("exit", (code, signal) => {
  if (code !== 0 && signal === null) {
    rejectResult(new Error(`${browser.command} exited with ${code}\n${output}`));
  }
});

const timeout = setTimeout(() => {
  rejectResult(new Error(`browser integration test timed out\n${output}`));
}, 45_000);

try {
  const browserResult = await result;
  if (browserResult.ok !== true) {
    throw new Error(browserResult.error ?? "browser integration test failed");
  }
  console.log(`${browser.name} window and module Worker integration test: OK`);
} finally {
  clearTimeout(timeout);
  child.kill("SIGTERM");
  server.close();
  await rm(profile, { recursive: true, force: true });
}

function findBrowser() {
  const configured = process.env.PROTOCACHE_TEST_BROWSER;
  if (configured !== undefined && configured !== "") {
    return browserDefinition(configured);
  }
  for (const command of ["firefox", "firefox-esr", "chromium", "google-chrome"]) {
    const probe = spawnSync(command, ["--version"], { stdio: "ignore" });
    if (probe.status === 0) return browserDefinition(command);
  }
  throw new Error(
    "No supported browser found; install Firefox/Chromium or set PROTOCACHE_TEST_BROWSER",
  );
}

function browserDefinition(command) {
  const name = command.toLowerCase().includes("firefox") ? "Firefox" : "Chromium";
  return name === "Firefox"
    ? {
        command,
        name,
        args: (page, selectedProfile) => [
          "--headless",
          "--no-remote",
          "--profile",
          selectedProfile,
          page,
        ],
      }
    : {
        command,
        name,
        args: (page, selectedProfile) => [
          "--headless=new",
          "--disable-gpu",
          "--no-sandbox",
          `--user-data-dir=${selectedProfile}`,
          page,
        ],
      };
}

function contentType(filename) {
  switch (extname(filename)) {
    case ".html": return "text/html; charset=utf-8";
    case ".js":
    case ".mjs": return "text/javascript; charset=utf-8";
    case ".wasm": return "application/wasm";
    case ".map":
    case ".json": return "application/json; charset=utf-8";
    default: return "application/octet-stream";
  }
}
