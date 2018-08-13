// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import * as log from 'loglevel'
import * as React from 'react'
import * as ReactDOM from 'react-dom'
import { Provider } from 'react-redux'

import App from 'components/App'
import { configureStore } from 'store'

import { IS_PRODUCTION } from './utils/env'

// Third-party CSS imports
import 'semantic-ui-forest-themes/semantic.darkly.css'

// App CSS
import './index.css'

if (!IS_PRODUCTION) {
  log.enableAll()
}

const store = configureStore()

const rootElement = document.getElementById('root')
ReactDOM.render(
  <Provider store={store}>
    <App />
  </Provider>,
  rootElement,
)
