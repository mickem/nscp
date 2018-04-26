import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';

import { MatButtonModule, MatCheckboxModule } from '@angular/material';

import { AppComponent } from './app.component';
import { AppRoutingModule } from './app-routing.module';
import { SharedModule } from './shared/shared.module';

import { HTTP_INTERCEPTORS, HttpClient, HttpClientModule } from '@angular/common/http';
import { TranslateLoader, TranslateModule } from '@ngx-translate/core';
import { HttpLoaderFactory } from './app.translate.factory';

import { NavComponent } from './nav/nav.component';
import { FooterComponent } from './footer/footer.component';

import { environment } from '../environments/environment';
import { SearchBarComponent } from './search-bar/search-bar.component';

import { BrowserAnimationsModule } from '@angular/platform-browser/animations';

import 'hammerjs';
import { AuthService } from './services/auth.service';
import { httpInterceptorProviders } from './shared/http-interceptor';

import { QueryComponent } from './query/query.component';
import { ModuleComponent } from './module/module.component';
import { OverviewComponent } from './overview/overview.component';
import { LoginComponent } from './login/login.component';
import { LogComponent } from './log/log.component';
import { MetricsComponent } from './metrics/metrics.component';
import { SettingsComponent } from './settings/settings.component';

import { fas } from '@fortawesome/free-solid-svg-icons';
import { far } from '@fortawesome/free-regular-svg-icons';
import { FontAwesomeModule } from '@fortawesome/angular-fontawesome';
import { library } from '@fortawesome/fontawesome-svg-core';
library.add(fas, far);

@NgModule({
  declarations: [
    AppComponent,
    NavComponent,
    FooterComponent,
    SearchBarComponent,
    QueryComponent,
    ModuleComponent,
    OverviewComponent,
    LoginComponent,
    LogComponent,
    MetricsComponent,
    SettingsComponent
  ],
  imports: [
    BrowserModule,
    MatButtonModule,
    MatCheckboxModule,
    HttpClientModule,
    TranslateModule.forRoot({
      loader: {
        provide: TranslateLoader,
        useFactory: HttpLoaderFactory,
        deps: [HttpClient]
      }
    }),
    SharedModule.forRoot(),
    AppRoutingModule,
    BrowserAnimationsModule,
    FontAwesomeModule
  ],
  providers: [AuthService, httpInterceptorProviders],
  bootstrap: [AppComponent]
})
export class AppModule { }
