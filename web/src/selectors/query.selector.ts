import { createSelector } from 'reselect'
import { QueryState } from '../state';
import { Query } from '../services';

const getVisibilityFilter = (state: QueryState) => state.visibilityFilter
const getQueries = (state: QueryState): Query[] => state.queries || []

export const getVisibleQueries = createSelector(
    [getVisibilityFilter, getQueries],
    (visibilityFilter, queries) => {
        if (!visibilityFilter) {
            return queries;
        }
        return queries.filter(function (item: Query) {
            return item.title.toLowerCase().search(
                visibilityFilter.toLowerCase()) !== -1
                || item.name.toLowerCase().search(
                    visibilityFilter.toLowerCase()) !== -1
                || item.description.toLowerCase().search(
                    visibilityFilter.toLowerCase()) !== -1;
        });
    }
)