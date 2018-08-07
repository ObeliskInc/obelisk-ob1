// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import axios from 'axios'
import * as _ from 'lodash'
import { getHistory } from 'utils'

// Dependencies injected into redux-logic process() functions
const deps = {
  axios,
  history: getHistory(),
}

import { applyMiddleware, compose, createStore } from 'redux'
const { batchStoreEnhancer, batchMiddleware } = require('redux-batch-enhancer')
import { createLogger } from 'redux-logger'

// Import all logic and combine together
import mainLogic from 'modules/Main/logic'
const logic = [...mainLogic]

const ReduxLogic = require('redux-logic')
const createLogicMiddleware = ReduxLogic.createLogicMiddleware

const logicMiddleware = createLogicMiddleware(logic, deps)

import { reducer as rootReducer } from 'reducer'

const loggerMiddleware = createLogger({
  collapsed: true,
  // Only log actions that have their meta.log set to true.
  // Set this to false to suppress noisy actions.
  predicate: (getState, action) => _.get(action, ['meta', 'log'], true),
})

export const configureStore = () => {
  const middleware = applyMiddleware(logicMiddleware, batchMiddleware, loggerMiddleware)

  // Build the initialState by having each reducer initialize itself and combine the results
  const initialState = rootReducer({}, { type: '@@redux/INIT' })

  const store = createStore(
    rootReducer,
    initialState,
    compose(
      middleware,
      batchStoreEnhancer,
    ),
  )

  return store
}
