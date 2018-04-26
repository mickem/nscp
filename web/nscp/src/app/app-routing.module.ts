import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';

import { OverviewComponent } from './overview/overview.component';
import { LoginComponent } from './login/login.component';
import { QueryComponent } from './query/query.component';
import { ModuleComponent } from './module/module.component';
import { LogComponent } from './log/log.component';
import { MetricsComponent } from './metrics/metrics.component';
import { SettingsComponent } from './settings/settings.component';

const routes: Routes = [
  {path: '', redirectTo: '/overview', pathMatch: 'full'},
  {path: 'login', component: LoginComponent},
  {path: 'queries', component: QueryComponent},
  {path: 'queries/:id', component: QueryComponent},
  {path: 'modules', component: ModuleComponent},
  {path: 'modules/:id', component: ModuleComponent},
  {path: 'overview', component: OverviewComponent},
  {path: 'log', component: LogComponent},
  {path: 'metrics', component: MetricsComponent},
  {path: 'settings', component: SettingsComponent},
  {path: 'settings/:path', component: SettingsComponent},
  {path: 'settings/:path/:key', component: SettingsComponent},

  {path: '**', redirectTo: '/overview'}
];

@NgModule({
  imports: [
    RouterModule.forRoot(routes)
  ],
  exports: [
    RouterModule
  ]
})

export class AppRoutingModule {
}
