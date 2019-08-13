import React from 'react';
import { Switch, FormControlLabel } from '@material-ui/core';
import { ModuleActions } from '../../actions';
import { ModuleProps } from './show.module';
import { connect } from 'react-redux';

export const Component = function (props: ModuleProps) {
  const { dispatch }: any = props;
  function handleToggle() {
    if (props.module.enabled) {
      dispatch(ModuleActions.disable(props.module));
    } else {
      dispatch(ModuleActions.enable(props.module));
    }
  }

  return (
    <FormControlLabel
      control={
        <Switch
          checked={props.module.enabled}
          onChange={handleToggle}
          inputProps={{ 'aria-label': 'enabled' }}
        />
      }
      label="Enabled"
    />
  );
}

export const EnableModuleSwitch = connect()(Component);

