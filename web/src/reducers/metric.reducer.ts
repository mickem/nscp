import { MetricConstants } from '../constants';
import { MetricAction } from '../actions';
import { MetricState } from '../state';

const initialState: MetricState = {
  metrics: {},
}

export function metric(state = initialState, action: MetricAction): MetricState {
  switch (action.type) {
    case MetricConstants.LIST.REQUEST:
      return {
        ...state,
        loading: true,
      };
    case MetricConstants.LIST.SUCCESS:
      return {
        ...state,
        loading: false,
        metrics: action.metrics || {}
      };
    case MetricConstants.LIST.FAILURE:
      return {
        ...state,
        loading: false,
        error: action.error,
        metrics: {}
      };
    case MetricConstants.SET_FILTER:
      return {
        ...state,
        visibilityFilter: action.filter,
      };
    default:
      return state
  }
}