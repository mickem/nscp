import { User } from "../services";

export interface AuthenticationState {
  loggingIn?: boolean;
  loggedIn?: boolean;
//  registering?:boolean;
//  items? : User[];
  user?: User;
//  loading?:boolean;
//  error?:String;
//  type?:String;
//  message?:String;
}
  