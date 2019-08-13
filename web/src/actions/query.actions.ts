import { QueryConstants } from '../constants';
import { Query, QueryService } from '../services';
import { defaultNSCPHandler } from './common.actions';

export interface QueryAction {
    type: string;
    queries?: Query[];
    query?: Query;
    error?: String;
    filter?: string;
}
export class QueryActions {

    static setFilter(text:string) {
        return { type: QueryConstants.SET_FILTER, filter: text }
    }

    static list(refresh: boolean = false) {
        return defaultNSCPHandler<QueryAction>(() => QueryService.list(refresh),
            () => { return { type: QueryConstants.LIST.REQUEST } },
            (queries: Query[]) : QueryAction => { return { type: QueryConstants.LIST.SUCCESS, queries } },
            (error: string) : QueryAction => { return { type: QueryConstants.LIST.FAILURE, error } }
        );
    }

}
