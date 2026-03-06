interface RegisterDisplayProps {
  /** Key-value pairs to display, e.g. { "A": 42, "D": 0 } */
  registers: Record<string, number | string>;
}

/** Monospace key-value list for register / segment display. */
export function RegisterDisplay({ registers }: RegisterDisplayProps) {
  return (
    <div className="register-display">
      {Object.entries(registers).map(([name, value]) => (
        <div key={name} className="register-row">
          <span className="reg-name">{name}</span>
          <span className="reg-value">{value}</span>
        </div>
      ))}
    </div>
  );
}
