import { createSlice, PayloadAction } from "@reduxjs/toolkit";

interface DashboardState {
  refreshRate: number;
  selectedNic: string;
  selectedDisk: string;
}

const initialState: DashboardState = {
  refreshRate: 10000,
  selectedNic: "",
  selectedDisk: "",
};

export const dashboardSlice = createSlice({
  name: "dashboard",
  initialState,
  reducers: {
    setRefreshRate(state, action: PayloadAction<number>) {
      state.refreshRate = action.payload;
    },
    setSelectedNic(state, action: PayloadAction<string>) {
      state.selectedNic = action.payload;
    },
    setSelectedDisk(state, action: PayloadAction<string>) {
      state.selectedDisk = action.payload;
    },
  },
});

export const { setRefreshRate, setSelectedNic, setSelectedDisk } = dashboardSlice.actions;

