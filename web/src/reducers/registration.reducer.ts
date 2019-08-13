import { UserConstants } from '../constants';
import { UserAction } from '../actions';
import { RegistrationState } from '../state';

export function registration(state : RegistrationState = {}, action: UserAction) : RegistrationState {
  switch (action.type) {
    case UserConstants.REGISTER_REQUEST:
      return { registering: true };
    case UserConstants.REGISTER_SUCCESS:
      return {};
    case UserConstants.REGISTER_FAILURE:
      return {};
    default:
      return state
  }
}