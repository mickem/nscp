import { handleResponse, CachedValue, getGetHeader, getUrl, ItemWithId } from './common.service';
import { Query } from './query.service';

export interface Module extends ItemWithId {
    name: string;
    title: string;
    description: string;
    enabled: boolean;
    id: string;
    load_url: string;
    loaded: boolean;
    metadata: {
        [key: string]: string;
    }
    module_url: string;
    unload_url: string;
    // INternal state fields
    queries : Query[];
}

export class ModuleService {

    static cachedList = new CachedValue<Module[]>();
    static list(refresh:boolean): Promise<Module[]> {
        if (!refresh && ModuleService.cachedList.isCached()) {
            return Promise.resolve(ModuleService.cachedList.get());
        } else {
            return fetch(getUrl('/modules?all=true'), getGetHeader())
                .then(handleResponse)
                .then(modules => {
                    return ModuleService.cachedList.set(modules);
                });
        }
    }

    static enable(module: Module): Promise<Module> {
        return fetch(getUrl(`/modules/${module.name}/commands/enable`), getGetHeader())
            .then(handleResponse)
            .then(module => {
                return module;
            });
    }
    static disable(module: Module): Promise<Module> {
        return fetch(getUrl(`/modules/${module.name}/commands/disable`), getGetHeader())
            .then(handleResponse)
            .then(module => {
                return module;
            });
    }
    static load(module: Module): Promise<Module> {
        return fetch(getUrl(`/modules/${module.name}/commands/load`), getGetHeader())
            .then(handleResponse)
            .then(module => {
                return module;
            });
    }
    static unload(module: Module): Promise<Module> {
        return fetch(getUrl(`/modules/${module.name}/commands/unload`), getGetHeader())
            .then(handleResponse)
            .then(module => {
                return module;
            });
    }

}
