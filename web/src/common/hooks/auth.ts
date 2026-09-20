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

// Where the bearer is kept between page loads.
//
// sessionStorage, not localStorage: the token carries whatever role the user
// has (`full` runs any check and rewrites the settings) and lives for eight
// hours, and localStorage handed it to every later visit from the same browser
// profile - so it survived a restart, travelled in a profile backup, and was
// readable by any script on the origin for as long as it lasted. sessionStorage
// is scoped to the one tab and cleared when that tab closes, which is as close
// to "in memory" as a page that has to survive a reload can get. The cost is
// that a newly opened tab asks for credentials again.
const TOKEN_STORAGE_KEY = "token";

// Storage is not always there to be had: Safari in private mode throws on
// write, and a browser configured to block site data throws on read. Losing
// the stored token means logging in again, which is survivable; throwing out
// of login() or of the first render is not.
const readStoredToken = (): string | null => {
  try {
    return sessionStorage.getItem(TOKEN_STORAGE_KEY);
  } catch {
    return null;
  }
};

const writeStoredToken = (token: string) => {
  try {
    sessionStorage.setItem(TOKEN_STORAGE_KEY, token);
  } catch {
    // Kept in the redux store for this page's lifetime either way.
  }
};

const clearStoredToken = () => {
  try {
    sessionStorage.removeItem(TOKEN_STORAGE_KEY);
  } catch {
    // Nothing to clear if storage is unavailable.
  }
  try {
    // A token written by a build that used localStorage would otherwise sit
    // there until it expired, which is the exact exposure this moved away
    // from. Clear it on the way out of any session.
    localStorage.removeItem(TOKEN_STORAGE_KEY);
  } catch {
    // Ignored, as above.
  }
};

export const useAuthentication = () => {
  const auth = useAppSelector((store) => store.auth);
  const dispatch = useAppDispatch();
  const [doLogin] = useLoginMutation();
  const [doLogout] = useLogoutMutation();

  const login = async (username: string, password: string) => {
    const token = await doLogin({ username, password }).unwrap();
    dispatch(authSlice.actions.setToken(token));
    writeStoredToken(token);
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
    // restoreToken() to notice: between the two the token sits in storage of a
    // browser whose user believes they have logged out.
    clearStoredToken();
    // Wipe every cached query so the next login (or even a stale tab) doesn't
    // see data from the previous session.
    dispatch(nsclientApi.util.resetApiState());
  };

  const restoreToken = () => {
    if (auth.token) {
      return auth.token;
    }
    if (auth.tokenInvalid) {
      clearStoredToken();
    }
    const token = readStoredToken();
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
