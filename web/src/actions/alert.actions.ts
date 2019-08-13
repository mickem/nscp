import { AlertConstants } from '../constants';

export interface AlertAction {
    type: AlertConstants;
    message?: String;
}

export class AlertActions {

    static success(message:String) : AlertAction {
        return { type: AlertConstants.SUCCESS, message };
    }
    
    static error(message:String) : AlertAction {
        return { type: AlertConstants.ERROR, message };
    }
    
    static clear() : AlertAction {
        return { type: AlertConstants.CLEAR };
    }
}
