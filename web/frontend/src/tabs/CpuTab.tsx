import { useState, useCallback, useRef, useEffect } from "react";
import { useWasm } from "../wasm/loader";
import type { CPUEngine as CPUEngineType } from "../wasm/types";
import { Editor } from "../components/Editor";
import { ControlBar } from "../components/ControlBar";
import { StatusBar, stateToName } from "../components/StatusBar";
import { RegisterDisplay } from "../components/RegisterDisplay";

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

export function CpuTab() {
  const wasm = useWasm();
  const engineRef = useRef<CPUEngineType | null>(null);
  const runningRef = useRef(false);

  const [hack, setHack] = useState(HACK_PLACEHOLDER);
  const [state, setState] = useState(0);
  const [error, setError] = useState("");
  const [running, setRunning] = useState(false);
  const [regs, setRegs] = useState({ A: 0, D: 0, PC: 0 });
  const [ram, setRam] = useState<number[]>([]);
  const [disasm, setDisasm] = useState<string[]>([]);
  const [instrCount, setInstrCount] = useState(0);

  useEffect(() => {
    const eng = new wasm.CPUEngine();
    engineRef.current = eng;
    return () => {
      eng.delete();
      engineRef.current = null;
    };
  }, [wasm]);

  const syncState = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    const s = eng.getState();
    setState(s);
    setError(eng.getErrorMessage());
    setRegs({ A: eng.getA(), D: eng.getD(), PC: eng.getPC() });
    // Read first 24 RAM words
    const r: number[] = [];
    for (let i = 0; i < 24; i++) r.push(eng.readRam(i));
    setRam(r);
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
    setInstrCount((c) => c + 1);
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
        setInstrCount((c) => c + BATCH_SIZE);
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
    setInstrCount(0);
    setDisasm([]);
    syncState();
  }, [syncState]);

  const stateName = stateToName(state);
  const pc = regs.PC;

  return (
    <div className="tab-panel">
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

        {/* Middle: disassembly */}
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

        {/* Right: registers + RAM */}
        <div style={{ width: 220, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <div className="panel">
            <div className="panel-header">Registers</div>
            <div className="panel-body">
              <RegisterDisplay registers={regs} />
            </div>
          </div>
          <div className="panel" style={{ flex: 1 }}>
            <div className="panel-header">RAM</div>
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
                      <td className="addr">{i}</td>
                      <td>{v}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
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
