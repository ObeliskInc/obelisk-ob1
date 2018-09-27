// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import { matchPath, withRouter, Route, Switch } from 'react-router-dom'
import { BrowserRouterProps } from 'react-router-dom'

import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Icon, Image, Menu, Responsive, Segment, Sidebar } from 'semantic-ui-react'

// import Main from 'components/Main'
import SidebarMenuItem from 'components/SidebarMenuItem'
import { fetchCurrUser, fetchVersions, logout, showSidebar, toggleSidebar } from 'modules/Main/actions'
import { getShowSidebar, getFirmwareVersion } from 'modules/Main/selectors'
import Dashboard from 'panels/Dashboard'
import MiningPanel from 'panels/MiningPanel'
import NetworkPanel from 'panels/NetworkPanel'
import PoolsPanel from 'panels/PoolsPanel'
import SystemPanel from 'panels/SystemPanel'

import { getHistory } from 'utils'

const history = getHistory()

interface ConnectProps {
  isShowSidebar: boolean
  firmwareVersion?: string
}

type CombinedProps = ConnectProps & BrowserRouterProps & InjectedProps & DispatchProp<any>

class App extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    content: {
      $debugName: 'container',
      display: 'flex',
      flex: 1,
      flexDirection: 'column',
      height: '100%',
      overflowY: 'hidden',
    },
    segment: {
      display: 'flex',
      flexDirection: 'column',
      flex: 1,
      width: 'calc(100% - 200px)',
      padding: '20px !important',
    },
    header: {
      $debugName: 'header',
      display: 'flex',
      flex: 'none',
      height: 64,
      background: '#000000',
    },
    logo: {
      maxHeight: 64,
    },
    spacer: {
      flex: 1,
    },
    version: {
      marginBottom: 10,
    }
  }

  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchCurrUser.started({}))
      this.props.dispatch(fetchVersions.started({}))
    }
  }

  navigateToPath = (path: string) => {
    const { dispatch } = this.props

    history.push(path)
    if (dispatch) {
      dispatch(showSidebar(false))
    }
  }

  isActive(path: string): boolean {
    const match = matchPath(location.pathname, { path, exact: true })
    return !!match
  }

  toggleVisibility = () => {
    const { dispatch } = this.props
    if (dispatch) {
      dispatch(toggleSidebar())
    }
  }

  handleLogout = () => {
    const { dispatch } = this.props
    if (!dispatch) {
      return
    }

    dispatch(logout.started({}))
  }

  render() {
    // const { classNames } = this.props
    const { classNames, isShowSidebar, firmwareVersion } = this.props

    const menuItems = [
      <SidebarMenuItem
        key="dashboard"
        name="dashboard"
        label="DASHBOARD"
        iconName="dashboard"
        onClick={() => this.navigateToPath('/dashboard')}
        active={this.isActive('/dashboard')}
      />,
      <SidebarMenuItem
        key="pools"
        name="pools"
        label="POOLS"
        iconName="list"
        onClick={() => this.navigateToPath('/pools')}
        active={this.isActive('/pools')}
      />,
      <SidebarMenuItem
        key="mining"
        name="mining"
        label="MINING"
        iconName="diamond"
        onClick={() => this.navigateToPath('/mining')}
        active={this.isActive('/mining')}
      />,
      <SidebarMenuItem
        key="system"
        name="system"
        label="SYSTEM"
        iconName="microchip"
        onClick={() => this.navigateToPath('/system')}
        active={this.isActive('/system')}
      />,
      <SidebarMenuItem
        key="network"
        name="network"
        label="NETWORK"
        iconName="settings"
        onClick={() => this.navigateToPath('/network')}
        active={this.isActive('/network')}
      />,
    ]

    return (
      <div className="fullHeight">
        <Sidebar.Pushable as={Segment}>
          <Sidebar
            as={Menu}
            animation="push"
            width="thin"
            visible={isShowSidebar}
            icon="labeled"
            vertical={true}
            inverted={true}
          >
            {menuItems}
            <div className={classNames.spacer} />
            <div className={classNames.version}>{firmwareVersion}</div>
            <Button onClick={this.handleLogout} icon="log out" content="LOGOUT" />
          </Sidebar>
          <Sidebar.Pusher>
            <div className="main">
              <Responsive minWidth={768} className="sidebar">
                <Image src={require('../images/obelisk-logo.png')} />
                <Menu icon="labeled" vertical={true} inverted={true}>
                  {menuItems}
                  <div className={classNames.spacer} />
                  <div className={classNames.version}>{firmwareVersion}</div>
                  <Button onClick={this.handleLogout} icon="log out" content="LOGOUT" />
                </Menu>
              </Responsive>
              <div className={classNames.segment}>
                <Responsive maxWidth={768}>
                  <div className={classNames.header}>
                    <Button onClick={this.toggleVisibility} className="hamburgerMenu">
                      <Icon name="bars" />
                    </Button>
                    <Image
                      src={require('../images/obelisk-logo.png')}
                      inline={true}
                      className={classNames.logo}
                    />
                  </div>
                </Responsive>
                <div className={classNames.content}>
                  <Switch>
                    <Route path="/dashboard" component={Dashboard} />
                    <Route path="/pools" component={PoolsPanel} />
                    <Route path="/mining" component={MiningPanel} />
                    <Route path="/network" component={NetworkPanel} />
                    <Route path="/system" component={SystemPanel} />
                    <Route component={Dashboard} />
                  </Switch>
                </div>
              </div>
            </div>
          </Sidebar.Pusher>
        </Sidebar.Pushable>
      </div>
    )
  }
}

const mapStateToProps = (state: any, props: any): ConnectProps => ({
  isShowSidebar: getShowSidebar(state.Main),
  firmwareVersion: getFirmwareVersion(state.Main),
})

const app = withStyles()<any>(App)

export default withRouter(connect<ConnectProps, any, any>(mapStateToProps)(app))
