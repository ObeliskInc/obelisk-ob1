import * as React from 'react'
import { render } from 'react-dom'
import { AppContainer } from 'react-hot-loader'
import Root from './containers/Root'
import './app.global.scss'

const { configureStore, history } = require('./store/configureStore')
export const store = configureStore()

// Open all links in external browser
let shell = require('electron').shell
document.addEventListener('click', function (event: any) {
  if (event && event.target.tagName === 'A' && event.target.href.startsWith('http')) {
    event.preventDefault()
    shell.openExternal(event.target.href)
  }
})

render(
  <AppContainer>
    <Root store={store} history={history} />
  </AppContainer>,
  document.getElementById('root')
)

if ((module as any).hot) {
  ;(module as any).hot.accept('./containers/Root', () => {
    const NextRoot = require('./containers/Root').default
    render(
      <AppContainer>
        <NextRoot store={store} history={history} />
      </AppContainer>,
      document.getElementById('root')
    )
  })
}
