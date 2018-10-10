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

interface ConnectProps {
  diagnostics?: string
}

type CombinedProps = ConnectProps & InjectedProps & DispatchProp<any>

class Diagnostics extends React.PureComponent<CombinedProps> {
  private textArea: React.RefObject<HTMLTextAreaElement>;
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
    this.textArea = React.createRef();
}
  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchDiagnostics.started({}))
    }
  }

  setTextArea = (element: any) => {
    this.textArea = element
  }

  handleCopy = () => {
    if (this.textArea) {
      this.textArea.select()
      window.document.execCommand("copy")
    }
  }

  render() {
    const { classNames, diagnostics } = this.props

    let diagnosticInfo: any = undefined;

    return (
      <Content>
        <Header as="h1">Diagnostics</Header>
        <Message
          icon="info"
          header="Diagnostic Info"
          content={
            'The information below has just been collected directly from the miner.  Please include ' +
            'this information if you need to contact Obelisk for technical support.'
          }
        />

        <TextArea ref={this.setTextArea} className={classNames.diagnostics} rows={40}  value={"Testing 1 2 3"} disabled={true}/>
        <Button onClick={this.handleCopy}>COPY TO CLIPBOARD</Button>

      </Content>
    )
  }
}

const mapStateToProps = (state: any, props: any): ConnectProps => ({
  diagnostics: getDiagnostics(state.Main),
})

const diagnostics = withStyles()<any>(Diagnostics)

export default connect<ConnectProps, any, any>(mapStateToProps)(diagnostics)
