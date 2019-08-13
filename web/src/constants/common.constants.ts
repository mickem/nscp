export function defaultRequestSet(prefix:string) {
    return {
        REQUEST : `${prefix}_REQUEST`,
        SUCCESS : `${prefix}_SUCCESS`,
        FAILURE : `${prefix}_FAILURE`,
    }
}