import { handleResponse, CachedValue, getGetHeader, getUrl } from './common.service';
export enum LogLevels {
    debug  = "debug"
}
export interface LogEntry {

    date: Date;
    file: string;
    level: LogLevels;
    line: number;
    message: string;
}

export class LogService {

    static cachedList = new CachedValue<LogEntry[]>();
    static list(refresh: boolean): Promise<LogEntry[]> {
        if (!refresh && LogService.cachedList.isCached()) {
            return Promise.resolve(LogService.cachedList.get());
        } else {
            return fetch(getUrl('/logs'), getGetHeader())
                .then(handleResponse)
                .then(logs => {
                    return LogService.cachedList.set(logs);
                });
        }
    }
}
