import React from 'react';
import ListItem from '@material-ui/core/ListItem';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import { BrowserRouter as Router, Route, Link } from "react-router-dom";
import { LocationDescriptor } from 'history';

export interface DrawerLinkProps {
  key: string;
  icon: any;
  title: string;
  to: LocationDescriptor;

}
export const DrawerLink = function (props: DrawerLinkProps) {
  return (
    <Link to={props.to} style={{ textDecoration: 'none' }}>
      <ListItem button key={props.key}>
        <ListItemIcon>{props.icon}</ListItemIcon>
        <ListItemText primary={props.title} />
      </ListItem>
    </Link>
  );
}
