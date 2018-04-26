import { Injectable } from '@angular/core';
import { HttpEvent, HttpInterceptor, HttpHandler, HttpRequest, HttpErrorResponse } from '@angular/common/http';

import { Observable } from 'rxjs';
import { Router, ActivatedRoute } from '@angular/router';
import { AuthService } from '../services/auth.service';
import { tap } from 'rxjs/operators';

@Injectable()
export class LoginInterceptor implements HttpInterceptor {

    constructor(
        private auth: AuthService,
        private router: Router
    ) { }
 
    intercept(req: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {
        if (!req.headers.has('Authorization')) {
            req = req.clone({
                setHeaders: {
                  Authorization: `Bearer ${this.auth.getToken()}`
                }
              });
        }
        return next.handle(req)
            .pipe(
                tap(event => {}, err => this.doTap(err))
            );
    }
    doTap(err) : void {
        if (err instanceof HttpErrorResponse && err.status == 403) {
            this.router.navigate(['/login', {'old': this.router.url}]);
        }
    }
}

