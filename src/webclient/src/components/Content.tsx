// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import * as React from 'react'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'

class Content extends React.Component<InjectedProps> {
  public static styles: InputSheet<{}> = {
    content: {
      $debugName: 'container',
      display: 'flex',
      flex: 1,
      flexDirection: 'column',
      overflowY: 'scroll',
      padding: 20,
    },
  }

  render() {
    const { classNames } = this.props

    return <div className={classNames.content}>{this.props.children}</div>
  }
}

export default withStyles()<any>(Content)
