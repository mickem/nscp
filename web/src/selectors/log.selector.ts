import { LogState } from '../state';
import { LogEntry } from '../services';

export const getLog = (state: LogState): LogEntry[] => state.log || []
