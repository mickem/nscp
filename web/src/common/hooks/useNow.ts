import { useEffect, useState } from "react";

/**
 * The current time in epoch milliseconds, re-read every `intervalMs`, for a
 * relative age ("12s ago") that keeps up while the page sits open. Read in a
 * hook rather than during render, which has to stay pure.
 */
export function useNow(intervalMs = 5000): number {
  const [now, setNow] = useState(() => Date.now());
  useEffect(() => {
    const timer = setInterval(() => setNow(Date.now()), intervalMs);
    return () => clearInterval(timer);
  }, [intervalMs]);
  return now;
}
