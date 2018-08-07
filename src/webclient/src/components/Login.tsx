// import * as cx from 'classnames'
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import { withRouter, BrowserRouterProps } from 'react-router-dom'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Image } from 'semantic-ui-react'

import { Dialog } from 'components/Dialog'
import { login } from 'modules/Main/actions'
import { getActiveRequestType, getLastError } from 'modules/Main/selectors'
import { dialogStyles } from 'styles/dialogs'
import * as Colors from 'utils/colors'

interface ConnectProps {
  lastError?: string
  activeRequestType?: string
}

interface State {
  username: string
  password: string
  isSubmitted: boolean
  localError?: string
}

type CombinedProps = ConnectProps & BrowserRouterProps & InjectedProps & DispatchProp<any>

class Login extends React.PureComponent<CombinedProps, State> {
  public static styles: InputSheet<{}> = {
    container: {
      $debugName: 'container',
      display: 'flex',
      flex: 1,
      background: Colors.black,
      color: Colors.mutedText,
      overflowY: 'hidden',
      justifyContent: 'space-around',
      alignItems: 'center',
      height: '100%',
    },
    innerContainer: {
      display: 'flex',
      flexDirection: 'column',
    },
    logo: {
      width: 250,
      alignSelf: 'center',
    },
    fieldContainer: { ...dialogStyles.fieldContainer },
    fieldLabel: { ...dialogStyles.fieldLabel },
    fieldInput: { ...dialogStyles.fieldInput },
    message: { ...dialogStyles.message },
    successMessage: { ...dialogStyles.successMessage },
    textLink: { ...dialogStyles.textLink },
    errorMessage: { ...dialogStyles.errorMessage },

    linkContainer: {
      $debugName: 'linkContainer',
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
    },
  }

  constructor(props: CombinedProps) {
    super(props)
    this.state = {
      username: '',
      password: '',
      isSubmitted: false,
      localError: undefined,
    }
  }

  handleLogin = () => {
    const { dispatch } = this.props
    const { username, password } = this.state

    if (!dispatch) {
      return
    }

    if (!username) {
      this.setState({ localError: 'Please enter your username.' })
      return
    }

    if (!password) {
      this.setState({ localError: 'Please enter your password.' })
      return
    }

    this.setState({ isSubmitted: true, localError: undefined })
    dispatch(login.started({ username, password }))
  }

  handleChangeUsername = (event: any) => {
    this.setState({ username: event.target.value })
  }

  handleChangePassword = (event: any) => {
    this.setState({ password: event.target.value })
  }

  handleKeyDown = (e: React.KeyboardEvent<any>) => {
    if (e.key === 'Enter') {
      this.handleLogin()
    }
  }

  // TODO: Add a componentDidUpdate() or similar call to check if the user is already
  //       logged in, and then change route to /dashboard.

  render() {
    const { classNames, lastError } = this.props
    const { username, password, isSubmitted, localError } = this.state

    const title = 'OBELISK LOGIN'
    const message = 'Please enter your username and password and click LOGIN.'

    let submitMessage
    if (isSubmitted && !localError && !lastError) {
      submitMessage = <div className={classNames.successMessage}>Logging in...</div>
    }

    const error = localError || lastError
    const errorMessage = error ? <div className={classNames.errorMessage}>{error}</div> : undefined

    const bodyContent = (
      <div>
        <div className={classNames.message}>{message}</div>
        <div className={classNames.fieldContainer}>
          <label className={classNames.fieldLabel}>USERNAME</label>
          <input
            name="username"
            value={username}
            type={'text'}
            onChange={this.handleChangeUsername}
            className={classNames.fieldInput}
            placeholder="Username"
          />
        </div>
        <div className={classNames.fieldContainer}>
          <label className={classNames.fieldLabel}>PASSWORD</label>
          <input
            name="password"
            value={password}
            type={'password'}
            onChange={this.handleChangePassword}
            onKeyDown={this.handleKeyDown}
            className={classNames.fieldInput}
            placeholder="Password"
          />
        </div>
        {submitMessage}
        {errorMessage}
      </div>
    )

    const footerContent = (
      <div>
        <Button className="formButton" onClick={this.handleLogin}>
          LOGIN
        </Button>
      </div>
    )

    return (
      <div className={classNames.container}>
        <div className={classNames.innerContainer}>
          <Image src={require('../images/obelisk-logo.png')} className={classNames.logo} />
          <Dialog title={title} bodyContent={bodyContent} footerContent={footerContent} />
        </div>
      </div>
    )
  }
}

const mapStateToProps = (state: any, props: any): ConnectProps => ({
  lastError: getLastError(state.Main),
  activeRequestType: getActiveRequestType(state.Main),
})

const loginComponent = withStyles()<any>(Login)

export default withRouter(connect<ConnectProps, any, any>(mapStateToProps)(loginComponent))
