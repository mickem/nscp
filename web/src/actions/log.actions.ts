import { LogConstants } from '../constants';
import { LogEntry, LogService } from '../services';
import { defaultNSCPHandler } from './common.actions';

export interface LogAction {
    type: string;
    log?: LogEntry[];
    error?: string;
    filter?: string;
}
export class LogActions {

    static list(refresh: boolean = false) {
        return defaultNSCPHandler<LogAction>(() => LogService.list(refresh),
            () => { return { type: LogConstants.LIST.REQUEST } },
            (log: LogEntry[]) : LogAction => { return { type: LogConstants.LIST.SUCCESS, log } },
            (error: string) : LogAction => { return { type: LogConstants.LIST.FAILURE, error } }
        );
    }

}
