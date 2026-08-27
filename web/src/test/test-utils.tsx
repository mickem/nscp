import { ReactElement, ReactNode } from "react";
import { render } from "@testing-library/react";
import { configureStore } from "@reduxjs/toolkit";
import { Provider } from "react-redux";
import { MemoryRouter } from "react-router";
import { vi } from "vitest";
import { nsclientApi } from "../api/api";
import { authSlice } from "../common/authSlice";
import { dashboardSlice } from "../common/dashboardSlice";
import type { RootState } from "../store/store";

/**
 * Build a fresh store with the same shape as the production store so tests
 * never share RTK Query caches or auth state with each other.
 */
export function makeStore(preloadedState?: Partial<RootState>) {
  return configureStore({
    reducer: {
      [authSlice.name]: authSlice.reducer,
      [dashboardSlice.name]: dashboardSlice.reducer,
      [nsclientApi.reducerPath]: nsclientApi.reducer,
    },
    middleware: (getDefaultMiddleware) => getDefaultMiddleware().concat(nsclientApi.middleware),
    // Cast: RTK's preloadedState typing wants every slice, tests only seed some.
    preloadedState: preloadedState as never,
  });
}

export type TestStore = ReturnType<typeof makeStore>;

interface RenderOptions {
  store?: TestStore;
  preloadedState?: Partial<RootState>;
  /** Wrap in a MemoryRouter (needed by components using useNavigate & co). */
  route?: string;
  withRouter?: boolean;
}

/** Render a component inside Redux (and optionally router) providers. */
export function renderWithProviders(ui: ReactElement, options: RenderOptions = {}) {
  const store = options.store ?? makeStore(options.preloadedState);
  const withRouter = options.withRouter ?? true;

  const Wrapper = ({ children }: { children: ReactNode }) => {
    const inner = <Provider store={store}>{children}</Provider>;
    if (!withRouter) {
      return inner;
    }
    return <MemoryRouter initialEntries={[options.route ?? "/"]}>{inner}</MemoryRouter>;
  };

  return { store, ...render(ui, { wrapper: Wrapper }) };
}

export const authenticatedState: Partial<RootState> = {
  auth: { token: "test-token", tokenInvalid: false },
};

type RouteHandler = Response | ((req: Request) => Response | Promise<Response>);

export function jsonResponse(body: unknown, init: ResponseInit = {}): Response {
  return new Response(JSON.stringify(body), {
    status: 200,
    headers: { "Content-Type": "application/json" },
    ...init,
  });
}

/**
 * Stub the global fetch used by RTK Query's fetchBaseQuery. Routes are keyed
 * by URL pathname (e.g. "/api/v2/metrics"); query strings are ignored when
 * matching. Unrouted paths get a 404 so a missing mock is visible in the test
 * rather than hanging.
 */
export function installFetchMock(routes: Record<string, RouteHandler>) {
  const fetchMock = vi.fn(async (input: RequestInfo | URL, init?: RequestInit) => {
    const request = input instanceof Request ? input : new Request(input, init);
    const path = new URL(request.url, "http://localhost").pathname;
    const handler = routes[path];
    if (handler === undefined) {
      return new Response(`No fetch mock for ${path}`, { status: 404 });
    }
    if (typeof handler === "function") {
      return handler(request);
    }
    // Response bodies are single-use: clone so a route can serve repeatedly.
    return handler.clone();
  });
  vi.stubGlobal("fetch", fetchMock);
  return fetchMock;
}
