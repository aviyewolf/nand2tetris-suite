import { useMemo } from "react";

interface OutputTableProps {
  /** Pipe-delimited text from the HDL engine (header + rows) */
  text: string;
}

/**
 * Parses the pipe-delimited output produced by the HDL test runner into an
 * HTML table.  Lines that don't match the expected column count (like
 * comparison error markers) are shown as full-width rows.
 */
export function OutputTable({ text }: OutputTableProps) {
  const { headers, rows } = useMemo(() => parseTable(text), [text]);

  if (headers.length === 0) return null;

  return (
    <table className="output-table">
      <thead>
        <tr>
          {headers.map((h, i) => (
            <th key={i}>{h}</th>
          ))}
        </tr>
      </thead>
      <tbody>
        {rows.map((row, i) => (
          <tr key={i} className={row.mismatch ? "mismatch" : ""}>
            {row.cells.map((cell, j) => (
              <td key={j}>{cell}</td>
            ))}
          </tr>
        ))}
      </tbody>
    </table>
  );
}

interface ParsedRow {
  cells: string[];
  mismatch: boolean;
}

function parseTable(text: string): {
  headers: string[];
  rows: ParsedRow[];
} {
  const lines = text
    .split("\n")
    .map((l) => l.trim())
    .filter((l) => l.length > 0);

  if (lines.length === 0) return { headers: [], rows: [] };

  const splitPipe = (line: string) =>
    line
      .split("|")
      .map((c) => c.trim())
      .filter((c) => c.length > 0);

  const headers = splitPipe(lines[0]!);
  const rows: ParsedRow[] = [];

  for (let i = 1; i < lines.length; i++) {
    const cells = splitPipe(lines[i]!);
    // Skip separator-only lines
    if (cells.length === 0) continue;
    rows.push({ cells, mismatch: false });
  }

  return { headers, rows };
}
