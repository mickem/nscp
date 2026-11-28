// What the module page around a panel offers it: where its save bar goes
// (pinned to the bottom of the window, below the lists that follow the
// panel), and the lists' paths, so Show config can print them too.
import { createContext } from "react";

export interface PageFrame {
  saveBar: HTMLElement | null;
  collectionPaths: string[];
}

export const PageFrameContext = createContext<PageFrame | null>(null);
