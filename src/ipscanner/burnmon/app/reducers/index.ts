import { combineReducers, Reducer } from 'redux'
import { routerReducer as routing } from 'react-router-redux'
import counter, { TState as TCounterState } from './counter'
import bridge, { IState as IBridgeState } from './bridge'

const rootReducer = combineReducers({
  counter,
  bridge,
  routing: routing as Reducer<any>,
})

export interface IState {
  counter: TCounterState
  bridge: IBridgeState
}

export default rootReducer
