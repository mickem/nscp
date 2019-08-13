import { handleResponse, CachedValue, getGetHeader, getUrl, ItemWithId } from './common.service';

export interface Query extends ItemWithId {
    name: string;
    description: string;
    title: string;
    plugin: string;
    metadata: {
        [key: string]: string;
    }
}

export class QueryService {

    static cachedList = new CachedValue<Query[]>();
    static list(refresh:boolean): Promise<Query[]> {
        if (!refresh && QueryService.cachedList.isCached()) {
            return Promise.resolve(QueryService.cachedList.get());
        } else {
            return fetch(getUrl('/queries'), getGetHeader())
                .then(handleResponse)
                .then(Querys => {
                    return QueryService.cachedList.set(Querys);
                });
        }
    }
}
