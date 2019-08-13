import { UserConstants } from '../constants';
import { UserAction } from '../actions';
import { AuthenticationState } from '../state';

const userJson = localStorage.getItem('user');
const user = userJson ? JSON.parse(userJson) : undefined;
const initialState : AuthenticationState = user ? { loggedIn: true, user } : {};

export function authentication(state = initialState, action : UserAction) : AuthenticationState {
  switch (action.type) {
    case UserConstants.LOGIN_REQUEST:
      return {
        loggingIn: true,
        user: action.user
      };
    case UserConstants.LOGIN_SUCCESS:
      return {
        loggedIn: true,
        user: action.user
      };
    case UserConstants.LOGIN_FAILURE:
      return {};
    case UserConstants.LOGOUT:
      return {};
    default:
      return state
  }
}