import { createContext, useContext } from "react";
import type { N2TModule } from "./types";

/** React context for the loaded WASM module */
export const WasmContext = createContext<N2TModule | null>(null);

/** Hook — throws if used before module is ready */
export function useWasm(): N2TModule {
  const mod = useContext(WasmContext);
  if (!mod) throw new Error("WASM module not loaded yet");
  return mod;
}

/**
 * Load the WASM module.
 *
 * The n2t.js + n2t.wasm files live in public/wasm/ and are served at /wasm/.
 * We load n2t.js via a dynamic <script type="module"> to avoid Vite's
 * restriction on importing JS from public/. The Emscripten glue registers
 * createN2TModule as an ES module default export, which we capture through
 * a global.
 */
export async function loadWasmModule(): Promise<N2TModule> {
  // Load the Emscripten glue via a dynamic module script that assigns
  // the factory to a global we can read.
  await new Promise<void>((resolve, reject) => {
    const script = document.createElement("script");
    script.type = "module";
    // The inline module imports the ES-module glue and stashes the factory.
    script.textContent = `
      import createN2TModule from "/wasm/n2t.js";
      window.__createN2TModule = createN2TModule;
      window.dispatchEvent(new Event("n2t-ready"));
    `;
    window.addEventListener("n2t-ready", () => resolve(), { once: true });
    script.onerror = reject;
    document.head.appendChild(script);
  });

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const createModule = (window as any).__createN2TModule;
  if (!createModule) {
    throw new Error("Failed to load WASM module");
  }

  const mod: N2TModule = await createModule();
  return mod;
}
