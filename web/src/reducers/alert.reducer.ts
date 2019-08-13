import { AlertConstants } from '../constants';
import { AlertAction } from '../actions';
import { AlertState } from '../state';

export function alert(state : AlertState = {}, action : AlertAction) : AlertState {
  switch (action.type) {
    case AlertConstants.SUCCESS:
      return {
        type: 'success',
        message: action.message
      };
    case AlertConstants.ERROR:
      return {
        type: 'error',
        message: action.message
      };
    case AlertConstants.CLEAR:
      return {};
    default:
      return state
  }
}