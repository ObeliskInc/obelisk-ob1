// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------
import { AxiosInstance } from 'axios'
import { History } from 'history'
import * as _ from 'lodash'
import { Action } from 'redux'
const ReduxLogic = require('redux-logic')
const createLogic = ReduxLogic.createLogic

import { getHistory } from 'utils'
import {
  changePassword,
  clearNextRoutes,
  fetchCurrUser,
  fetchDashboardStatus,
  fetchMiningConfig,
  fetchNetworkConfig,
  fetchPoolsConfig,
  fetchSystemConfig,
  login,
  logout,
  rebootMiner,
  setDeviceModel,
  setLastError,
  setMiningConfig,
  setNetworkConfig,
  setPoolsConfig,
  setRequestDone,
  setRequestFailed,
  setRequestStarted,
  setSystemConfig,
  setUploadFilename,
  setUploadProgress,
  uploadFirmwareFile,
  resetConfig,
} from './actions'
import { State } from './types'

interface LogicDeps {
  action: any
  getState: () => { Main: State }
  dispatch: (action: Action) => void
  axios: AxiosInstance
  history: History
}

export const loginLogic = createLogic({
  type: login.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: login.done,
    failType: login.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    // TODO: Better type
    const { action, axios } = deps
    let result
    result = await axios.post('/api/login', action.payload)
    if (result.status === 200) {
      const deviceModel = _.get(result, 'deviceModel')
      dispatch(setDeviceModel(deviceModel))
      getHistory().push('/dashboard')
    }
    done()
    return result
  },
})

export const logoutLogic = createLogic({
  type: logout.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: logout.done,
    failType: logout.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    // TODO: Better type
    const { action, axios } = deps
    await axios.post('/api/logout', action.payload)
    getHistory().push('/login')
    done()
  },
})

export const fetchCurrUserLogic = createLogic({
  type: fetchCurrUser.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: fetchCurrUser.done,
    failType: fetchCurrUser.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    // TODO: Better type
    const { axios } = deps
    let result
    try {
      result = await axios.get('/api/currUser')
    } catch (err) {
      // If the user is not logged in, switch to the login view
      getHistory().push('/login')
    }
    done()
    return result
  },
})

export const fetchNetworkConfigLogic = createLogic({
  type: fetchNetworkConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: fetchNetworkConfig.done,
    failType: fetchNetworkConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    // TODO: Better type
    const { axios } = deps
    const result = await axios.get('/api/config/network')
    done()
    return result
  },
})

export const fetchDashboardStatusLogic = createLogic({
  type: fetchDashboardStatus.started,

  processOptions: {
    dispatchReturn: true,
    successType: fetchDashboardStatus.done,
    failType: fetchDashboardStatus.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { axios } = deps

    // Make sure we are not already in the middle of a request
    // TODO: activeRequestType is already set to FETCH_DASHBOARD_STATUS_STARTED
    // somehow even the first time, so need another way to prevent
    // sending another request when the previous one is still in progress.
    // const s = getState()
    // const { activeRequestType } = s.Main
    // if (activeRequestType === 'FETCH_DASHBOARD_STATUS_STARTED') {
    //   done()
    //   return
    // }

    const result: any = await axios.get('/api/status/dashboard')
    done()
    return result
  },
})

export const fetchSystemConfigLogic = createLogic({
  type: fetchSystemConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: fetchSystemConfig.done,
    failType: fetchSystemConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    // TODO: Better type
    const { axios } = deps
    const result = await axios.get('/api/config/system')
    done()
    return result
  },
})

export const fetchMiningConfigLogic = createLogic({
  type: fetchMiningConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: fetchMiningConfig.done,
    failType: fetchMiningConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    // TODO: Better type
    const { axios } = deps
    const result = await axios.get('/api/config/mining')
    done()
    return result
  },
})

export const fetchPoolsConfigLogic = createLogic({
  type: fetchPoolsConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: fetchPoolsConfig.done,
    failType: fetchPoolsConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    // TODO: Better type
    const { axios } = deps
    const result = await axios.get('/api/config/pools')
    done()
    return result
  },
})

export const setNetworkConfigLogic = createLogic({
  type: setNetworkConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: setNetworkConfig.done,
    failType: setNetworkConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action, axios } = deps
    const networkConfig: any = {
      ...action.payload,
      dhcpEnabled: action.payload.dhcpEnabled === 'enabled' ? true : false,
    }
    return axios.post(`/api/config/network`, networkConfig)
  },
})

export const setPoolsConfigLogic = createLogic({
  type: setPoolsConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: setPoolsConfig.done,
    failType: setPoolsConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action, axios } = deps
    return axios.post(`/api/config/pools`, action.payload)
  },
})

export const setSystemConfigLogic = createLogic({
  type: setSystemConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: setSystemConfig.done,
    failType: setSystemConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action, axios } = deps
    return axios.post(`/api/config/system`, action.payload)
  },
})

export const resetSystemConfigLogic = createLogic({
  type: resetConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: resetConfig.done,
    failType: resetConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { axios } = deps
    return axios.post(`/api/action/resetMinerConfig`)
  },
})

export const setMiningConfigLogic = createLogic({
  type: setMiningConfig.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: setMiningConfig.done,
    failType: setMiningConfig.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action, axios } = deps
    return axios.post(`/api/config/mining`, action.payload)
  },
})

export const uploadFirmwareFileLogic = createLogic({
  type: uploadFirmwareFile.started.type,

  processOptions: {
    dispatchReturn: false,
    // successType: uploadFirmwareFile.done,
    // failType: uploadFirmwareFile.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action, axios } = deps
    const uploadParams = action.payload
    dispatch(setUploadFilename(uploadParams.file.name))
    dispatch(setUploadProgress(0))
    const fileSize = uploadParams.file.size
    const FILE_BLOCK_SIZE = 4 * 1024

    let offset = 0
    let endOffset = Math.min(fileSize, FILE_BLOCK_SIZE)

    const reader = new FileReader()

    // If we use onloadend, we need to check the readyState.
    reader.onloadend = async function(evt: any) {
      if (evt.target.readyState === FileReader.DONE) {
        const data = btoa(evt.target.result)

        try {
          await axios.post(`/api/action/uploadFirmwareFile`, { data, offset })
        } catch (e) {
          dispatch(setLastError(e.response.statusText))
          done()
          return
        }

        // Read the next block
        offset = endOffset

        const p = Math.round((offset * 100) / fileSize)
        // console.log('offset=' + offset + '  fileSize=' + fileSize + ' p=' + p)
        dispatch(setUploadProgress(p))

        if (offset < fileSize) {
          endOffset = Math.min(offset + FILE_BLOCK_SIZE, fileSize)
          const blob2 = uploadParams.file.slice(offset, endOffset)
          reader.readAsBinaryString(blob2)
        } else {
          // Nothing else to send
          done()
        }
      }
    }

    // Kick off the initial read
    const blob = uploadParams.file.slice(offset, endOffset)
    reader.readAsBinaryString(blob)

    return true
    // return axios.post(`/api/action/uploadFirmwareFile`, action.payload)
  },
})

// export const uploadFirmwareFileFragmentLogic = createLogic({
//   type: uploadFirmwareFileFragment.started.type,

//   processOptions: {
//     dispatchReturn: true,
//     successType: uploadFirmwareFileFragment.done,
//     failType: uploadFirmwareFileFragment.failed,
//   },

//   async process(deps: LogicDeps, dispatch: any, done: () => void) {
//     const { action, axios } = deps
//     return axios.post(`/api/action/uploadFirmwareFile`, action.payload)
//   },
// })

export const changePasswordLogic = createLogic({
  type: changePassword.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: changePassword.done,
    failType: changePassword.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action, axios } = deps
    return axios.post(`/api/action/changePassword`, action.payload)
  },
})

export const rebootMinerLogic = createLogic({
  type: rebootMiner.started.type,

  processOptions: {
    dispatchReturn: true,
    successType: rebootMiner.done,
    failType: rebootMiner.failed,
  },

  async process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { axios } = deps
    return axios.post(`/api/action/reboot`)
  },
})

export const requestStartedLogic = createLogic({
  type: /.*_STARTED$/,

  processOptions: {
    dispatchReturn: false,
  },

  process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action } = deps
    dispatch(setRequestStarted(action))
    done()
  },
})

export const requestDoneLogic = createLogic({
  type: /.*_DONE$/,

  processOptions: {
    dispatchReturn: false,
  },

  process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { getState, history } = deps
    const state = getState().Main
    dispatch(setRequestDone())

    if (state.nextRouteOnSuccess) {
      if (state.nextRouteOnSuccess === 'back') {
        if (history.length >= 1) {
          history.goBack()
        }
      } else {
        history.push(state.nextRouteOnSuccess)
      }
      dispatch(clearNextRoutes())
    }
    done()
  },
})

export const requestFailLogic = createLogic({
  type: /.*_FAILED$/,

  processOptions: {
    dispatchReturn: false,
  },

  process(deps: LogicDeps, dispatch: any, done: () => void) {
    const { action, getState, history } = deps
    const failSilently = _.get(action, ['meta', 'meta', 'failSilently'], false)
    const state = getState().Main
    const statusCode: number =
      _.get(action, ['payload', 'error', 'response', 'status']) ||
      _.get(action, ['payload', 'response', 'status'])
    let errorMsg
    if (statusCode === 401) {
      // TODO: This is a hack - for some reason, passport is returning a 404
      //       when the user is not found in the LocalStrategy lookup.
      //       See if there is a better way to get the error message from the server.
      errorMsg = 'Invalid username or password'
    } else {
      errorMsg =
        _.get(action, ['payload', 'response', 'data', 'error', 'message']) ||
        _.get(action, ['payload', 'response', 'data', 'error']) ||
        _.get(action, ['payload', 'message'])
    }

    dispatch(
      setRequestFailed({
        error: errorMsg,
        params: undefined,
        failSilently,
      }),
    )

    if (state.nextRouteOnFail) {
      if (state.nextRouteOnFail === 'back') {
        if (history.length >= 1) {
          history.goBack()
        }
      } else {
        history.push(state.nextRouteOnFail)
      }
      dispatch(clearNextRoutes())
    }
    done()
  },
})

export default [
  loginLogic,
  logoutLogic,
  fetchCurrUserLogic,
  fetchNetworkConfigLogic,
  fetchSystemConfigLogic,
  fetchPoolsConfigLogic,

  fetchDashboardStatusLogic,
  resetSystemConfigLogic,
  setNetworkConfigLogic,
  setPoolsConfigLogic,
  setSystemConfigLogic,
  setMiningConfigLogic,
  uploadFirmwareFileLogic,
  changePasswordLogic,
  rebootMinerLogic,
  requestDoneLogic,
  requestFailLogic,
  requestStartedLogic,
]
