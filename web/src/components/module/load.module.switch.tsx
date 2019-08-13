import React from 'react';
import { Switch, FormControlLabel } from '@material-ui/core';
import { ModuleActions } from '../../actions';
import { ModuleProps } from './show.module';
import { connect } from 'react-redux';

const Component = function (props: ModuleProps) {
  const { dispatch }: any = props;
  function handleToggle() {
    if (props.module.loaded) {
      dispatch(ModuleActions.unload(props.module));
    } else {
      dispatch(ModuleActions.load(props.module));
    }
  }

  return (
    <FormControlLabel
      control={
        <Switch
          checked={props.module.loaded}
          onChange={handleToggle}
          inputProps={{ 'aria-label': 'loaded' }}
        />
      }
      label="Loaded"
    />
  );
}

export const LoadModuleSwitch = connect()(Component);
