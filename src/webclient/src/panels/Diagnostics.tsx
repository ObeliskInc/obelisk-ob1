// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import _ = require('lodash')
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Header, Message, TextArea } from 'semantic-ui-react'

import Content from 'components/Content'
import { fetchDiagnostics } from 'modules/Main/actions'
import { getDiagnostics } from 'modules/Main/selectors'

const {CopyToClipboard } = require('react-copy-to-clipboard');

interface ConnectProps {
  diagnostics?: string
}

type CombinedProps = ConnectProps & InjectedProps & DispatchProp<any>

class Diagnostics extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    diagnostics: {
      background: 'black',
      color: 'white',
      fontFamily: 'monospace',
      minHeight: 200,
      padding: 12,
    }
  }

  constructor(props: CombinedProps) {
    super(props);
  }

  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchDiagnostics.started({}))
    }
  }

  render() {
    const { classNames, diagnostics } = this.props

    return (
      <Content>
        <Header as="h1">Diagnostics</Header>
        <Message
          icon="warning sign"
          header="Privacy Warning"
          content={
            'The information below has been collected directly from your miner.  It ' +
            'may contain private information such as wallet addresses and pool passwords. ' +
            'Please edit the information to remove anything you don\'t want to share.'
          }
        />

        <TextArea className={classNames.diagnostics} rows={40}  value={diagnostics} disabled={true}/>
        <CopyToClipboard text={diagnostics}>
        <CopyToClipboard text={'[CopyToClipboard]\n' + info}>
          <Button>COPY TO CLIPBOARD</Button>
        </CopyToClipboard>
      </Content>
    )
  }
}

const mapStateToProps = (state: any, props: any): ConnectProps => ({
  diagnostics: getDiagnostics(state.Main),
})

const diagnostics = withStyles()<any>(Diagnostics)

export default connect<ConnectProps, any, any>(mapStateToProps)(diagnostics)
