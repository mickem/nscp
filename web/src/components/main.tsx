import React from 'react';
import { Route } from "react-router-dom";
import { PrivateRoute } from './private-router';
import { Login } from './login';
import Navigation from './navigation';
import { Account } from './account';
import { ModuleList } from './module';
import { QueryList } from './query';
import { LogList } from './log';
import { MetricList } from './metric';
import Welcome from './welcome';

let Main: any = function (props: any) {
  return (
    <Navigation>
      <Route exact path="/login" component={Login} />
      <PrivateRoute exact path="/" component={Welcome} />
      <PrivateRoute exact path="/account" component={Account} />
      <PrivateRoute exact path="/modules" component={ModuleList} />
      <PrivateRoute exact path="/queries" component={QueryList} />
      <PrivateRoute exact path="/metrics" component={MetricList} />
      <PrivateRoute exact path="/log" component={LogList} />
    </Navigation>
  );
}
export default Main;
