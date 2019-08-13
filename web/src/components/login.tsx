import React from "react";
import { Button, Card, CardContent, InputAdornment, IconButton, FormControl, InputLabel, Input, CardHeader, CardActions, Grid } from "@material-ui/core";
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import { connect } from 'react-redux'
import { UserActions } from "../actions";
import AccountCircle from '@material-ui/icons/AccountCircle';
import Visibility from '@material-ui/icons/Visibility';
import VisibilityOff from '@material-ui/icons/VisibilityOff';

interface State {
  username: String,
  password: String,
  showPassword: boolean;
}
const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    card: {
      display: 'flex',
      flexWrap: 'wrap',
      justifyContent: "center",
      margin: 20,
      padding: 20,
      maxWidth: 375,
    },
    margin: {
      margin: theme.spacing(1),
    },
  }),
);
const Component = function (props: any) {
  const classes = useStyles();
  const [values, setValues] = React.useState<State>({
    username: '',
    password: '',
    showPassword: false,
  });
  const { dispatch }: any = props;

  const handleChange = (name: any) => (event: React.ChangeEvent<HTMLInputElement>) => {
    setValues({ ...values, [name]: event.target.value });
  };
  const handleClickShowPassword = () => {
    setValues({ ...values, showPassword: !values.showPassword });
  };
  const login = () => {
    dispatch(UserActions.login(values.username, values.password));
  }

  return (
    <Card className={classes.card}>
      <form noValidate autoComplete="off">
        <CardHeader
          title="Login"
          subheader="Please login"
        />
        <CardContent>
          <FormControl>
            <InputLabel htmlFor="username">Username</InputLabel>
            <Input
              id="username"
              value={values.username}
              required
              onChange={handleChange('username')}
              startAdornment={(
                <InputAdornment position="start">
                  <AccountCircle />
                </InputAdornment>
              )}
            />
          </FormControl>
          <br />
          <FormControl>
            <InputLabel htmlFor="password">Password</InputLabel>
            <Input
              id="password"
              type={values.showPassword ? 'text' : 'password'}
              value={values.password}
              required
              onChange={handleChange('password')}
              endAdornment={
                <InputAdornment position="end">
                  <IconButton aria-label="Toggle password visibility" onClick={handleClickShowPassword}>
                    {values.showPassword ? <Visibility /> : <VisibilityOff />}
                  </IconButton>
                </InputAdornment>
              }
            />
          </FormControl>
        </CardContent>
        <CardActions>
          <Grid container alignItems="flex-start" justify="flex-end" direction="row">
            <Button onClick={login} size="small" color="primary">
              Login
        </Button>
          </Grid>
        </CardActions>
      </form>
    </Card>
  );
}

export const Login = connect()(Component);