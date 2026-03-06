import { useEffect, useRef, useCallback } from "react";
import { EditorState } from "@codemirror/state";
import { EditorView, keymap, lineNumbers } from "@codemirror/view";
import { defaultKeymap, history, historyKeymap } from "@codemirror/commands";
import { searchKeymap, highlightSelectionMatches } from "@codemirror/search";
import { syntaxHighlighting, defaultHighlightStyle } from "@codemirror/language";

interface EditorProps {
  /** Label shown in the toolbar */
  label: string;
  /** Controlled value */
  value: string;
  /** Called when user edits */
  onChange: (value: string) => void;
  /** Accepted file extensions, e.g. ".hdl,.tst" */
  accept?: string;
  /** Make editor read-only */
  readOnly?: boolean;
}

/** CodeMirror 6 editor with a file-upload toolbar. */
export function Editor({ label, value, onChange, accept, readOnly }: EditorProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const viewRef = useRef<EditorView | null>(null);
  const fileRef = useRef<HTMLInputElement>(null);

  // Stable callback ref so extensions don't recreate on every render
  const onChangeRef = useRef(onChange);
  onChangeRef.current = onChange;

  useEffect(() => {
    if (!containerRef.current) return;

    const updateListener = EditorView.updateListener.of((update) => {
      if (update.docChanged) {
        onChangeRef.current(update.state.doc.toString());
      }
    });

    const state = EditorState.create({
      doc: value,
      extensions: [
        lineNumbers(),
        history(),
        syntaxHighlighting(defaultHighlightStyle),
        highlightSelectionMatches(),
        keymap.of([...defaultKeymap, ...historyKeymap, ...searchKeymap]),
        updateListener,
        EditorView.theme({
          "&": { height: "100%" },
          ".cm-scroller": { overflow: "auto" },
          ".cm-content": { caretColor: "var(--accent)" },
          "&.cm-focused .cm-cursor": { borderLeftColor: "var(--accent)" },
          ".cm-gutters": {
            background: "var(--bg-surface2)",
            color: "var(--text-dim)",
            border: "none",
          },
          ".cm-activeLine": { background: "rgba(122,162,247,0.08)" },
          ".cm-activeLineGutter": { background: "rgba(122,162,247,0.12)" },
        }),
        EditorView.baseTheme({
          "&": { background: "var(--bg-surface)" },
          ".cm-content": { color: "var(--text)" },
        }),
        ...(readOnly ? [EditorState.readOnly.of(true)] : []),
      ],
    });

    const view = new EditorView({ state, parent: containerRef.current });
    viewRef.current = view;

    return () => {
      view.destroy();
      viewRef.current = null;
    };
    // Intentionally only run once on mount
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Sync external value changes into the editor
  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const current = view.state.doc.toString();
    if (current !== value) {
      view.dispatch({
        changes: { from: 0, to: current.length, insert: value },
      });
    }
  }, [value]);

  const handleFile = useCallback(() => {
    const file = fileRef.current?.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      if (typeof reader.result === "string") {
        onChange(reader.result);
      }
    };
    reader.readAsText(file);
    // Reset so the same file can be selected again
    if (fileRef.current) fileRef.current.value = "";
  }, [onChange]);

  return (
    <div className="editor-wrapper panel">
      <div className="editor-toolbar">
        <label>{label}</label>
        <input
          ref={fileRef}
          type="file"
          accept={accept}
          onChange={handleFile}
        />
        <button
          className="file-btn"
          onClick={() => fileRef.current?.click()}
        >
          Open file
        </button>
      </div>
      <div className="editor-container" ref={containerRef} />
    </div>
  );
}
