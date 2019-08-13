import { ModuleConstants } from '../constants';
import { Module, ModuleService } from '../services';
import { defaultNSCPHandler } from './common.actions';

export interface ModuleAction {
    type: string;
    modules?: Module[];
    module?: Module;
    error?: String;
    filter?: string;
}
export class ModuleActions {

    static setFilter(text:string) {
        return { type: ModuleConstants.SET_FILTER, filter: text }
    }

    static list(refresh: boolean = false) {
        return defaultNSCPHandler<ModuleAction>(() => ModuleService.list(refresh),
            () => { return { type: ModuleConstants.LIST.REQUEST } },
            (modules: Module[]) : ModuleAction => { return { type: ModuleConstants.LIST.SUCCESS, modules } },
            (error: string) : ModuleAction => { return { type: ModuleConstants.LIST.FAILURE, error } }
        );
    }

    static unload(module: Module) {
        return defaultNSCPHandler<ModuleAction>(() => ModuleService.unload(module),
            () => { return { type: ModuleConstants.UNLOAD.REQUEST } },
            (result: string) : ModuleAction => { return { type: ModuleConstants.UNLOAD.SUCCESS, module } },
            (error: string) : ModuleAction => { return { type: ModuleConstants.UNLOAD.FAILURE, error } }
        );
    }

    static load(module: Module) {
        return defaultNSCPHandler<ModuleAction>(() => ModuleService.load(module),
            () => { return { type: ModuleConstants.LOAD.REQUEST } },
            (result: string) : ModuleAction => { return { type: ModuleConstants.LOAD.SUCCESS, module } },
            (error: string) : ModuleAction => { return { type: ModuleConstants.LOAD.FAILURE, error } }
        );
    }

    static disable(module: Module) {
        return defaultNSCPHandler<ModuleAction>(() => ModuleService.disable(module),
            () => { return { type: ModuleConstants.DISABLE.REQUEST } },
            (result: string) : ModuleAction => { return { type: ModuleConstants.DISABLE.SUCCESS, module } },
            (error: string) : ModuleAction => { return { type: ModuleConstants.DISABLE.FAILURE, error } }
        );
    }

    static enable(module: Module) {
        return defaultNSCPHandler<ModuleAction>(() => ModuleService.enable(module),
            () => { return { type: ModuleConstants.ENABLE.REQUEST } },
            (result: string) : ModuleAction => { return { type: ModuleConstants.ENABLE.SUCCESS, module } },
            (error: string) : ModuleAction => { return { type: ModuleConstants.ENABLE.FAILURE, error } }
        );
    }

}
