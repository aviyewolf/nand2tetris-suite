/**
 * Map browser KeyboardEvent to Hack keyboard code.
 * Returns null if the key is not mappable.
 *
 * Hack key codes (from nand2tetris spec):
 *   newline=128, backspace=129, left=130, up=131, right=132, down=133,
 *   home=134, end=135, pageUp=136, pageDown=137, insert=138, delete=139,
 *   escape=140, F1..F12=141..152
 *   Printable ASCII maps directly (32–126).
 */
export function mapKeyToHack(e: KeyboardEvent): number | null {
  // Special keys
  switch (e.key) {
    case "Enter":     return 128;
    case "Backspace": return 129;
    case "ArrowLeft": return 130;
    case "ArrowUp":   return 131;
    case "ArrowRight":return 132;
    case "ArrowDown": return 133;
    case "Home":      return 134;
    case "End":       return 135;
    case "PageUp":    return 136;
    case "PageDown":  return 137;
    case "Insert":    return 138;
    case "Delete":    return 139;
    case "Escape":    return 140;
    case "F1":  return 141;
    case "F2":  return 142;
    case "F3":  return 143;
    case "F4":  return 144;
    case "F5":  return 145;
    case "F6":  return 146;
    case "F7":  return 147;
    case "F8":  return 148;
    case "F9":  return 149;
    case "F10": return 150;
    case "F11": return 151;
    case "F12": return 152;
  }

  // Printable ASCII (single character)
  if (e.key.length === 1) {
    const code = e.key.charCodeAt(0);
    if (code >= 32 && code <= 126) return code;
  }

  return null;
}
