import React, { useEffect } from "react";
import { Grid, Table, TableHead, TableRow, TableCell, TableBody } from "@material-ui/core";
import { makeStyles, createStyles, Theme } from '@material-ui/core/styles';
import { connect } from 'react-redux'
import { AppState } from "../../reducers";
import { MetricActions } from "../../actions";
import { getVisibleMetrics } from '../../selectors'
import Page from "../common/page";
import { MetricToolbar } from "./toolbar.metric";
import { MetricsDictionary } from "../../services";

const useStyles = makeStyles((theme: Theme) =>
    createStyles({
        root: {
            flexGrow: 1,
        },
        table: {
            paddingLeft: theme.spacing(2)
        }
    }),
);

interface Properties {
    metrics: MetricsDictionary;
    loading: boolean;
}


const List = function (props: Properties) {
    const { dispatch }: any = props;
    console.log(props);

    useEffect(() => {
        dispatch(MetricActions.list());
    }, []);

    const classes = useStyles();
    return (
        <Page
            loading={props.loading}
            menu={<MetricToolbar />}
            content={
                <Table className={classes.table} padding="none">
                    <TableHead>
                        <TableRow>
                            <TableCell>Key</TableCell>
                            <TableCell>Value</TableCell>
                        </TableRow>
                    </TableHead>
                    <TableBody>
                        {Object.keys(props.metrics).map(key => (
                            <TableRow>
                                <TableCell>{key}</TableCell>
                                <TableCell>{props.metrics[key]}</TableCell>
                            </TableRow>
                        ))}
                    </TableBody>
                </Table>
            } />
    );
}

const mapStateToProps = (state: AppState) => {
    return {
        metrics: getVisibleMetrics(state.metric),
        loading: state.metric.loading || false
    }
}

export const MetricList = connect(mapStateToProps)(List);
