// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------
import { isType, Action } from 'typescript-fsa'
const updeep = require('updeep')

import { NetworkConfig, State } from './types'

import {
  clearLastErrorAC,
  clearNextRoutesAC,
  fetchCurrUser,
  fetchDashboardStatus,
  fetchMiningConfig,
  fetchNetworkConfig,
  fetchPoolsConfig,
  fetchSystemConfig,
  login,
  logout,
  setDeviceModelAC,
  setLastErrorAC,
  setRequestDoneAC,
  setRequestFailedAC,
  setRequestStartedAC,
  setRouteOnRequestFailedAC,
  setRouteOnRequestSuccessAC,
  setUploadFilenameAC,
  setUploadProgressAC,
  showSidebarAC,
  toggleSidebarAC,
  setPoolsConfig,
  clearFormStatusAC,
  setSystemConfig,
  changePassword,
  runUpgrade,
} from './actions'

const initialState: State = {
  showSidebar: false,
  username: '',
  deviceModel: '',

  dashboardStatus: {
    hashrateData: [],
    poolStatus: [],
    hashboardStatus: [],
    systemInfo: [],
  },

  networkConfig: {
    ipAddress: '',
    subnetMask: '',
    gateway: '',
    dnsServer: '',
    dhcpEnabled: 'disabled',
    hostname: '',
  },

  poolsConfig: [
    {
      url: '',
      worker: '',
      password: '',
    },
    {
      url: '',
      worker: '',
      password: '',
    },
    {
      url: '',
      worker: '',
      password: '',
    },
  ],
  forms: {
    poolForm: '',
    passwordForm: '',
    systemForm: '',
  },
  miningConfig: {
    optimizationMode: 'efficiency',
  },

  systemConfig: {
    timezone: 'America/New_York',
    ntpServer: 'pool.ntp.org',
  },

  activeRequestType: undefined,
  lastError: undefined,
  lastRequestParams: undefined,
  lastRequestType: undefined,

  uploadStatus: {
    percent: 0,
  },

  nextRouteOnFail: undefined,
  nextRouteOnSuccess: undefined,
}

// console.log(JSON.stringify(initialState.dashboardStatus, null, 0))

export const reducer = (state: State = initialState, action: Action<any>): State => {
  let newState: State = state

  // --------------------------------------------------------------------------------------------
  // Requests done
  // --------------------------------------------------------------------------------------------
  if (isType(action, setRequestStartedAC)) {
    const { requestAction } = action.payload
    newState = updeep(
      {
        activeRequestType: requestAction.type,
        lastRequestType: undefined,
        lastRequestParams: undefined,
        lastError: undefined,
      },
      state,
    )
  } else if (isType(action, setRequestFailedAC)) {
    const { errorResult } = action.payload

    if (errorResult.failSilently) {
      newState = updeep(
        {
          activeRequestType: undefined,
          lastRequestType: state.activeRequestType,
          lastRequestParams: errorResult.params,
        },
        state,
      )
    } else {
      newState = updeep(
        {
          activeRequestType: undefined,
          lastRequestType: state.activeRequestType,
          lastRequestParams: errorResult.params,
          lastError: errorResult.error,
        },
        state,
      )
    }
  } else if (isType(action, setRequestDoneAC)) {
    newState = updeep(
      {
        activeRequestType: undefined,
        lastRequestType: undefined,
        lastRequestParams: undefined,
        lastError: undefined,
      },
      state,
    )

    // -------------------------------------------------------------------------------------------
    // Login.done || fetchCurrUser.done
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, login.done) || isType(action, fetchCurrUser.done)) {
    const payload = action.payload as any
    const { username, deviceModel } = payload.data
    newState = updeep({ deviceModel, username, lastError: undefined }, newState)

    // -------------------------------------------------------------------------------------------
    // Login.failed
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, login.failed)) {
    const payload = action.payload as any
    const status = payload.response.status
    const statusText = payload.response.statusText
    let error = 'Invalid username or password'
    if (status !== 401) {
      error = 'Server error: ' + status + ' ' + statusText
    }
    newState = updeep({ username: undefined, lastError: error }, newState)

    // -------------------------------------------------------------------------------------------
    // Logout.done || Logout.failed
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, logout.done) || isType(action, logout.failed)) {
    newState = initialState

    // --------------------------------------------------------------------------------------------
    // Sidebar
    // --------------------------------------------------------------------------------------------
  } else if (isType(action, showSidebarAC)) {
    const { show } = action.payload
    newState = updeep({ showSidebar: show }, state)
  } else if (isType(action, toggleSidebarAC)) {
    newState = updeep({ showSidebar: !state.showSidebar }, state)

    // --------------------------------------------------------------------------------------------
    // Device Model
    // --------------------------------------------------------------------------------------------
  } else if (isType(action, setDeviceModelAC)) {
    const { deviceModel } = action.payload
    newState = updeep({ deviceModel }, state)

    // -------------------------------------------------------------------------------------------
    // SetRouteOnRequestSuccess
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, setRouteOnRequestSuccessAC)) {
    const { route } = action.payload
    newState = updeep({ nextRouteOnSuccess: route }, newState)

    // -------------------------------------------------------------------------------------------
    // SetRouteOnRequestFail
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, setRouteOnRequestFailedAC)) {
    const { route } = action.payload
    newState = updeep({ nextRouteOnFail: route }, newState)

    // -------------------------------------------------------------------------------------------
    // ClearNextRoutes
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, clearNextRoutesAC)) {
    newState = updeep({ nextRouteOnFail: undefined, nextRouteOnSuccess: undefined }, newState)

    // -------------------------------------------------------------------------------------------
    // SetLastError
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, setLastErrorAC)) {
    const { error } = action.payload
    newState = updeep({ lastError: error }, newState)

    // -------------------------------------------------------------------------------------------
    // ClearLastError
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, clearLastErrorAC)) {
    newState = updeep({ lastError: undefined }, newState)

    // -------------------------------------------------------------------------------------------
    // Network Config
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, fetchNetworkConfig.done)) {
    const payload: any = action.payload
    const networkConfig = payload.data as NetworkConfig
    newState = updeep(
      {
        networkConfig: {
          ...payload.data,
          dhcpEnabled: networkConfig.dhcpEnabled ? 'enabled' : 'disabled',
        },
      },
      newState,
    )

    // -------------------------------------------------------------------------------------------
    // System Config
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, fetchSystemConfig.done)) {
    const payload = action.payload as any
    newState = updeep({ systemConfig: payload.data }, newState)

    // -------------------------------------------------------------------------------------------
    // Mining Config
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, fetchMiningConfig.done)) {
    const payload = action.payload as any
    newState = updeep({ miningConfig: payload.data }, newState)

    // -------------------------------------------------------------------------------------------
    // Pools Config
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, fetchPoolsConfig.done)) {
    const payload = action.payload as any
    newState = updeep({ poolsConfig: payload.data }, newState)

    // -------------------------------------------------------------------------------------------
    // Dashboard Status
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, fetchDashboardStatus.done)) {
    const payload = action.payload as any
    newState = updeep({ dashboardStatus: payload.data }, newState)

    // -------------------------------------------------------------------------------------------
    // Upload status
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, setUploadFilenameAC)) {
    const { filename } = action.payload
    newState = updeep({ uploadStatus: { filename } }, newState)
  } else if (isType(action, setUploadProgressAC)) {
    const { percent } = action.payload
    newState = updeep({ uploadStatus: { percent } }, newState)

    // -------------------------------------------------------------------------------------------
    // Upgrade Message
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, runUpgrade.done)) {
    const payload = action.payload as any
    newState = updeep({ upgradeMessage: payload.data.message }, newState)

    // -------------------------------------------------------------------------------------------
    // Pool form
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, setPoolsConfig.started)) {
    newState = updeep({ forms: { poolForm: 'started' } }, newState)
  } else if (isType(action, setPoolsConfig.failed)) {
    newState = updeep({ forms: { poolForm: 'failed' } }, newState)
  } else if (isType(action, setPoolsConfig.done)) {
    newState = updeep({ forms: { poolForm: 'done' } }, newState)
  } else if (isType(action, clearFormStatusAC)) {
    newState = updeep({ forms: initialState.forms }, newState)
    // -------------------------------------------------------------------------------------------
    // System form
    // -------------------------------------------------------------------------------------------
  } else if (isType(action, setSystemConfig.started)) {
    newState = updeep({ forms: { systemForm: 'started' } }, newState)
  } else if (isType(action, setSystemConfig.failed)) {
    newState = updeep({ forms: { systemForm: 'failed' } }, newState)
  } else if (isType(action, setSystemConfig.done)) {
    newState = updeep({ forms: { systemForm: 'done' } }, newState)
  } else if (isType(action, changePassword.started)) {
    newState = updeep({ forms: { passwordForm: 'started' } }, newState)
  } else if (isType(action, changePassword.failed)) {
    newState = updeep({ forms: { passwordForm: 'failed' } }, newState)
  } else if (isType(action, changePassword.done)) {
    newState = updeep({ forms: { passwordForm: 'done' } }, newState)
  }

  return newState
}
