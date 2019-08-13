import { defaultRequestSet } from "./common.constants";
const prefix = 'METRIC';

export const MetricConstants = {
    LIST: defaultRequestSet(`${prefix}_LIST`),

    SET_FILTER: `${prefix}SET_FILTER`,

};