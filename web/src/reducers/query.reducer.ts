import { QueryConstants } from '../constants';
import { QueryAction } from '../actions';
import { QueryState } from '../state';
import { Query } from '../services';
import { AppState } from '.';

const initialState: QueryState = {
  queries: [],
}

const filterQuerys = (queries: Query[], query: Query, action: { (m: Query): Query; }) => {
  return queries!.map(m => {
    if (m.id === query.id) {
      return action(m);
    }
    return m;
  })
}

export function query(state = initialState, action: QueryAction): QueryState {
  switch (action.type) {
    case QueryConstants.LIST.REQUEST:
      return {
        ...state,
        loading: true,
      };
    case QueryConstants.LIST.SUCCESS:
      return {
        ...state,
        loading: false,
        queries: action.queries || []
      };
    case QueryConstants.LIST.FAILURE:
      return {
        ...state,
        loading: false,
        error: action.error,
        queries: []
      };
    case QueryConstants.SET_FILTER:
      return {
        ...state,
        visibilityFilter: action.filter,
      };
    default:
      return state
  }
}