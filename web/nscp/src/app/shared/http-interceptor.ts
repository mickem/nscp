import { HTTP_INTERCEPTORS } from '@angular/common/http';
import { LoginInterceptor } from './login-interceptor';

export const httpInterceptorProviders = [
  { provide: HTTP_INTERCEPTORS, useClass: LoginInterceptor, multi: true },
];