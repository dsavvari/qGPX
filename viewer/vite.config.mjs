import { readFile, readdir } from "node:fs/promises";
import { join } from "node:path";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [{
    name: "third-party-notices",
    async generateBundle() {
      const directories = new Set();
      for (const id of this.getModuleIds()) {
        const marker = "/node_modules/";
        const index = id.lastIndexOf(marker);
        if (index < 0) continue;
        const parts = id.slice(index + marker.length).split("/");
        const name = parts[0].startsWith("@") ? parts.slice(0, 2).join("/") : parts[0];
        directories.add(id.slice(0, index + marker.length) + name);
      }
      const notices = ["qGPX viewer - bundled third-party licenses\n"];
      for (const directory of [...directories].sort()) {
        const metadata = JSON.parse(await readFile(join(directory, "package.json"), "utf8"));
        notices.push(`\n${metadata.name} @ ${metadata.version}\nLicense: ${metadata.license}\n`);
        const files = (await readdir(directory))
          .filter((file) => /^(licen[cs]e|copying|notice)/i.test(file)).sort();
        if (!files.length) throw new Error(`Missing license notice for bundled package: ${metadata.name}`);
        for (const file of files) notices.push(await readFile(join(directory, file), "utf8"));
      }
      for (const [name, path] of [
        ["cpp-httplib v0.66.0 (MIT)", "../third_party/cpp-httplib/LICENSE"],
        ["nlohmann/json v3.12.0 (MIT)", "../third_party/nlohmann-json/LICENSE.MIT"],
        ["nlohmann/json embedded component notices", "../third_party/nlohmann-json/NOTICE.txt"],
        ["nlohmann/json embedded Abseil code (Apache-2.0)", "../third_party/nlohmann-json/LICENSES/Apache-2.0.txt"],
        ["nlohmann/json embedded Hedley code (CC0-1.0)", "../third_party/nlohmann-json/LICENSES/CC0-1.0.txt"],
      ]) {
        notices.push(`\n${name}\n${await readFile(new URL(path, import.meta.url), "utf8")}`);
      }
      this.emitFile({ type: "asset", fileName: "third-party-licenses.txt", source: notices.join("\n") });
    },
  }],
});
