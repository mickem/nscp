import React from 'react';
import { makeStyles, Theme, createStyles } from '@material-ui/core/styles';
import clsx from 'clsx';
import Card from '@material-ui/core/Card';
import CardHeader from '@material-ui/core/CardHeader';
import CardContent from '@material-ui/core/CardContent';
import CardActions from '@material-ui/core/CardActions';
import Collapse from '@material-ui/core/Collapse';
import Avatar from '@material-ui/core/Avatar';
import IconButton from '@material-ui/core/IconButton';
import Typography from '@material-ui/core/Typography';
import { red } from '@material-ui/core/colors';
import ExpandMoreIcon from '@material-ui/icons/ExpandMore';
import { Query as QueryType } from '../../services';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    card: {
      maxWidth: 345,
      width: 345,
    },
    enabled: {
      backgroundColor: "none",
    },
    disabled: {
      backgroundColor: "#90A4AE",
    },
    expand: {
      transform: 'rotate(0deg)',
      marginLeft: 'auto',
      transition: theme.transitions.create('transform', {
        duration: theme.transitions.duration.shortest,
      }),
    },
    expandOpen: {
      transform: 'rotate(180deg)',
    },
    avatar: {
      backgroundColor: red[500],
    },
    description: {
      maxHeight: 50,
      height: 50,
    }
  }),
);

export interface QueryProps {
  query: QueryType;
}
export const Query = function (props: QueryProps) {
  const classes = useStyles();
  const [expanded, setExpanded] = React.useState(false);
  const query : QueryType = props.query;

  function handleExpandClick() {
    setExpanded(!expanded);
  }
  const avatar = query.name.startsWith('check_') ? query.name.substr(6, 1) : query.name.startsWith('check') ? query.name.substr(5, 1) :query.name.substr(0,1);

  return (
    <Card className={classes.card}>
      <CardHeader
        avatar={
          <Avatar aria-label="Query" className={classes.avatar}>
            {avatar}
          </Avatar>
        }
        action={
          <IconButton
          className={clsx(classes.expand, {
            [classes.expandOpen]: expanded,
          })}
          onClick={handleExpandClick}
          aria-expanded={expanded}
          aria-label="Show more"
        >
          <ExpandMoreIcon />
        </IconButton>
        }
        title={query.name}
        subheader={query.title}
      />
      <CardContent>
        <Typography variant="body2" color="textSecondary" component="p" noWrap={!expanded}>
          {query.description}
        </Typography>

      </CardContent>
      <CardActions disableSpacing>
        TODO: Execute here
      </CardActions>
      <Collapse in={expanded} timeout="auto" unmountOnExit>
        <CardContent>
          <Typography paragraph>TODO: Show help here</Typography>
        </CardContent>
      </Collapse>
    </Card>
  );
}


