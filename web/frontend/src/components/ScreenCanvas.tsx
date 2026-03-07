import { useRef, useEffect, useCallback } from "react";

/** Hack screen: 256 rows x 512 cols. Each word = 16 pixels, MSB-first. */
const SCREEN_WIDTH = 512;
const SCREEN_HEIGHT = 256;
const SCREEN_BASE = 16384;
const WORDS_PER_ROW = 32; // 512 / 16

interface ScreenCanvasProps {
  /** Read a RAM word at the given address. */
  readRam: (addr: number) => number;
  /** CSS width override (canvas is always 512x256 logical pixels). */
  width?: number;
}

export function ScreenCanvas({ readRam, width }: ScreenCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const imageData = ctx.createImageData(SCREEN_WIDTH, SCREEN_HEIGHT);
    const data = imageData.data;

    for (let row = 0; row < SCREEN_HEIGHT; row++) {
      for (let word = 0; word < WORDS_PER_ROW; word++) {
        const addr = SCREEN_BASE + row * WORDS_PER_ROW + word;
        const val = readRam(addr) & 0xffff;
        for (let bit = 0; bit < 16; bit++) {
          const col = word * 16 + bit;
          const pixelIndex = (row * SCREEN_WIDTH + col) * 4;
          // Hack screen: bit 0 = leftmost pixel (LSB-first per nand2tetris spec)
          const on = (val >> bit) & 1;
          const color = on ? 0 : 255; // 1 = black, 0 = white
          data[pixelIndex] = color;
          data[pixelIndex + 1] = color;
          data[pixelIndex + 2] = color;
          data[pixelIndex + 3] = 255;
        }
      }
    }

    ctx.putImageData(imageData, 0, 0);
  }, [readRam]);

  // Redraw periodically when mounted
  useEffect(() => {
    draw();
    const id = setInterval(draw, 100);
    return () => clearInterval(id);
  }, [draw]);

  const style: React.CSSProperties = width
    ? { width, height: width * (SCREEN_HEIGHT / SCREEN_WIDTH), imageRendering: "pixelated" }
    : { width: SCREEN_WIDTH, height: SCREEN_HEIGHT, imageRendering: "pixelated" as const };

  return (
    <canvas
      ref={canvasRef}
      width={SCREEN_WIDTH}
      height={SCREEN_HEIGHT}
      className="screen-canvas"
      style={style}
    />
  );
}
