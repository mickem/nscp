import { MetricConstants } from '../constants';
import { MetricService, MetricsDictionary } from '../services';
import { defaultNSCPHandler } from './common.actions';

export interface MetricAction {
    type: string;
    metrics?: MetricsDictionary;
    error?: String;
    filter?: string;
}
export class MetricActions {

    static setFilter(text:string) {
        return { type: MetricConstants.SET_FILTER, filter: text }
    }

    static list(refresh: boolean = false) {
        return defaultNSCPHandler<MetricAction>(() => MetricService.list(refresh),
            () => { return { type: MetricConstants.LIST.REQUEST } },
            (metrics: MetricsDictionary) : MetricAction => { return { type: MetricConstants.LIST.SUCCESS, metrics } },
            (error: string) : MetricAction => { return { type: MetricConstants.LIST.FAILURE, error } }
        );
    }

}
