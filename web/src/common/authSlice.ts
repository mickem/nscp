import { createSlice, PayloadAction } from "@reduxjs/toolkit";

interface AuthenticatedState {
  tokenInvalid: boolean;
  token?: string;
}

const initialState: AuthenticatedState = {
  tokenInvalid: false,
  token: undefined,
};

export const setToken = (token: string) => token;

export const authSlice = createSlice({
  name: "auth",
  initialState,
  reducers: {
    setToken(state, action: PayloadAction<string>) {
      state.token = setToken(action.payload);
      state.tokenInvalid = false;
    },
    removeToken(state) {
      state.token = undefined;
      state.tokenInvalid = true;
    },
  },
});
