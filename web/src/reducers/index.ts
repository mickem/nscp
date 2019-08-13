import { combineReducers } from 'redux';

import { authentication } from './authentication.reducer';
import { registration } from './registration.reducer';
import { users } from './users.reducer';
import { alert } from './alert.reducer';
import { module } from './module.reducer';
import { query } from './query.reducer';
import { log } from './log.reducer';
import { metric } from './metric.reducer';

const rootReducer = combineReducers({
  authentication,
  registration,
  users,
  alert,
  module,
  query,
  log,
  metric
});

export default rootReducer;
export type AppState = ReturnType<typeof rootReducer>;