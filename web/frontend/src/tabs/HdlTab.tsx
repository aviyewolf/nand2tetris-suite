import { useState, useCallback, useRef, useEffect } from "react";
import { useWasm } from "../wasm/loader";
import type { HDLEngine as HDLEngineType } from "../wasm/types";
import { Editor } from "../components/Editor";
import { ControlBar } from "../components/ControlBar";
import { StatusBar, stateToName } from "../components/StatusBar";
import { OutputTable } from "../components/OutputTable";

const HDL_PLACEHOLDER = `// Load an HDL file or paste chip definition here
CHIP Not {
    IN in;
    OUT out;

    PARTS:
    Nand(a=in, b=in, out=out);
}`;

const TST_PLACEHOLDER = `// Load a test script (.tst) here
load Not.hdl,
output-list in%B3.1.3 out%B3.1.3;

set in 0, eval, output;
set in 1, eval, output;`;

export function HdlTab() {
  const wasm = useWasm();
  const engineRef = useRef<HDLEngineType | null>(null);
  const preparedRef = useRef(false);

  const [hdl, setHdl] = useState(HDL_PLACEHOLDER);
  const [tst, setTst] = useState(TST_PLACEHOLDER);
  const [cmp, setCmp] = useState("");
  const [output, setOutput] = useState("");
  const [state, setState] = useState(0);
  const [error, setError] = useState("");
  const [stats, setStats] = useState({ evals: 0, rows: 0 });

  // Create engine on mount, destroy on unmount
  useEffect(() => {
    const eng = new wasm.HDLEngine();
    engineRef.current = eng;
    return () => {
      eng.delete();
      engineRef.current = null;
    };
  }, [wasm]);

  const syncState = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    setState(eng.getState());
    setOutput(eng.getOutputTable());
    setError(eng.getErrorMessage());
    const s = eng.getStats();
    setStats({ evals: s.eval_count, rows: s.output_rows });
  }, []);

  const handleRunTest = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    preparedRef.current = false;
    try {
      eng.runTestString(tst, cmp);
    } catch (e) {
      setError(String(e));
    }
    syncState();
  }, [tst, cmp, syncState]);

  const handleStep = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    // If not yet prepared, load HDL and prepare the test for stepping
    if (!preparedRef.current) {
      try {
        eng.loadString(hdl);
        eng.prepareTest(tst, cmp);
        preparedRef.current = true;
      } catch (e) {
        setError(String(e));
        syncState();
        return;
      }
    }
    try {
      eng.stepTest();
    } catch (e) {
      setError(String(e));
    }
    syncState();
  }, [hdl, tst, cmp, syncState]);

  const handleReset = useCallback(() => {
    const eng = engineRef.current;
    if (!eng) return;
    eng.reset();
    preparedRef.current = false;
    setOutput("");
    setError("");
    setStats({ evals: 0, rows: 0 });
    syncState();
  }, [syncState]);

  const stateName = stateToName(state);

  return (
    <div className="tab-panel">
      <ControlBar
        onRun={handleRunTest}
        onStep={handleStep}
        onReset={handleReset}
        hideStepOver
        hideStepOut
      />
      <div className="tab-content">
        <div style={{ flex: 1, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <Editor
            label="HDL"
            value={hdl}
            onChange={setHdl}
            accept=".hdl"
          />
          <Editor
            label="Test Script (.tst)"
            value={tst}
            onChange={setTst}
            accept=".tst"
          />
          <Editor
            label="Compare (.cmp)"
            value={cmp}
            onChange={setCmp}
            accept=".cmp"
          />
        </div>
        <div style={{ flex: 1, display: "flex", flexDirection: "column", gap: 8, overflow: "hidden" }}>
          <div className="panel" style={{ flex: 1 }}>
            <div className="panel-header">Output</div>
            <div className="panel-body">
              <OutputTable text={output} />
            </div>
          </div>
        </div>
      </div>
      <StatusBar
        state={stateName}
        error={stateName === "error" ? error : undefined}
        stats={{
          Evals: stats.evals,
          "Output rows": stats.rows,
        }}
      />
    </div>
  );
}
