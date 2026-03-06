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
 * The built n2t.js file is expected at /wasm/n2t.js (copied into public/ or
 * served alongside the frontend).  Vite's dev server serves `public/` at root.
 */
export async function loadWasmModule(): Promise<N2TModule> {
  // Dynamic import so Vite doesn't try to bundle the Emscripten glue.
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const factory = (await import(/* @vite-ignore */ "/wasm/n2t.js")) as any;
  const createModule = factory.default ?? factory.createN2TModule ?? factory;
  const mod: N2TModule = await createModule();
  return mod;
}
