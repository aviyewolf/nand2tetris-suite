interface StatusBarProps {
  /** Engine state name: "ready" | "running" | "paused" | "halted" | "error" */
  state: string;
  /** Error message (shown when state === "error") */
  error?: string;
  /** Key-value stats to display */
  stats?: Record<string, string | number>;
}

const STATE_NAMES = ["ready", "running", "paused", "halted", "error"] as const;

export function stateToName(n: number): string {
  return STATE_NAMES[n] ?? "unknown";
}

export function StatusBar({ state, error, stats }: StatusBarProps) {
  return (
    <div className="status-bar">
      <span className={`state ${state}`}>{state.toUpperCase()}</span>
      {error && <span className="error-msg">{error}</span>}
      {stats &&
        Object.entries(stats).map(([k, v]) => (
          <span key={k} className="stat">
            {k}: {v}
          </span>
        ))}
    </div>
  );
}
