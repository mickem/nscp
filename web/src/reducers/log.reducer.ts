import { LogConstants } from '../constants';
import { LogAction } from '../actions';
import { LogState } from '../state';
import { LogEntry } from '../services';

const initialState: LogState = {
  log: [],
}

export function log(state = initialState, action: LogAction): LogState {
  switch (action.type) {
    case LogConstants.LIST.REQUEST:
      return {
        ...state,
        loading: true,
      };
    case LogConstants.LIST.SUCCESS:
      return {
        ...state,
        loading: false,
        log: action.log || []
      };
    case LogConstants.LIST.FAILURE:
      return {
        ...state,
        loading: false,
        error: action.error,
        log: []
      };
    default:
      return state
  }
}