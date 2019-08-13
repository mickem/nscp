import React, { SyntheticEvent } from 'react';
import { createStyles, makeStyles, useTheme, Theme } from '@material-ui/core/styles';
import { Snackbar, IconButton } from '@material-ui/core';
import { AlertActions } from '../actions';
import { history } from '../helpers';
import { connect } from 'react-redux'
import { AppState } from '../reducers';
import CloseIcon from '@material-ui/icons/Close';
import { green, amber } from '@material-ui/core/colors';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    snackIcon: {
      fontSize: 20,
    },
    success: {
      backgroundColor: green[600],
    },
    error: {
      backgroundColor: theme.palette.error.dark,
    },
    info: {
      backgroundColor: theme.palette.primary.main,
    },
    warning: {
      backgroundColor: amber[700],
    },
  }),
);

let Messages: any = function (props: any) {
  const { dispatch }: any = props;
  history.listen((location, action) => {
    dispatch(AlertActions.clear());
  });
  function onSnackClose(event?: SyntheticEvent, reason?: string) {
    if (reason === 'clickaway') {
      return;
    }
    dispatch(AlertActions.clear());
  }

  const classes = useStyles();
  const { alert }: AppState = props;
  return (
    <Snackbar
      anchorOrigin={{
        vertical: 'bottom',
        horizontal: 'left',
      }}
      ContentProps={{
        classes: {
          root: classes[alert.type || "error"]
        }
      }}
      open={alert.message != undefined}
      message={<span>{alert.message}</span>}
      action={[
        <IconButton key="close" aria-label="Close" color="inherit" onClick={onSnackClose}>
          <CloseIcon className={classes.snackIcon} />
        </IconButton>,
      ]}
    />
  );
}

const mapStateToProps = (state: AppState) => {
  return {
    alert: state.alert
  }
}

Messages = connect(mapStateToProps)(Messages)

export default Messages;
