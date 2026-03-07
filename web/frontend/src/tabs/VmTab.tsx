import { useState, useCallback, useRef, useEffect } from "react";
import { useWasm } from "../wasm/loader";
import type { VMEngine as VMEngineType, CallFrame } from "../wasm/types";
import { Editor } from "../components/Editor";
import { ControlBar } from "../components/ControlBar";
import { StatusBar, stateToName } from "../components/StatusBar";
import { RegisterDisplay } from "../components/RegisterDisplay";
import { ScreenCanvas } from "../components/ScreenCanvas";
import { mapKeyToHack } from "../util/keyboard";

const VM_PLACEHOLDER = `// Paste or load a .vm file
function Main.main 2
  push constant 10
  push constant 20
  add
  pop local 0
  push local 0
  return`;

const BATCH_SIZE = 50000;

export function VmTab() {
  const wasm = useWasm();
  const engineRef = useRef<VMEngineType | null>(null);
  const runningRef = useRef(false);
  const tabRef = useRef<HTMLDivElement>(null);

  const [source, setSource] = useState(VM_PLACEHOLDER);
  const [state, setState] = useState(0);
  const [error, setError] = useState("");
  const [running, setRunning] = useState(false);
  const [pc, setPc] = useState(0);
  const [currentFn, setCurrentFn] = useState("");
  const [currentCmd, setCurrentCmd] = useState("");
  const [stack, setStack] = useState<number[]>([]);
  const [segments, setSegments] = useState<Record<string, number>>({});
  const [callStack, setCallStack] = useState<CallFrame[]>([]);
  const [instrCount, setInstrCount] = useState(0);
  const [currentKey, setCurrentKey] = useState(0);

  useEffect(() => {
    const eng = new wasm.VMEngine();
    engineRef.current = eng;
    return () => {
      eng.delete();
      engineRef.current = null;
    };
  }, [wasm]);

  const readRam = useCallback((addr: number) => {
    return engineRef.current?.readRam(addr) ?? 0;
  }, []);

  const syncState = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    setState(eng.getState());
    setError(eng.getErrorMessage());
    setPc(eng.getPC());
    setCurrentFn(eng.getCurrentFunction());
    setCurrentCmd(eng.currentCommandString());
    setCallStack(eng.getCallStack());

    // Stack
    try {
      const sp = eng.getSP();
      const s: number[] = [];
      for (let i = 256; i < sp && i < 2048; i++) s.push(eng.readRam(i));
      setStack(s);
    } catch {
      setStack([]);
    }

    // Segment pointers
    setSegments({
      SP: eng.readRam(0),
      LCL: eng.readRam(1),
      ARG: eng.readRam(2),
      THIS: eng.readRam(3),
      THAT: eng.readRam(4),
    });
  }, []);

  const loadProgram = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return false;
    try {
      eng.loadString(source);
      syncState();
      return true;
    } catch (e) {
      setError(String(e));
      syncState();
      return false;
    }
  }, [source, syncState]);

  const ensureLoaded = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return false;
    if (eng.getState() === 0 && eng.getCommandCount() === 0) {
      return loadProgram();
    }
    return true;
  }, [loadProgram]);

  const handleStep = useCallback(() => {
    if (!ensureLoaded()) return;
    try {
      engineRef.current!.step();
    } catch (e) {
      setError(String(e));
    }
    syncState();
    setInstrCount((c) => c + 1);
  }, [ensureLoaded, syncState]);

  const handleStepOver = useCallback(() => {
    if (!ensureLoaded()) return;
    try {
      engineRef.current!.stepOver();
    } catch (e) {
      setError(String(e));
    }
    syncState();
  }, [ensureLoaded, syncState]);

  const handleStepOut = useCallback(() => {
    if (!ensureLoaded()) return;
    try {
      engineRef.current!.stepOut();
    } catch (e) {
      setError(String(e));
    }
    syncState();
  }, [ensureLoaded, syncState]);

  const handleRun = useCallback(() => {
    if (!ensureLoaded()) return;
    const eng = engineRef.current!;
    runningRef.current = true;
    setRunning(true);

    const tick = () => {
      if (!runningRef.current) return;
      try {
        const s = eng.runFor(BATCH_SIZE);
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
    engineRef.current?.pause();
    syncState();
  }, [syncState]);

  const handleReset = useCallback(() => {
    runningRef.current = false;
    setRunning(false);
    engineRef.current?.reset();
    setError("");
    setInstrCount(0);
    setCurrentKey(0);
    syncState();
  }, [syncState]);

  // Keyboard input
  useEffect(() => {
    const el = tabRef.current;
    if (!el) return;

    const onKeyDown = (e: KeyboardEvent) => {
      const eng = engineRef.current;
      if (!eng) return;
      const hackKey = mapKeyToHack(e);
      if (hackKey !== null) {
        eng.setKeyboard(hackKey);
        setCurrentKey(hackKey);
        e.preventDefault();
      }
    };
    const onKeyUp = () => {
      engineRef.current?.setKeyboard(0);
      setCurrentKey(0);
    };

    el.addEventListener("keydown", onKeyDown);
    el.addEventListener("keyup", onKeyUp);
    return () => {
      el.removeEventListener("keydown", onKeyDown);
      el.removeEventListener("keyup", onKeyUp);
    };
  }, []);

  const stateName = stateToName(state);

  return (
    <div className="tab-panel" ref={tabRef} tabIndex={0}>
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
        {/* Left: source */}
        <div style={{ flex: 1, overflow: "hidden" }}>
          <Editor
            label="VM Code"
            value={source}
            onChange={setSource}
            accept=".vm"
          />
        </div>

        {/* Middle: stack + screen */}
        <div style={{ display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <div className="panel" style={{ width: 180, flex: 1, overflow: "hidden" }}>
            <div className="panel-header">Stack</div>
            <div className="panel-body">
              <div className="register-display">
                {stack.length === 0 && (
                  <div style={{ color: "var(--text-dim)", fontSize: 12 }}>
                    (empty)
                  </div>
                )}
                {[...stack].reverse().map((v, i) => (
                  <div key={i} className="register-row">
                    <span className="reg-name">{stack.length - 1 - i + 256}</span>
                    <span className="reg-value">{v}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
          <div className="panel">
            <div className="panel-header">
              Screen
              {currentKey > 0 && (
                <span className="keyboard-status">Key: {currentKey}</span>
              )}
            </div>
            <div className="panel-body" style={{ display: "flex", justifyContent: "center", padding: 4 }}>
              <ScreenCanvas readRam={readRam} width={180} />
            </div>
          </div>
        </div>

        {/* Right: segments + call stack */}
        <div style={{ width: 220, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <div className="panel">
            <div className="panel-header">Segments</div>
            <div className="panel-body">
              <RegisterDisplay registers={segments} />
            </div>
          </div>

          <div className="panel">
            <div className="panel-header">Current</div>
            <div className="panel-body">
              <RegisterDisplay
                registers={{
                  PC: pc,
                  Function: currentFn || "(none)",
                  Command: currentCmd || "(none)",
                }}
              />
            </div>
          </div>

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
                  {f.caller_file && (
                    <span className="location">
                      {f.caller_file}:{f.caller_line}
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
        stats={{ PC: pc, Instructions: instrCount }}
      />
    </div>
  );
}
