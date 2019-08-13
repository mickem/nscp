import React from 'react';
import NavBar from './navbar';
import ClippedDrawer from './drawer';
import clsx from 'clsx';
import { createStyles, makeStyles, useTheme, Theme } from '@material-ui/core/styles';
import { CssBaseline } from '@material-ui/core';
import { Router } from "react-router-dom";
import Messages from '../messages';
import { history } from '../../helpers';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      display: 'flex',
    },
    content: {
      flexGrow: 1,
      padding: theme.spacing(3),
      transition: theme.transitions.create('margin', {
        easing: theme.transitions.easing.sharp,
        duration: theme.transitions.duration.leavingScreen,
      }),
    },
    contentShift: {
      transition: theme.transitions.create('margin', {
        easing: theme.transitions.easing.easeOut,
        duration: theme.transitions.duration.enteringScreen,
      }),
    },
    toolbar: {
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'flex-end',
      padding: '0 8px',
      ...theme.mixins.toolbar,
    },
  }),
);

let Navigation: any = function (props: any) {
  const classes = useStyles();
  const [open, setOpen] = React.useState(false);
  return (
    <div className={classes.root}>
      <CssBaseline />
      <Router history={history}>
        <NavBar drawerOpen={open} setDrawerOpen={() => setOpen(true)}></NavBar>
        <ClippedDrawer drawerOpen={open} setDrawerClosed={() => setOpen(false)}></ClippedDrawer>
        <main
          className={clsx(classes.content, {
            [classes.contentShift]: open,
          })}
        >
          <div className={classes.toolbar} />
          <Messages />
          {props.children}
        </main>
      </Router>
    </div>
  );
}

export default Navigation;
