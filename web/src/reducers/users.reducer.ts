import { UserAction } from '../actions';
import { UserState } from '../state';

export function users(state : UserState = {}, action : UserAction) : UserState {
  switch (action.type) {
    default:
      return state
  }
}