import { configureStore } from "@reduxjs/toolkit";
import { TypedUseSelectorHook, useDispatch, useSelector } from "react-redux";
import { nsclientApi } from "../api/api";
import { authSlice } from "../common/authSlice";

export const store = configureStore({
  reducer: {
    [authSlice.name]: authSlice.reducer,
    [nsclientApi.reducerPath]: nsclientApi.reducer,
  },
  middleware: (getDefaultMiddleware) => getDefaultMiddleware().concat(nsclientApi.middleware),
});

export type RootState = ReturnType<typeof store.getState>;
export type AppDispatch = typeof store.dispatch;
export const useAppDispatch = () => useDispatch<AppDispatch>();
export const useAppSelector: TypedUseSelectorHook<RootState> = useSelector;
