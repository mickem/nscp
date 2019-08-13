import React from "react";
import { Toolbar, TextField, IconButton } from "@material-ui/core";
import { makeStyles, createStyles, Theme } from '@material-ui/core/styles';
import { LogActions } from "../../actions";
import RefreshIcon from '@material-ui/icons/Refresh';
import { connect } from "react-redux";

const useStyles = makeStyles((theme: Theme) =>
    createStyles({
        spacer: {
            flexGrow: 1,
        },
    }),
);

const Component = function (props: any) {
    const { dispatch }: any = props;
    const handleChange = (event: React.ChangeEvent<HTMLInputElement>) => {
        //dispatch(LogActions.setFilter(event.target.value));
    };
    const handleRefresh = () => {
        dispatch(LogActions.list(true));
    };
    const classes = useStyles();
    return (
        <Toolbar>
            <span className={classes.spacer} />
            <IconButton
                color="inherit"
                aria-label="refresh"
                onClick={handleRefresh}
            >
                <RefreshIcon />
            </IconButton>
        </Toolbar>
    );
}
export const LogToolbar = connect()(Component);