// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import * as React from 'react'
import { Route, Router, Switch } from 'react-router-dom'

import Login from 'components/Login'
import Main from 'components/Main'
import { getHistory } from 'utils'

export default class App extends React.PureComponent<{}> {
  render() {
    return (
      <Router history={getHistory()}>
        <Switch>
          <Route exact={true} path="/login" component={Login} />
          <Route exact={true} path="/" component={Login} />
          <Route component={Main} />
        </Switch>
      </Router>
    )
  }
}
