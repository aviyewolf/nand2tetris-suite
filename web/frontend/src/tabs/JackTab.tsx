import { useState, useCallback, useRef, useEffect } from "react";
import { useWasm } from "../wasm/loader";
import type {
  JackDebugger as JackDebuggerType,
  JackCallFrame,
  JackVariableValue,
  SourceEntry,
} from "../wasm/types";
import { Editor } from "../components/Editor";
import { ControlBar } from "../components/ControlBar";
import { StatusBar, stateToName } from "../components/StatusBar";

const VM_PLACEHOLDER = `// Paste or load compiled VM code here`;
const SMAP_PLACEHOLDER = `// Paste or load the .smap source-map file here`;
const BATCH_SIZE = 50000;

const VAR_KIND_NAMES = ["local", "argument", "field", "static"];

export function JackTab() {
  const wasm = useWasm();
  const dbgRef = useRef<JackDebuggerType | null>(null);
  const runningRef = useRef(false);

  const [vm, setVm] = useState(VM_PLACEHOLDER);
  const [smap, setSmap] = useState(SMAP_PLACEHOLDER);
  // Jack source files for auto source-map mode
  const [jackSources, setJackSources] = useState<[string, string][]>([]);
  const [state, setState] = useState(0);
  const [error, setError] = useState("");
  const [running, setRunning] = useState(false);
  const [source, setSource] = useState<SourceEntry | null>(null);
  const [currentFn, setCurrentFn] = useState("");
  const [callStack, setCallStack] = useState<JackCallFrame[]>([]);
  const [variables, setVariables] = useState<JackVariableValue[]>([]);
  const [instrCount, setInstrCount] = useState(0);

  useEffect(() => {
    const d = new wasm.JackDebugger();
    dbgRef.current = d;
    return () => {
      d.delete();
      dbgRef.current = null;
    };
  }, [wasm]);

  const syncState = useCallback(() => {
    const d = dbgRef.current;
    if (!d) return;
    setState(d.getState());
    setCurrentFn(d.getCurrentFunction());
    setSource(d.getCurrentSource());
    setCallStack(d.getCallStack());
    try {
      setVariables(d.getAllVariables());
    } catch {
      setVariables([]);
    }
    // Surface error message from underlying VM engine
    if (d.getState() === 4) {
      setError(d.getErrorMessage());
    }
  }, []);

  const loadProgram = useCallback(() => {
    const d = dbgRef.current;
    if (!d) return false;
    try {
      if (jackSources.length > 0) {
        d.loadWithSources(jackSources, vm);
      } else {
        d.load(vm, smap);
      }
      syncState();
      return true;
    } catch (e) {
      setError(String(e));
      syncState();
      return false;
    }
  }, [vm, smap, jackSources, syncState]);

  const ensureLoaded = useCallback(() => {
    const d = dbgRef.current;
    if (!d) return false;
    if (d.getState() === 0) return loadProgram();
    return true;
  }, [loadProgram]);

  const handleStep = useCallback(() => {
    if (!ensureLoaded()) return;
    try {
      dbgRef.current!.step();
    } catch (e) {
      setError(String(e));
    }
    syncState();
    setInstrCount((c) => c + 1);
  }, [ensureLoaded, syncState]);

  const handleStepOver = useCallback(() => {
    if (!ensureLoaded()) return;
    try {
      dbgRef.current!.stepOver();
    } catch (e) {
      setError(String(e));
    }
    syncState();
  }, [ensureLoaded, syncState]);

  const handleStepOut = useCallback(() => {
    if (!ensureLoaded()) return;
    try {
      dbgRef.current!.stepOut();
    } catch (e) {
      setError(String(e));
    }
    syncState();
  }, [ensureLoaded, syncState]);

  const handleRun = useCallback(() => {
    if (!ensureLoaded()) return;
    const d = dbgRef.current!;
    runningRef.current = true;
    setRunning(true);

    const tick = () => {
      if (!runningRef.current) return;
      try {
        const s = d.runFor(BATCH_SIZE);
        syncState();
        setInstrCount((c) => c + BATCH_SIZE);
        if (s === 3 || s === 4 || s === 2) {
          runningRef.current = false;
          setRunning(false);
          return;
        }
      } catch (e) {
        setError(String(e));
        syncState();
        runningRef.current = false;
        setRunning(false);
        return;
      }
      setTimeout(tick, 0);
    };
    tick();
  }, [ensureLoaded, syncState]);

  const handlePause = useCallback(() => {
    runningRef.current = false;
    setRunning(false);
    dbgRef.current?.pause();
    syncState();
  }, [syncState]);

  const handleReset = useCallback(() => {
    runningRef.current = false;
    setRunning(false);
    dbgRef.current?.reset();
    setError("");
    setInstrCount(0);
    setSource(null);
    setCallStack([]);
    setVariables([]);
    syncState();
  }, [syncState]);

  const stateName = stateToName(state);

  return (
    <div className="tab-panel">
      <ControlBar
        onRun={handleRun}
        onStep={handleStep}
        onStepOver={handleStepOver}
        onStepOut={handleStepOut}
        onReset={handleReset}
        onPause={handlePause}
        running={running}
      />
      <div className="tab-content">
        {/* Left: editors */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <Editor
            label="VM Code"
            value={vm}
            onChange={setVm}
            accept=".vm"
          />
          <Editor
            label="Source Map (.smap)"
            value={smap}
            onChange={(v) => { setSmap(v); setJackSources([]); }}
            accept=".smap"
          />
          <div className="panel" style={{ padding: 8 }}>
            <label style={{ fontSize: 12, color: "var(--text-dim)" }}>
              Or load .jack source files (auto source map):
            </label>
            <input
              type="file"
              accept=".jack"
              multiple
              style={{ fontSize: 12, marginTop: 4, display: "block" }}
              onChange={async (e) => {
                const files = e.target.files;
                if (!files || files.length === 0) return;
                const sources: [string, string][] = [];
                for (let i = 0; i < files.length; i++) {
                  const f = files[i];
                  if (!f) continue;
                  const text = await f.text();
                  sources.push([f.name, text]);
                }
                setJackSources(sources);
                setError("");
              }}
            />
            {jackSources.length > 0 && (
              <div style={{ fontSize: 11, color: "var(--text-dim)", marginTop: 4 }}>
                {jackSources.length} .jack file(s) loaded
              </div>
            )}
          </div>
        </div>

        {/* Right: source location, variables, call stack */}
        <div style={{ width: 280, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          {/* Source location */}
          <div className="panel">
            <div className="panel-header">Source Location</div>
            <div className="panel-body">
              {source ? (
                <div className="register-display">
                  <div className="register-row">
                    <span className="reg-name">File</span>
                    <span className="reg-value">{source.jack_file}</span>
                  </div>
                  <div className="register-row">
                    <span className="reg-name">Line</span>
                    <span className="reg-value">{source.jack_line}</span>
                  </div>
                  <div className="register-row">
                    <span className="reg-name">Function</span>
                    <span className="reg-value">{currentFn}</span>
                  </div>
                </div>
              ) : (
                <div style={{ color: "var(--text-dim)", fontSize: 12 }}>
                  No source info
                </div>
              )}
            </div>
          </div>

          {/* Variables */}
          <div className="panel" style={{ flex: 1 }}>
            <div className="panel-header">Variables</div>
            <div className="panel-body variables-list">
              {variables.length === 0 && (
                <div style={{ color: "var(--text-dim)", fontSize: 12 }}>
                  No variables
                </div>
              )}
              {variables.map((v, i) => (
                <div key={i} className="var-entry">
                  <span>
                    <span className="var-name">{v.name}</span>
                    <span className="var-type">
                      {v.type_name} ({VAR_KIND_NAMES[v.kind] ?? "?"})
                    </span>
                  </span>
                  <span className="var-value">{v.signed_value}</span>
                </div>
              ))}
            </div>
          </div>

          {/* Call stack */}
          <div className="panel" style={{ flex: 1 }}>
            <div className="panel-header">Call Stack</div>
            <div className="panel-body call-stack">
              {callStack.length === 0 && (
                <div style={{ color: "var(--text-dim)", fontSize: 12 }}>
                  (empty)
                </div>
              )}
              {callStack.map((f, i) => (
                <div key={i} className="call-stack-entry">
                  <span className="fn-name">{f.function_name}</span>
                  {f.jack_file && (
                    <span className="location">
                      {f.jack_file}:{f.jack_line}
                    </span>
                  )}
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
      <StatusBar
        state={stateName}
        error={stateName === "error" ? error : undefined}
        stats={{ Instructions: instrCount }}
      />
    </div>
  );
}
