import { createStore, applyMiddleware } from 'redux'
import { createBrowserHistory } from 'history'
import { routerMiddleware } from 'react-router-redux'
import rootReducer from '../reducers'
import arrLogic from '../logic'
import { createLogicMiddleware } from 'redux-logic'

const history = createBrowserHistory()
const router = routerMiddleware(history)
const logic = createLogicMiddleware(arrLogic)
const enhancer = applyMiddleware(logic, router)

export = {
  history,
  configureStore(initialState: Object | void) {
    return createStore(rootReducer, initialState, enhancer)
  },
}
