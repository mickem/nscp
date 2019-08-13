import { createSelector } from 'reselect'
import { MetricState } from '../state';
import { MetricsDictionary } from '../services';

const getVisibilityFilter = (state: MetricState) => state.visibilityFilter
const getMetrics = (state: MetricState): MetricsDictionary => state.metrics || {}

export const getVisibleMetrics = createSelector(
    [getVisibilityFilter, getMetrics],
    (visibilityFilter, metrics) => {
        if (!visibilityFilter) {
            return metrics;
        }
        return metrics;
        /*.filter(function (item: Metric) {
            return item.title.toLowerCase().search(
                visibilityFilter.toLowerCase()) !== -1
                || item.name.toLowerCase().search(
                    visibilityFilter.toLowerCase()) !== -1
                || item.description.toLowerCase().search(
                    visibilityFilter.toLowerCase()) !== -1;
        });
        */
    }
)