import { describe, expect, it } from "vitest";
import { HISTORY_SIZE, dashboardSlice } from "./dashboardSlice";

const { reducer, actions } = dashboardSlice;
const initial = reducer(undefined, { type: "@@INIT" });

describe("dashboardSlice", () => {
  it("defaults to a 10s refresh rate and zero-filled histories", () => {
    expect(initial.refreshRate).toBe(10000);
    expect(initial.cpuHistory.kernel).toHaveLength(HISTORY_SIZE);
    expect(initial.cpuHistory.kernel.every((v) => v === 0)).toBe(true);
    expect(initial.memHistory).toHaveLength(HISTORY_SIZE);
    expect(initial.hideDefaults).toBe(false);
  });

  it("appends samples while keeping the history size fixed", () => {
    let state = initial;
    state = reducer(state, actions.pushCpu({ kernel: 12, user: 34 }));
    expect(state.cpuHistory.kernel).toHaveLength(HISTORY_SIZE);
    expect(state.cpuHistory.kernel[HISTORY_SIZE - 1]).toBe(12);
    expect(state.cpuHistory.user[HISTORY_SIZE - 1]).toBe(34);
  });

  it("trims the oldest samples once the history is full", () => {
    let state = initial;
    for (let i = 0; i < HISTORY_SIZE + 5; i++) {
      state = reducer(state, actions.pushMem(i));
    }
    expect(state.memHistory).toHaveLength(HISTORY_SIZE);
    expect(state.memHistory[HISTORY_SIZE - 1]).toBe(HISTORY_SIZE + 4);
    expect(state.memHistory[0]).toBe(5);
  });

  it("resets all histories when the refresh rate changes", () => {
    let state = reducer(initial, actions.pushCpu({ kernel: 99, user: 99 }));
    state = reducer(state, actions.pushMem(55));
    state = reducer(state, actions.setRefreshRate(5000));
    expect(state.refreshRate).toBe(5000);
    expect(state.cpuHistory.kernel.every((v) => v === 0)).toBe(true);
    expect(state.memHistory.every((v) => v === 0)).toBe(true);
  });

  it("resets the network history when another NIC is selected", () => {
    let state = reducer(initial, actions.pushNetwork({ received: 100, sent: 200 }));
    expect(state.networkHistory.bytesReceived[HISTORY_SIZE - 1]).toBe(100);
    state = reducer(state, actions.setSelectedNic("eth1"));
    expect(state.selectedNic).toBe("eth1");
    expect(state.networkHistory.bytesReceived.every((v) => v === 0)).toBe(true);
    expect(state.networkHistory.bytesSent.every((v) => v === 0)).toBe(true);
  });

  it("toggles hideDefaults", () => {
    let state = reducer(initial, actions.toggleHideDefaults());
    expect(state.hideDefaults).toBe(true);
    state = reducer(state, actions.toggleHideDefaults());
    expect(state.hideDefaults).toBe(false);
    state = reducer(state, actions.setHideDefaults(true));
    expect(state.hideDefaults).toBe(true);
  });
});
