import React, { useEffect } from "react";
import { Table, TableBody, TableRow, TableHead, TableCell, Container, Grid, ListItem, ListItemText, List } from "@material-ui/core";
import { makeStyles, createStyles, Theme } from '@material-ui/core/styles';
import { connect } from 'react-redux'
import { AppState } from "../../reducers";
import { LogActions } from "../../actions";
import { getLog } from '../../selectors'
import Page from "../common/page";
import { LogToolbar } from "./toolbar.log";
import { LogEntry } from "../../services";

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

const Component = function (props: any) {
    const { dispatch }: any = props;
    const [selected, setSelected] = React.useState<LogEntry | undefined>(undefined);

    useEffect(() => {
        dispatch(LogActions.list());
    }, []);


    function handleClick(event: React.MouseEvent<unknown>, log: LogEntry) {
        if (selected === log) {
            setSelected(undefined);
        } else {
            setSelected(log);
        }
    }

    const isSelected = (log: LogEntry) => selected === log;

    const classes = useStyles();
    return (
        <Page
            loading={props.loading}
            menu={<LogToolbar />}
            content={
                <Grid container direction="row">
                    <Grid item xs={selected === undefined ? 12 : 10}>
                        <Table className={classes.table} padding="none">
                            <TableHead>
                                <TableRow>
                                    <TableCell>Date</TableCell>
                                    <TableCell>Level</TableCell>
                                    <TableCell>Message</TableCell>
                                </TableRow>
                            </TableHead>
                            <TableBody>
                                {props.log.map((log: LogEntry) => {
                                    const isItemSelected = isSelected(log);
                                    return (
                                        <TableRow
                                            onClick={event => handleClick(event, log)}
                                            selected={isItemSelected}
                                        >
                                            <TableCell>{log.date}</TableCell>
                                            <TableCell>{log.level}</TableCell>
                                            <TableCell>{log.message}</TableCell>
                                        </TableRow>
                                    )
                                })}
                            </TableBody>
                        </Table>
                    </Grid>
                    {selected !== undefined ? (
                        <Grid item xs={2}>
                            <List>
                                <ListItem>
                                    <ListItemText primary={selected.level} secondary="Level" />
                                </ListItem>
                                <ListItem>
                                    <ListItemText primary={selected.date} secondary="Date" />
                                </ListItem>
                                <ListItem>
                                    <ListItemText primary={selected.file + ":" + selected.line} secondary="Reference" />
                                </ListItem>
                                <ListItem>
                                    <ListItemText primary={selected.message} />
                                </ListItem>
                            </List>
                        </Grid>) : null}
                </Grid>

            }
        />
    );
}

const mapStateToProps = (state: AppState) => {
    return {
        log: getLog(state.log),
        loading: state.log.loading || false
    }
}

export const LogList = connect(mapStateToProps)(Component);
