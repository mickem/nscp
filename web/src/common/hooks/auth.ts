import { useAppDispatch, useAppSelector } from "../../store/store";
import { authSlice } from "../authSlice";
import { nsclientApi, useLoginMutation, useLogoutMutation } from "../../api/api.ts";

// How long logging out waits for the server to acknowledge the revoke before
// giving up on it. The revoke matters - it is what stops a captured bearer
// working for the rest of its eight hours - but it must never be what keeps a
// user signed in: an agent that has stopped, or a network that swallows the
// request, would otherwise hold the token in this browser (and in
// localStorage) for however long the platform's own timeout happens to be.
export const LOGOUT_REVOKE_TIMEOUT_MS = 2000;

export const useAuthentication = () => {
  const auth = useAppSelector((store) => store.auth);
  const dispatch = useAppDispatch();
  const [doLogin] = useLoginMutation();
  const [doLogout] = useLogoutMutation();

  const login = async (username: string, password: string) => {
    const token = await doLogin({ username, password }).unwrap();
    dispatch(authSlice.actions.setToken(token));
    localStorage.setItem("token", token);
  };

  const logout = async () => {
    // Revoke the token on the server first, while it is still in the store for
    // the base query to send: dropping it locally only makes this browser
    // forget it, and the token itself stays accepted until it expires eight
    // hours later. It carries whatever role the user has - `full` runs any
    // check and rewrites the settings - so anyone who picked it up from a
    // shared machine, a profile backup or a proxy log would still hold it.
    //
    // Neither a failure nor a hang here may leave the UI logged in: the server
    // may be gone, the token already expired, or the request simply never
    // answered, and in every case clearing the client state is still the right
    // outcome. A rejection is caught; a stall is cut short by aborting the
    // request, which then rejects into the same catch.
    const revoke = doLogout();
    const giveUp = setTimeout(() => revoke.abort(), LOGOUT_REVOKE_TIMEOUT_MS);
    try {
      await revoke.unwrap();
    } catch {
      // Deliberately ignored - see above.
    } finally {
      clearTimeout(giveUp);
    }
    dispatch(authSlice.actions.removeToken());
    // Drop the persisted copy now rather than leaving it for the next
    // restoreToken() to notice: between the two the token sits in
    // localStorage of a browser whose user believes they have logged out.
    localStorage.removeItem("token");
    // Wipe every cached query so the next login (or even a stale tab) doesn't
    // see data from the previous session.
    dispatch(nsclientApi.util.resetApiState());
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
