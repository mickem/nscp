import { handleResponse, CachedValue, getGetHeader, getUrl } from './common.service';


export type MetricsDictionary = { [key:string]:string|number; };

export class MetricService {

    static cachedList = new CachedValue<MetricsDictionary>();
    static list(refresh:boolean): Promise<MetricsDictionary> {
        if (!refresh && MetricService.cachedList.isCached()) {
            return Promise.resolve(MetricService.cachedList.get());
        } else {
            return fetch(getUrl('/metrics'), getGetHeader())
                .then(handleResponse)
                .then(Metrics => {
                    return MetricService.cachedList.set(Metrics);
                });
        }
    }
}
