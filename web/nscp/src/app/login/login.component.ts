import { Component, OnInit } from '@angular/core';
import { LoginService } from '../services/login.service';
import { AuthService } from '../services/auth.service';
import { ActivatedRoute, Router } from '@angular/router';
import { User } from '../services/user';

@Component({
  selector: 'app-login',
  templateUrl: './login.component.html',
  styleUrls: ['./login.component.css'],
  providers: [LoginService, AuthService]
})
export class LoginComponent implements OnInit {

  username: string;
  password: string;
  old: string;
  
  constructor(private loginService: LoginService,
    private auth: AuthService,
    private route: ActivatedRoute,
    private router: Router) { }

  ngOnInit() {
    this.route.paramMap.subscribe(params => {
      this.old = params.get("old");
    });
  }

  login() : void {
    this.loginService.login(this.username, this.password)
      .subscribe(user => this.loginSuccess(user));
  }

  loginSuccess(user: User) : void {
    this.auth.setUser(user);
    if (this.old) {
      this.router.navigate([this.old]);
    } else {
      this.router.navigate(["/"]);
    }

  }

}
