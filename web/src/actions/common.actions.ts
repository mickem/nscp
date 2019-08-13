import { AlertAction, AlertActions } from "./alert.actions";
import { ServiceError, ErrorType } from "../services/common.service";
import { UserActions, UserAction } from "./user.actions";

// { (arg0: (dispatch: (action: UserAction) => void) => void): void; (action: ActionType): void; (action: AlertAction): void; (action: UserAction): void; }
export function defaultNSCPHandler<ActionType>(exec: any, request: any, success: any, failure: any) {
    return (dispatch: { (arg0: (dispatch: (action: UserAction) => void) => void): void; (action: ActionType): void; (action: AlertAction): void; (action: UserAction): void; } ) => {
        dispatch(request());

        exec()
            .then(
                (result: any) => {
                    console.log(result);
                    dispatch(success(result));
                },
                (error: ServiceError) => {
                    if (error.type == ErrorType.AUTH) {
                        dispatch(UserActions.validateCredentials(error.message));
                    } else {
                        dispatch(failure(error.message));
                        dispatch(AlertActions.error(error.message));
                    }
                }
            );
    };
}
