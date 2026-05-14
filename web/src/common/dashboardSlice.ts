import { createSlice, PayloadAction } from "@reduxjs/toolkit";

export const HISTORY_SIZE = 30;

interface CpuHistory {
  kernel: number[];
  user: number[];
}

interface NetworkHistory {
  bytesReceived: number[];
  bytesSent: number[];
}

interface DiskIoHistory {
  readBytes: number[];
  writeBytes: number[];
}

interface DashboardState {
  refreshRate: number;
  selectedNic: string;
  selectedDisk: string;
  cpuHistory: CpuHistory;
  memHistory: number[];
  networkHistory: NetworkHistory;
  diskIoHistory: DiskIoHistory;
  // When true, the settings list views hide rows whose stored value matches
  // the schema default (or is empty and therefore falls back to the default).
  // Toggled from the top bar.
  hideDefaults: boolean;
}

const emptyCpu = (): CpuHistory => ({
  kernel: Array(HISTORY_SIZE).fill(0),
  user: Array(HISTORY_SIZE).fill(0),
});
const emptyMem = (): number[] => Array(HISTORY_SIZE).fill(0);
const emptyNetwork = (): NetworkHistory => ({
  bytesReceived: Array(HISTORY_SIZE).fill(0),
  bytesSent: Array(HISTORY_SIZE).fill(0),
});
const emptyDiskIo = (): DiskIoHistory => ({
  readBytes: Array(HISTORY_SIZE).fill(0),
  writeBytes: Array(HISTORY_SIZE).fill(0),
});

const initialState: DashboardState = {
  refreshRate: 10000,
  selectedNic: "",
  selectedDisk: "",
  cpuHistory: emptyCpu(),
  memHistory: emptyMem(),
  networkHistory: emptyNetwork(),
  diskIoHistory: emptyDiskIo(),
  hideDefaults: false,
};

function pushTrim(arr: number[], value: number) {
  arr.push(value);
  if (arr.length > HISTORY_SIZE) arr.splice(0, arr.length - HISTORY_SIZE);
}

export const dashboardSlice = createSlice({
  name: "dashboard",
  initialState,
  reducers: {
    setRefreshRate(state, action: PayloadAction<number>) {
      state.refreshRate = action.payload;
      // X-axis spacing changes with the refresh rate, so prior samples no
      // longer line up with the new time scale.
      state.cpuHistory = emptyCpu();
      state.memHistory = emptyMem();
      state.networkHistory = emptyNetwork();
      state.diskIoHistory = emptyDiskIo();
    },
    setSelectedNic(state, action: PayloadAction<string>) {
      state.selectedNic = action.payload;
      state.networkHistory = emptyNetwork();
    },
    setSelectedDisk(state, action: PayloadAction<string>) {
      state.selectedDisk = action.payload;
      state.diskIoHistory = emptyDiskIo();
    },
    pushCpu(state, action: PayloadAction<{ kernel?: number; user?: number }>) {
      if (action.payload.kernel !== undefined) pushTrim(state.cpuHistory.kernel, action.payload.kernel);
      if (action.payload.user !== undefined) pushTrim(state.cpuHistory.user, action.payload.user);
    },
    pushMem(state, action: PayloadAction<number>) {
      pushTrim(state.memHistory, action.payload);
    },
    pushNetwork(state, action: PayloadAction<{ received?: number; sent?: number }>) {
      if (action.payload.received !== undefined) pushTrim(state.networkHistory.bytesReceived, action.payload.received);
      if (action.payload.sent !== undefined) pushTrim(state.networkHistory.bytesSent, action.payload.sent);
    },
    pushDiskIo(state, action: PayloadAction<{ read?: number; write?: number }>) {
      if (action.payload.read !== undefined) pushTrim(state.diskIoHistory.readBytes, action.payload.read);
      if (action.payload.write !== undefined) pushTrim(state.diskIoHistory.writeBytes, action.payload.write);
    },
    toggleHideDefaults(state) {
      state.hideDefaults = !state.hideDefaults;
    },
    setHideDefaults(state, action: PayloadAction<boolean>) {
      state.hideDefaults = action.payload;
    },
  },
});

export const {
  setRefreshRate,
  setSelectedNic,
  setSelectedDisk,
  pushCpu,
  pushMem,
  pushNetwork,
  pushDiskIo,
  toggleHideDefaults,
  setHideDefaults,
} = dashboardSlice.actions;
