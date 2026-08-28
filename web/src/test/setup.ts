import "@testing-library/jest-dom/vitest";
import { afterEach, vi } from "vitest";

// jsdom does not implement ResizeObserver, which @mui/x-charts and some MUI
// components rely on for sizing.
class ResizeObserverStub {
  observe() {}
  unobserve() {}
  disconnect() {}
}
if (!("ResizeObserver" in globalThis)) {
  (globalThis as { ResizeObserver?: unknown }).ResizeObserver = ResizeObserverStub;
}

// Node's undici Request (used under vitest) rejects the relative URLs that
// fetchBaseQuery produces ("/api/..."); resolve them against the jsdom origin
// the way a browser would.
const NativeRequest = globalThis.Request;
class RelativeUrlRequest extends NativeRequest {
  constructor(input: RequestInfo | URL, init?: RequestInit) {
    if (typeof input === "string" && input.startsWith("/")) {
      input = new URL(input, window.location.origin).toString();
    }
    super(input, init);
  }
}
globalThis.Request = RelativeUrlRequest;

// jsdom does not implement matchMedia, used by MUI's useMediaQuery.
if (!window.matchMedia) {
  window.matchMedia = (query: string): MediaQueryList =>
    ({
      matches: false,
      media: query,
      onchange: null,
      addListener: () => {},
      removeListener: () => {},
      addEventListener: () => {},
      removeEventListener: () => {},
      dispatchEvent: () => false,
    }) as MediaQueryList;
}

afterEach(() => {
  // Tests seed tokens through localStorage (the auth hook persists there);
  // make sure state never leaks from one test into the next.
  localStorage.clear();
  vi.unstubAllGlobals();
});
