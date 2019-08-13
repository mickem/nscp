import { User } from "../services";

export interface UserState {
  items? : User[];
  loading?:boolean;
  error?:String;
}
  