import { defaultRequestSet } from "./common.constants";
const prefix = 'QUERY';

export const QueryConstants = {
    LIST: defaultRequestSet(`${prefix}_LIST`),

    SET_FILTER: `${prefix}SET_FILTER`,

};