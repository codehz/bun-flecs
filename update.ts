const flecs_base =
  "https://github.com/SanderMertens/flecs/raw/refs/heads/master/distr/";
const napi_base =
  "https://raw.githubusercontent.com/oven-sh/bun/refs/heads/main/src/napi/";

async function downloadFile(base: string, filename: string) {
  console.time("Downloading " + filename);
  await Bun.write("./c-src/" + filename, await fetch(base + filename));
  console.timeEnd("Downloading " + filename);
}

await downloadFile(flecs_base, "flecs.c");
await downloadFile(flecs_base, "flecs.h");
await downloadFile(napi_base, "js_native_api.h");
await downloadFile(napi_base, "js_native_api_types.h");
await downloadFile(napi_base, "node_api.h");
await downloadFile(napi_base, "node_api_types.h");
