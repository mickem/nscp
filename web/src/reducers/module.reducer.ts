import { ModuleConstants } from '../constants';
import { ModuleAction } from '../actions';
import { ModuleState } from '../state';
import { Module } from '../services';
import { filterListById } from './common.reducers';

const initialState: ModuleState = {
  modules: [],
}

export function module(state = initialState, action: ModuleAction): ModuleState {
  switch (action.type) {
    case ModuleConstants.LIST.REQUEST:
      return {
        ...state,
        loading: true,
      };
    case ModuleConstants.LIST.SUCCESS:
      return {
        ...state,
        loading: false,
        modules: action.modules || []
      };
    case ModuleConstants.LIST.FAILURE:
      return {
        ...state,
        loading: false,
        error: action.error,
        modules: []
      };
    case ModuleConstants.SET_FILTER:
      return {
        ...state,
        visibilityFilter: action.filter,
      };
    case ModuleConstants.ENABLE.SUCCESS:
      return {
        ...state,
        modules: filterListById<Module>(state.modules!, action.module!, m => ({
          ...m,
          enabled: true,
        }))
      }
    case ModuleConstants.DISABLE.SUCCESS:
      return {
        ...state,
        modules: filterListById<Module>(state.modules!, action.module!, m => ({
          ...m,
          enabled: false,
        }))
      }
    case ModuleConstants.LOAD.SUCCESS:
      return {
        ...state,
        modules: filterListById<Module>(state.modules!, action.module!, m => ({
          ...m,
          loaded: true,
        }))
      }
    case ModuleConstants.UNLOAD.SUCCESS:
      return {
        ...state,
        modules: filterListById<Module>(state.modules!, action.module!, m => ({
          ...m,
          loaded: false,
        }))
      }
    default:
      return state
  }
}