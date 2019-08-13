import React, { useEffect } from "react";
import { Grid } from "@material-ui/core";
import { makeStyles, createStyles, Theme } from '@material-ui/core/styles';
import { connect } from 'react-redux'
import { AppState } from "../../reducers";
import { QueryActions } from "../../actions";
import { getVisibleQueries } from '../../selectors'
import Page from "../common/page";
import { QueryToolbar } from "./toolbar.query";
import { Query } from "./show.query";

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
        dispatch(QueryActions.list());
    }, []);

    const classes = useStyles();
    return (
        <Page
            loading={props.loading}
            menu={<QueryToolbar />}
            content={
                <Grid
                    container
                    spacing={2}
                    className={classes.root}
                >
                    {props.queries.map((query: any) => (
                        <Grid key={query.name} item>
                            <Query query={query} />
                        </Grid>
                    ))}
                </Grid>
            } />
    );
}

const mapStateToProps = (state: AppState) => {
    return {
        queries: getVisibleQueries(state.query),
        loading: state.query.loading || false
    }
}

export const QueryList = connect(mapStateToProps)(List);
