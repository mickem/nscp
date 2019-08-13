import React from "react";
import { Toolbar, Typography } from "@material-ui/core";
import { makeStyles, createStyles, Theme } from '@material-ui/core/styles';
import { connect } from 'react-redux'
import { AppState } from "../../reducers";
import Page from "../common/page";
import { User } from "../../services";

const useStyles = makeStyles((theme: Theme) =>
    createStyles({
        root: {
            flexGrow: 1,
        },
        paper: {
            height: 140,
            width: 100,
        },
        margin: {
            margin: theme.spacing(1),
        },
    }),
);
interface PropsType {
    user: User;
}
const Component : any = function (props: PropsType) {
    const { dispatch }: any = props;
    const classes = useStyles();
    return (
        <Page menu={
            <Toolbar>
            </Toolbar>
        }
            content={
                <Typography>Your logged in as: {props.user.user}</Typography>
            }
        />

    );
}

const mapStateToProps = (state: AppState) => {
    return {
        user: state.authentication.user
    };
}

export const Account = connect(mapStateToProps)(Component);