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
import { Module as ModuleType } from '../../services';
import { EnableModuleSwitch } from './enable.module.switch';
import { LoadModuleSwitch } from './load.module.switch';

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

export interface ModuleProps {
  module: ModuleType;
}
export const Module = function (props: ModuleProps) {
  const classes = useStyles();
  const [expanded, setExpanded] = React.useState(false);
  const module = props.module;

  function handleExpandClick() {
    setExpanded(!expanded);
  }
  const avatar = module.id.startsWith('Check') ? module.id.substr(5, 1) : module.id.substr(0, 1);

  return (
    <Card className={clsx(classes.card, module.enabled ? classes.enabled : classes.disabled)} elevation={module.enabled ? 3 : 1}>
      <CardHeader
        avatar={
          <Avatar aria-label="Module" className={classes.avatar}>
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
        title={module.id}
        subheader={module.title}
      />
      <CardContent>
        <Typography variant="body2" color="textSecondary" component="p" noWrap={!expanded}>
          {module.description}
        </Typography>

      </CardContent>
      <CardActions disableSpacing>
        <EnableModuleSwitch module={module} />
        <LoadModuleSwitch module={module} />
      </CardActions>
      <Collapse in={expanded} timeout="auto" unmountOnExit>
        <CardContent>
          {module.queries ?
            (
              module.queries.map((query) => (
                <Typography>{query.name}</Typography>
              ))
            ): (
              <Typography>no queries</Typography>
            )
          }
          <Typography paragraph>TODO: Add queries here</Typography>
        </CardContent>
      </Collapse>
    </Card>
  );
}


