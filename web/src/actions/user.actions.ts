import { UserConstants } from '../constants';
import { UserService, User } from '../services';
import { history } from '../helpers';
import { ServiceError } from '../services/common.service';
import { defaultNSCPHandler } from './common.actions';

export interface UserAction {
    type: UserConstants;
    user?: User;
    error?: String;
}
export class UserActions {

    static login(username: String, password: String) {
        return defaultNSCPHandler<UserAction>(() => UserService.login(username, password),
            () : UserAction => { return { type: UserConstants.LOGIN_REQUEST, user: { user: username } } },
            (user: User) : UserAction => { 
                history.push('/');
                return { type: UserConstants.LOGIN_SUCCESS, user }
             },
            (error: string) : UserAction => { return { type: UserConstants.LOGIN_FAILURE, error } }
        );
    }
    
    
    static validateCredentials(originalError:string) {
        return (dispatch: { (action: UserAction): void }) => {
            dispatch(request());
    
            UserService.ping()
                .then(
                    () => { 
                        dispatch(success());
                    },
                    (error :ServiceError) => {
                        UserService.logout();
                        history.push('/');
                        dispatch(failure(originalError));
                    }
                );
        };
    
        function request() : UserAction { return { type: UserConstants.LOGIN_REQUEST } }
        function success() : UserAction  { return { type: UserConstants.LOGIN_SUCCESS } }
        function failure(error: String) : UserAction  { return { type: UserConstants.LOGIN_FAILURE, error } }
    }
    
    static logout() {
        UserService.logout();
        return { type: UserConstants.LOGOUT };
    }
}
