
import {throwError as observableThrowError,  Observable } from 'rxjs';
import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders, HttpErrorResponse } from '@angular/common/http';
import { User } from './user';
import { catchError } from 'rxjs/operators';

const API_URL = '/api/v1'

@Injectable()
export class LoginService  {

  constructor(private http: HttpClient) { }

  public login(username: string, password: string): Observable<User> {
    const headers = new HttpHeaders().set('Authorization', "Basic " + btoa(username + ":" + password));
    return this.http
      .get<User>(API_URL + '/login/', { headers })
      .pipe(
        catchError(err => this.handleError(err))
      );
  }

  private handleError (error: HttpErrorResponse) {
    console.error('ApiService::handleError', error);
    return observableThrowError(error);
  }
}
