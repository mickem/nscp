import React from 'react';
import { createStyles, makeStyles, useTheme, Theme } from '@material-ui/core/styles';
import IconButton from '@material-ui/core/IconButton';
import AccountCircle from '@material-ui/icons/AccountCircle';
import MenuItem from '@material-ui/core/MenuItem';
import Menu from '@material-ui/core/Menu';
import { Link } from "react-router-dom";
import { AppState } from '../../reducers';
import { connect } from 'react-redux';
import { UserActions } from '../../actions';
const useStyles = makeStyles((theme: Theme) =>
createStyles({
  root: {
    flexGrow: 1,
  },
  menuButton: {
    marginRight: theme.spacing(2),
  },
  title: {
    flexGrow: 1,
  },
}),
);

const Component = function(props:any) {
  const { dispatch }: any = props;
  const classes = useStyles();
  const theme = useTheme();
  const [auth, setAuth] = React.useState(true);
  const [anchorEl, setAnchorEl] = React.useState<null | HTMLElement>(null);
  const open = Boolean(anchorEl);

  function handleMenu(event: React.MouseEvent<HTMLElement>) {
    setAnchorEl(event.currentTarget);
  }
  function handleClose() {
    setAnchorEl(null);
  }
  function handleLogout() {
    dispatch(UserActions.logout());
    handleClose();
  }

  return (
      <div>
        <IconButton
        aria-label="Account of current user"
        aria-controls="menu-appbar"
        aria-haspopup="true"
        onClick={handleMenu}
        color="inherit"
    >
        <AccountCircle />
    </IconButton>
    <Menu
        id="menu-appbar"
        anchorEl={anchorEl}
        anchorOrigin={{
        vertical: 'top',
        horizontal: 'right',
        }}
        keepMounted
        transformOrigin={{
        vertical: 'top',
        horizontal: 'right',
        }}
        open={open}
        onClose={handleClose}
    >
        <Link to="/account" style={{ textDecoration: 'none' }}><MenuItem onClick={handleClose}>Profile</MenuItem></Link>
        <Link to="/" style={{ textDecoration: 'none' }}><MenuItem onClick={handleLogout}>Logout</MenuItem></Link>
    </Menu>
  </div>
  );
}

const mapStateToProps = (state: AppState) => {
  return {
    authentication: state.authentication,
  }
}

export const AccountButton = connect(mapStateToProps)(Component);
