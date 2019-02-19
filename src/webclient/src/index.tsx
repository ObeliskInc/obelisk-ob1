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

// The Diagnostics tab copies the text with HTML styling, which we do not want.
// This bit of code converts it to plain text.
document.addEventListener('copy', (e: any) => {
  const textContent = e.target.textContent
  const filterText = '[CopyToClipboard]\n'
  if (textContent.startsWith(filterText)) {
    e.clipboardData.setData('text/plain', textContent.replace(filterText, ''))
    e.preventDefault()
  }
})

const store = configureStore()

const rootElement = document.getElementById('root')
ReactDOM.render(
  <Provider store={store}>
    <App />
  </Provider>,
  rootElement,
)
