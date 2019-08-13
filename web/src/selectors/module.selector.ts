import { createSelector } from 'reselect'
import { ModuleState, QueryState } from '../state';
import { Module, Query } from '../services';
import { AppState } from '../reducers';

const getVisibilityFilter = (state: ModuleState) => state.visibilityFilter
const getModules = (state: ModuleState): Module[] => state.modules || []
const getQueries = (state: QueryState): Query[] => state.queries || []

type QueriesByModule = { [key:string]:Query[]; };

export const getQueriesByModule = createSelector(
    [getQueries],
    (queries: Query[]) => {
        const ret : QueriesByModule = {};
        queries.map(q => {
            if (q.plugin in ret) {
                ret[q.plugin].push(q);
            } else {
                ret[q.plugin] = [q];
            }
        })
        return ret;
    }
);

export const getDecoratedModules = createSelector(
    [getModules, getQueriesByModule],
    (modules: Module[], queries: QueriesByModule) => {
        return modules.map(m => ({ ...m,
            queries: queries[m.id],
        }));
    }
);

export const getVisibleModules = createSelector(
    [getVisibilityFilter, getDecoratedModules],
    (visibilityFilter, modules) => {
        if (!visibilityFilter) {
            return modules;
        }
        return modules.filter(function (item: Module) {
            return item.title.toLowerCase().search(
                visibilityFilter.toLowerCase()) !== -1
                || item.name.toLowerCase().search(
                    visibilityFilter.toLowerCase()) !== -1
                || item.description.toLowerCase().search(
                    visibilityFilter.toLowerCase()) !== -1;
        });
    }
)