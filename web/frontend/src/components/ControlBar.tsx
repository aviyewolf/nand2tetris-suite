interface ControlBarProps {
  onRun?: () => void;
  onStep?: () => void;
  onStepOver?: () => void;
  onStepOut?: () => void;
  onReset?: () => void;
  onPause?: () => void;
  /** True while a batched run() is in progress */
  running?: boolean;
  /** Hide buttons that don't apply to this engine */
  hideStepOver?: boolean;
  hideStepOut?: boolean;
}

export function ControlBar({
  onRun,
  onStep,
  onStepOver,
  onStepOut,
  onReset,
  onPause,
  running,
  hideStepOver,
  hideStepOut,
}: ControlBarProps) {
  return (
    <div className="control-bar">
      {onRun && !running && (
        <button className="primary" onClick={onRun}>
          Run
        </button>
      )}
      {onPause && running && (
        <button onClick={onPause}>Pause</button>
      )}
      {onStep && (
        <button disabled={running} onClick={onStep}>
          Step
        </button>
      )}
      {!hideStepOver && onStepOver && (
        <button disabled={running} onClick={onStepOver}>
          Step Over
        </button>
      )}
      {!hideStepOut && onStepOut && (
        <button disabled={running} onClick={onStepOut}>
          Step Out
        </button>
      )}
      {onReset && (
        <button onClick={onReset}>Reset</button>
      )}
    </div>
  );
}
