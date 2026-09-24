import { useState } from "react";

/**
 * Cap a host-sized list at `limit`, keeping whether the reader has opened it.
 *
 * A widget whose length the host decides - the volumes on a file server, the
 * interfaces on a hypervisor, the tags a fleet publishes - is unbounded, and
 * one long one pushes everything after it off the page. The fold is counted
 * in items rather than pixels on purpose: between two rows it reads as a
 * fold, while at an arbitrary height it cuts a row in half and reads as a
 * rendering bug.
 */
export function useCapped<T>(items: T[], limit: number) {
  const [showAll, setShowAll] = useState(false);
  return {
    shown: showAll ? items : items.slice(0, limit),
    hidden: Math.max(items.length - limit, 0),
    showAll,
    toggle: () => setShowAll((was) => !was),
  };
}
