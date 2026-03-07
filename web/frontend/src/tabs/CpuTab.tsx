import { useState, useCallback, useRef, useEffect } from "react";
import { useWasm } from "../wasm/loader";
import type { CPUEngine as CPUEngineType, CPUStats } from "../wasm/types";
import { Editor } from "../components/Editor";
import { ControlBar } from "../components/ControlBar";
import { StatusBar, stateToName } from "../components/StatusBar";
import { RegisterDisplay } from "../components/RegisterDisplay";
import { ScreenCanvas } from "../components/ScreenCanvas";
import { mapKeyToHack } from "../util/keyboard";

const HACK_PLACEHOLDER = `// Paste or load a .hack file (binary machine code)
// Example: adds 2 + 3
0000000000000010
1110110000010000
0000000000000011
1110000010010000
0000000000000000
1110001100001000`;

/** Number of instructions to run per batch when using Run */
const BATCH_SIZE = 50000;
const RAM_VIEW_SIZE = 16;

export function CpuTab() {
  const wasm = useWasm();
  const engineRef = useRef<CPUEngineType | null>(null);
  const runningRef = useRef(false);
  const tabRef = useRef<HTMLDivElement>(null);

  const [hack, setHack] = useState(HACK_PLACEHOLDER);
  const [state, setState] = useState(0);
  const [error, setError] = useState("");
  const [running, setRunning] = useState(false);
  const [regs, setRegs] = useState({ A: 0, D: 0, PC: 0 });
  const [ram, setRam] = useState<number[]>([]);
  const [ramBase, setRamBase] = useState(0);
  const [ramInput, setRamInput] = useState("0");
  const [disasm, setDisasm] = useState<string[]>([]);
  const [cpuStats, setCpuStats] = useState<CPUStats | null>(null);
  const [currentKey, setCurrentKey] = useState(0);

  useEffect(() => {
    const eng = new wasm.CPUEngine();
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
    const s = eng.getState();
    setState(s);
    setError(eng.getErrorMessage());
    setRegs({ A: eng.getA(), D: eng.getD(), PC: eng.getPC() });
    // Read RAM from current base
    setRamBase((base) => {
      const r: number[] = [];
      for (let i = 0; i < RAM_VIEW_SIZE; i++) r.push(eng.readRam(base + i));
      setRam(r);
      return base;
    });
    try {
      setCpuStats(eng.getStats());
    } catch {
      // getStats may not be available in all builds
    }
  }, []);

  const updateRamView = useCallback((base: number) => {
    const eng = engineRef.current;
    if (!eng) return;
    const r: number[] = [];
    for (let i = 0; i < RAM_VIEW_SIZE; i++) r.push(eng.readRam(base + i));
    setRam(r);
    setRamBase(base);
  }, []);

  const loadProgram = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return false;
    try {
      eng.loadString(hack);
      // Disassemble
      const size = eng.romSize();
      const end = Math.min(size, 200);
      if (end > 0) {
        setDisasm(eng.disassembleRange(0, end));
      }
      syncState();
      return true;
    } catch (e) {
      setError(String(e));
      syncState();
      return false;
    }
  }, [hack, syncState]);

  const handleStep = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    if (eng.getState() === 0 /* READY */ && eng.romSize() === 0) {
      if (!loadProgram()) return;
    }
    try {
      eng.step();
    } catch (e) {
      setError(String(e));
    }
    syncState();
  }, [loadProgram, syncState]);

  const handleRun = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    if (eng.getState() === 0 && eng.romSize() === 0) {
      if (!loadProgram()) return;
    }
    runningRef.current = true;
    setRunning(true);

    const tick = () => {
      if (!runningRef.current || !eng) return;
      try {
        const s = eng.runFor(BATCH_SIZE);
        syncState();
        if (s === 3 /* HALTED */ || s === 4 /* ERROR */ || s === 2 /* PAUSED */) {
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
  }, [loadProgram, syncState]);

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
    setDisasm([]);
    setCpuStats(null);
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

  const handleRamGo = useCallback(() => {
    const addr = parseInt(ramInput, 10);
    if (!isNaN(addr) && addr >= 0) {
      updateRamView(addr);
    }
  }, [ramInput, updateRamView]);

  const stateName = stateToName(state);
  const pc = regs.PC;

  return (
    <div className="tab-panel" ref={tabRef} tabIndex={0}>
      <ControlBar
        onRun={handleRun}
        onStep={handleStep}
        onReset={handleReset}
        onPause={handlePause}
        running={running}
        hideStepOver
        hideStepOut
      />
      <div className="tab-content">
        {/* Left: code editor */}
        <div style={{ flex: 1, overflow: "hidden" }}>
          <Editor
            label="Hack Assembly (.hack)"
            value={hack}
            onChange={setHack}
            accept=".hack,.asm"
          />
        </div>

        {/* Middle: disassembly + screen */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <div className="panel" style={{ flex: 1, overflow: "hidden" }}>
            <div className="panel-header">Disassembly</div>
            <div className="panel-body disassembly">
              {disasm.map((line, i) => (
                <div key={i} className={`disasm-line ${i === pc ? "current" : ""}`}>
                  <span className="addr">{i}</span>
                  <span className="instr">{line}</span>
                </div>
              ))}
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
              <ScreenCanvas readRam={readRam} width={256} />
            </div>
          </div>
        </div>

        {/* Right: registers + RAM + stats */}
        <div style={{ width: 220, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <div className="panel">
            <div className="panel-header">Registers</div>
            <div className="panel-body">
              <RegisterDisplay registers={regs} />
            </div>
          </div>
          <div className="panel" style={{ flex: 1 }}>
            <div className="panel-header">
              RAM
            </div>
            <div style={{ display: "flex", gap: 4, padding: "4px 8px", background: "var(--bg-surface2)" }}>
              <input
                className="ram-addr-input"
                type="text"
                value={ramInput}
                onChange={(e) => setRamInput(e.target.value)}
                onKeyDown={(e) => { if (e.key === "Enter") handleRamGo(); e.stopPropagation(); }}
                placeholder="Address"
              />
              <button className="ram-go-btn" onClick={handleRamGo}>Go</button>
            </div>
            <div className="panel-body ram-viewer">
              <table>
                <thead>
                  <tr>
                    <th className="addr">Addr</th>
                    <th>Value</th>
                  </tr>
                </thead>
                <tbody>
                  {ram.map((v, i) => (
                    <tr key={i}>
                      <td className="addr">{ramBase + i}</td>
                      <td>{v}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
          {cpuStats && (
            <div className="panel">
              <div className="panel-header">Stats</div>
              <div className="panel-body">
                <RegisterDisplay
                  registers={{
                    Instructions: cpuStats.instructions_executed,
                    "A-instr": cpuStats.a_instruction_count,
                    "C-instr": cpuStats.c_instruction_count,
                    Jumps: cpuStats.jump_count,
                    "Mem reads": cpuStats.memory_reads,
                    "Mem writes": cpuStats.memory_writes,
                  }}
                />
              </div>
            </div>
          )}
        </div>
      </div>
      <StatusBar
        state={stateName}
        error={stateName === "error" ? error : undefined}
        stats={{
          Instructions: cpuStats?.instructions_executed ?? 0,
        }}
      />
    </div>
  );
}
