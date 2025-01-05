import { useAppDispatch, useAppSelector } from "../../store/store";
import { authSlice } from "../authSlice";
import { useLoginMutation } from "../../api/api.ts";

export const useAuthentication = () => {
  const auth = useAppSelector((store) => store.auth);
  const dispatch = useAppDispatch();
  const [doLogin] = useLoginMutation();

  const login = async (username: string, password: string) => {
    const token = await doLogin({ username, password }).unwrap();
    dispatch(authSlice.actions.setToken(token));
    localStorage.setItem("token", token);
  };

  const logout = async () => {
    dispatch(authSlice.actions.removeToken());
  };

  const restoreToken = () => {
    if (auth.token) {
      return auth.token;
    }
    if (auth.tokenInvalid) {
      localStorage.removeItem("token");
    }
    const token = localStorage.getItem("token");
    if (token) {
      dispatch(authSlice.actions.setToken(token));
    }
    return token;
  };

  return {
    isAuthenticated: auth.token !== undefined,
    token: auth.token,
    restoreToken,
    login,
    logout,
  };
};
