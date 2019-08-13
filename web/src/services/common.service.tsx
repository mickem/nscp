import { UserService } from "./user.service";

export enum ErrorType {
    AUTH = '403_NOT_AUTHENTICATED',
    MISSING = '404_MISSING',
    GENERIC = '500_SERVER_ERROR',

}

export interface ItemWithId {
    id:string;
}

export interface ServiceError {
    type: ErrorType;
    message:string;
}

function mkError(error:string, type: ErrorType = ErrorType.GENERIC) : ServiceError {
    return {
        message:error,
        type: type,
    }
}
export function handleResponse(response:Response) {
    return response.text().then(text => {
        if (response.status === 403) {
            return Promise.reject(mkError(text, ErrorType.AUTH));
        }
        try {
            const data = text && JSON.parse(text);
            if (!response.ok) {
                if (response.status === 401) {
                    UserService.logout();
                    //location.reload(true);
                }
    
                const error = (data && data.message) || response.statusText;
                return Promise.reject(mkError(error));
            }
    
            return data;
        } catch (error) {
            return Promise.reject(mkError("Failed to parse response from server"));
        }
    });
}

export function getHeader() {
    const user: any = JSON.parse( localStorage.getItem('user') || '{}');
    const token:String = user.key;

    return { 
        'Content-Type': 'application/json' ,
        'Authorization': `Bearer ${token}`
    };
}

export function getGetHeader() {
    return {
        method: 'GET',
        headers: getHeader(),
    };    
}

export function getUrl(url:String) {
    return `/api/v2${url}`;
}

export class CachedValue<T> {
    cache? : T;

    constructor() {
        this.cache = undefined;
    }
    isCached() {
        return this.cache != undefined; 
    }
    set(value: T) : T {
        this.cache = value;
        return value;

    }
    get() : T {
        return this.cache!;
    }


}