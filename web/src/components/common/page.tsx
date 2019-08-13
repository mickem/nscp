import React from "react";
import { Container, Typography, Paper, makeStyles, createStyles, Theme } from "@material-ui/core";


const useStyles = makeStyles((theme: Theme) =>
    createStyles({
        container: {
            paddingLeft: theme.spacing(3),
            paddingRight: theme.spacing(3)
        },
    }),
);
const Page = function (props: any) {
    const classes = useStyles();
    return (
        <div>
            <Container maxWidth="lg">
                <Paper>
                    {props.menu}
                    <div className={classes.container}>
                        {props.loading ?
                            (<Typography>Loading...</Typography>) :
                            (<div>{props.content}</div>)
                        }
                    </div>
                </Paper>
            </Container>
        </div>
    );
}
export default Page;