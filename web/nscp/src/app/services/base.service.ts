
import {throwError as observableThrowError,  Observable } from 'rxjs';
import { Http } from '@angular/http';
import { environment } from '../../environments/environment';
import { HttpClient } from '@angular/common/http';
import { catchError } from 'rxjs/operators';

export const API_URL = '/api/v2';

export class BaseService {

    constructor(private module: string,
        private http: HttpClient)
    {}

    public handleError (error: Response | any) {
        console.error(`${this.module}::handleError`, error);
        return observableThrowError(error.error.message || 'Server error');
    }

    public httpGet(url: string, options?) : Observable<any> {
        return this.http
        .get(API_URL + url, options)
        .pipe(
            catchError(err => this.handleError(err))
          );
    }
    
}