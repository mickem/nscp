import { Injectable } from '@angular/core';
import { User } from './user';

@Injectable()
export class AuthService {

  constructor() { }

  public getToken(): string {
    return localStorage.getItem('token');
  }

  public setToken(token: string) : void {
    localStorage.setItem('token', token);
  }

  public setUser(user: User) : void {
    this.setToken(user.key);
  }

  public logout() : void {
    localStorage.removeItem('token');
  }

}
