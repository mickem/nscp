import React, { useEffect } from "react";
import { Grid } from "@material-ui/core";
import { makeStyles, createStyles, Theme } from '@material-ui/core/styles';
import { connect } from 'react-redux'
import { AppState } from "../../reducers";
import { ModuleActions, QueryActions } from "../../actions";
import { getVisibleModules } from '../../selectors'
import Page from "../common/page";
import { ModuleToolbar } from "./toolbar.module";
import { Module } from "./show.module";

const useStyles = makeStyles((theme: Theme) =>
    createStyles({
        root: {
            flexGrow: 1,
        },
    }),
);

const List = function (props: any) {
    const { dispatch }: any = props;

    useEffect(() => {
        dispatch(ModuleActions.list());
        dispatch(QueryActions.list());
    }, []);

    const classes = useStyles();
    return (
        <Page
            loading={props.loading}
            menu={<ModuleToolbar />}
            content={
                <Grid
                    container
                    spacing={2}
                    className={classes.root}
                >
                    {props.modules.map((module: any) => (
                        <Grid key={module.id} item>
                            <Module module={module} />
                        </Grid>
                    ))}
                </Grid>
            } />
    );
}

const mapStateToProps = (state: AppState) => {
    return {
        modules: getVisibleModules(state.module),
        loading: state.module.loading || false
    }
}

export const ModuleList = connect(mapStateToProps)(List);
