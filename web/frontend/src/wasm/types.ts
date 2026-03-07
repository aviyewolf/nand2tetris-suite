// =============================================================================
// TypeScript declarations for the Embind-generated WASM module
// =============================================================================

/** Enum values mirroring C++ HDLState */
export const enum HDLState {
  READY = 0,
  RUNNING = 1,
  PAUSED = 2,
  HALTED = 3,
  ERROR = 4,
}

export const enum CPUState {
  READY = 0,
  RUNNING = 1,
  PAUSED = 2,
  HALTED = 3,
  ERROR = 4,
}

export const enum VMState {
  READY = 0,
  RUNNING = 1,
  PAUSED = 2,
  HALTED = 3,
  ERROR = 4,
}

export const enum SegmentType {
  LOCAL = 0,
  ARGUMENT = 1,
  THIS = 2,
  THAT = 3,
  CONSTANT = 4,
  STATIC = 5,
  TEMP = 6,
  POINTER = 7,
}

// -- HDL Engine ---------------------------------------------------------------

export interface HDLStats {
  eval_count: number;
  output_rows: number;
}

export interface HDLEngine {
  loadString(source: string, name?: string): void;
  reset(): void;
  setInput(pin: string, value: number): void;
  getOutput(pin: string): number;
  eval(): void;
  tick(): void;
  tock(): void;
  runTestString(tst: string, cmp?: string, name?: string): HDLState;
  prepareTest(tst: string, cmp?: string, name?: string): HDLState;
  stepTest(): HDLState;
  getOutputTable(): string;
  hasComparisonError(): boolean;
  getState(): HDLState;
  getStats(): HDLStats;
  getErrorMessage(): string;
  delete(): void;
}

// -- CPU Engine ---------------------------------------------------------------

export interface DecodedInstruction {
  type: number;
  value: number;
  reads_memory: boolean;
  comp?: number;
  dest_a?: boolean;
  dest_d?: boolean;
  dest_m?: boolean;
  jump?: number;
}

export interface CPUStats {
  instructions_executed: number;
  a_instruction_count: number;
  c_instruction_count: number;
  jump_count: number;
  memory_reads: number;
  memory_writes: number;
}

export interface CPUEngine {
  loadString(hack: string): void;
  reset(): void;
  run(): CPUState;
  runFor(n: number): CPUState;
  step(): CPUState;
  pause(): void;
  getState(): CPUState;
  getA(): number;
  getD(): number;
  getPC(): number;
  readRam(addr: number): number;
  writeRam(addr: number, value: number): void;
  readRom(addr: number): number;
  romSize(): number;
  getKeyboard(): number;
  setKeyboard(key: number): void;
  addBreakpoint(addr: number): void;
  removeBreakpoint(addr: number): void;
  clearBreakpoints(): void;
  hasBreakpoint(addr: number): boolean;
  getBreakpoints(): number[];
  disassemble(addr: number): string;
  disassembleRange(start: number, end: number): string[];
  getCurrentInstruction(): DecodedInstruction;
  getStats(): CPUStats;
  getErrorMessage(): string;
  getErrorLocation(): number;
  delete(): void;
}

// -- VM Engine ----------------------------------------------------------------

export interface CallFrame {
  return_address: number;
  function_name: string;
  num_args: number;
  num_locals: number;
  caller_file: string;
  caller_line: number;
}

export interface VMEngine {
  loadString(source: string, fileName?: string): void;
  setEntryPoint(fn: string): void;
  reset(): void;
  run(): VMState;
  runFor(n: number): VMState;
  step(): VMState;
  stepOver(): VMState;
  stepOut(): VMState;
  pause(): void;
  getState(): VMState;
  getPC(): number;
  getCommandCount(): number;
  getCurrentFunction(): string;
  getCallStack(): CallFrame[];
  getStack(): number[];
  getSP(): number;
  getSegment(seg: SegmentType, index: number, file?: string): number;
  commandToString(index: number): string;
  currentCommandString(): string;
  readRam(addr: number): number;
  writeRam(addr: number, value: number): void;
  addBreakpoint(index: number): void;
  addFunctionBreakpoint(fn: string, offset?: number): void;
  removeBreakpoint(index: number): void;
  clearBreakpoints(): void;
  getKeyboard(): number;
  setKeyboard(key: number): void;
  getErrorMessage(): string;
  getErrorLocation(): number;
  delete(): void;
}

// -- Jack Debugger ------------------------------------------------------------

export interface SourceEntry {
  jack_file: string;
  jack_line: number;
  vm_command_index: number;
  function_name: string;
}

export interface JackCallFrame {
  function_name: string;
  jack_file: string;
  jack_line: number;
  vm_command_index: number;
}

export interface JackVariableValue {
  name: string;
  type_name: string;
  kind: number;
  index: number;
  raw_value: number;
  signed_value: number;
}

export interface InspectedField {
  field_name: string;
  type_name: string;
  raw_value: number;
  signed_value: number;
  is_reference: boolean;
}

export interface InspectedObject {
  class_name: string;
  heap_address: number;
  fields: InspectedField[];
}

export interface InspectedArray {
  heap_address: number;
  length: number;
  elements: number[];
}

export interface JackBreakpoint {
  file: string;
  line: number;
}

export interface JackDebugger {
  load(vm: string, smap: string, name?: string): void;
  loadVM(vm: string, name?: string): void;
  loadSourceMap(smap: string, name?: string): void;
  loadWithSources(jackSources: [string, string][], vmSource: string, name?: string): void;
  setEntryPoint(fn: string): void;
  reset(): void;
  step(): VMState;
  stepOver(): VMState;
  stepOut(): VMState;
  run(): VMState;
  runFor(n: number): VMState;
  pause(): void;
  getState(): VMState;
  isRunning(): boolean;
  getCurrentSource(): SourceEntry | null;
  getCurrentFunction(): string;
  getCallStack(): JackCallFrame[];
  addBreakpoint(file: string, line: number): boolean;
  removeBreakpoint(file: string, line: number): boolean;
  clearBreakpoints(): void;
  getBreakpoints(): JackBreakpoint[];
  getVariable(name: string): JackVariableValue | null;
  getAllVariables(): JackVariableValue[];
  evaluate(expr: string): number | null;
  inspectObject(addr: number, cls: string): InspectedObject;
  inspectThis(): InspectedObject;
  inspectArray(addr: number, len: number): InspectedArray;
  getErrorMessage(): string;
  delete(): void;
}

// -- Module factory -----------------------------------------------------------

export interface N2TModule {
  HDLEngine: { new (): HDLEngine };
  CPUEngine: { new (): CPUEngine };
  VMEngine: { new (): VMEngine };
  JackDebugger: { new (): JackDebugger };
  // Enum objects
  HDLState: Record<string, number>;
  CPUState: Record<string, number>;
  VMState: Record<string, number>;
  SegmentType: Record<string, number>;
}

export type CreateN2TModule = () => Promise<N2TModule>;
