import { describe, expect, it } from "vitest";
import { authSlice } from "./authSlice";

const { reducer, actions } = authSlice;

describe("authSlice", () => {
  it("starts without a token", () => {
    const state = reducer(undefined, { type: "@@INIT" });
    expect(state.token).toBeUndefined();
    expect(state.tokenInvalid).toBe(false);
  });

  it("stores the token on setToken and clears the invalid flag", () => {
    const invalid = reducer(undefined, actions.removeToken());
    const state = reducer(invalid, actions.setToken("abc123"));
    expect(state.token).toBe("abc123");
    expect(state.tokenInvalid).toBe(false);
  });

  it("drops the token and flags it invalid on removeToken", () => {
    const loggedIn = reducer(undefined, actions.setToken("abc123"));
    const state = reducer(loggedIn, actions.removeToken());
    expect(state.token).toBeUndefined();
    expect(state.tokenInvalid).toBe(true);
  });
});
