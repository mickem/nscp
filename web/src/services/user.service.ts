import { handleResponse, CachedValue, getGetHeader, getUrl } from './common.service';

export interface User {
    key?: String;
    user?: String;
}

export class UserService {
    static login(username: String, password: String) {
        const requestOptions = {
            method: 'GET',
            headers: { 
                'Content-Type': 'application/json' ,
                'Authorization': 'Basic ' + btoa(username + ":" + password)
        },
        };
        return fetch(getUrl('/login'), requestOptions)
            .then(handleResponse)
            .then(user => {
                localStorage.setItem('user', JSON.stringify(user));
                return user;
            });
    }
    
    static logout() {
        localStorage.removeItem('user');
    }

    static ping(): Promise<void> {
        return fetch(getUrl('/'), getGetHeader())
            .then(handleResponse)
            .then(() => {
            });
    }
}
