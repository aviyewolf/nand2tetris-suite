import { useEffect, useState } from "react";
import { WasmContext, loadWasmModule } from "./wasm/loader";
import type { N2TModule } from "./wasm/types";
import { HdlTab } from "./tabs/HdlTab";
import { CpuTab } from "./tabs/CpuTab";
import { VmTab } from "./tabs/VmTab";
import { JackTab } from "./tabs/JackTab";

type TabId = "hdl" | "cpu" | "vm" | "jack";

const TABS: { id: TabId; label: string }[] = [
  { id: "hdl", label: "HDL Simulator" },
  { id: "cpu", label: "CPU Simulator" },
  { id: "vm", label: "VM Emulator" },
  { id: "jack", label: "Jack Debugger" },
];

export function App() {
  const [wasm, setWasm] = useState<N2TModule | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [tab, setTab] = useState<TabId>("hdl");

  useEffect(() => {
    loadWasmModule().then(setWasm).catch((e) => setError(String(e)));
  }, []);

  if (error) {
    return (
      <div className="app-loading">
        <h2>Failed to load WASM module</h2>
        <pre>{error}</pre>
        <p>
          Make sure you built the WASM module and copied <code>n2t.js</code> +{" "}
          <code>n2t.wasm</code> into <code>web/frontend/public/wasm/</code>.
        </p>
      </div>
    );
  }

  if (!wasm) {
    return (
      <div className="app-loading">
        <h2>Loading Nand2Tetris Suite...</h2>
        <p>Initializing WebAssembly module</p>
      </div>
    );
  }

  return (
    <WasmContext.Provider value={wasm}>
      <div className="app">
        <header className="app-header">
          <h1>Nand2Tetris Suite</h1>
          <nav className="tab-bar">
            {TABS.map((t) => (
              <button
                key={t.id}
                className={`tab-btn ${tab === t.id ? "active" : ""}`}
                onClick={() => setTab(t.id)}
              >
                {t.label}
              </button>
            ))}
          </nav>
        </header>
        <main className="app-main">
          {tab === "hdl" && <HdlTab />}
          {tab === "cpu" && <CpuTab />}
          {tab === "vm" && <VmTab />}
          {tab === "jack" && <JackTab />}
        </main>
      </div>
    </WasmContext.Provider>
  );
}
