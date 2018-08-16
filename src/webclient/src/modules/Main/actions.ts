// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import { Action } from 'redux'
import actionCreatorFactory from 'typescript-fsa'
const actionCreator = actionCreatorFactory()

import { ApiError, ApiOK } from 'utils/types'
import {
  ChangePasswordParams,
  DashboardStatus,
  LoginRequest,
  LoginResponse,
  MiningConfig,
  NetworkConfig,
  PoolConfig,
  SystemConfig,
  UploadFirmwareFileParams,
} from './types'

export const showSidebarAC = actionCreator<{ show: boolean }>('SHOW_SIDEBAR')
export const showSidebar = (show: boolean) => showSidebarAC({ show })

export const toggleSidebarAC = actionCreator<{}>('TOGGLE_SIDEBAR')
export const toggleSidebar = () => toggleSidebarAC({})

export const setDeviceModelAC = actionCreator<{ deviceModel: string }>('SET_DEVICE_MODEL')
export const setDeviceModel = (deviceModel: string) => setDeviceModelAC({ deviceModel })

export const login = actionCreator.async<LoginRequest, LoginResponse, ApiError>('LOGIN')

export const logout = actionCreator.async<{}, ApiOK, ApiError>('LOGOUT')

export const fetchCurrUser = actionCreator.async<{}, ApiOK, ApiError>('FETCH_CURR_USER', {
  meta: { failSilently: true },
})

export const fetchNetworkConfig = actionCreator.async<any, NetworkConfig, ApiError>(
  'FETCH_NETWORK_CONFIG',
)
export const fetchPoolsConfig = actionCreator.async<any, PoolConfig[], ApiError>(
  'FETCH_POOLS_CONFIG',
)
export const fetchSystemConfig = actionCreator.async<any, SystemConfig, ApiError>(
  'FETCH_SYSTEM_CONFIG',
)
export const fetchMiningConfig = actionCreator.async<any, MiningConfig, ApiError>(
  'FETCH_MINING_CONFIG',
)
export const fetchDashboardStatus = actionCreator.async<any, DashboardStatus, ApiError>(
  'FETCH_DASHBOARD_STATUS',
)

export const setNetworkConfig = actionCreator.async<NetworkConfig, ApiOK, ApiError>(
  'SET_NETWORK_CONFIG',
)
export const setPoolsConfig = actionCreator.async<PoolConfig[], ApiOK, ApiError>('SET_POOLS_CONFIG')
export const setSystemConfig = actionCreator.async<SystemConfig, ApiOK, ApiError>(
  'SET_SYSTEM_CONFIG',
)
export const setMiningConfig = actionCreator.async<MiningConfig, ApiOK, ApiError>(
  'SET_MINING_CONFIG',
)

export const resetConfig = actionCreator.async<{}, ApiOK, ApiError>('RESET_CONFIG')

export const setRouteOnRequestSuccessAC = actionCreator<{ route: string }>(
  'SET_ROUTE_ON_REQUEST_SUCCESS_STATUS',
)
export const setRouteOnRequestSuccess = (route: string) => setRouteOnRequestSuccessAC({ route })

export const setRouteOnRequestFailedAC = actionCreator<{ route: string }>(
  'SET_ROUTE_ON_REQUEST_FAILED_STATUS',
)
export const setRouteOnRequestFailed = (route: string) => setRouteOnRequestFailedAC({ route })

export const clearNextRoutesAC = actionCreator<{}>('CLEAR_NEXT_ROUTES')
export const clearNextRoutes = () => clearNextRoutesAC({})

// Set request state
export const setRequestStartedAC = actionCreator<{ requestAction: Action }>(
  'SET_REQUEST_STARTED_STATUS',
)
export const setRequestStarted = (requestAction: Action) => setRequestStartedAC({ requestAction })

export const setRequestDoneAC = actionCreator<{}>('SET_REQUEST_DONE_STATUS')
export const setRequestDone = () => setRequestDoneAC({})

export const setLastErrorAC = actionCreator<{ error: string }>('SET_MAIN_LAST_ERROR')
export const setLastError = (error: string) => setLastErrorAC({ error })

export const clearLastErrorAC = actionCreator<{}>('CLEAR_LAST_ERROR')
export const clearLastError = () => clearLastErrorAC({})

export const clearFormStatusAC = actionCreator<{}>('CLEAR_FORM_STATUS')
export const clearFormStatus = () => clearFormStatusAC({})

// TODO: Get better type for error from redux logic
interface ErrorResult {
  error: string
  params: any
  failSilently?: boolean
}
export const setRequestFailedAC = actionCreator<{ errorResult: ErrorResult }>(
  'SET_REQUEST_FAILED_STATUS',
)
export const setRequestFailed = (errorResult: ErrorResult) => setRequestFailedAC({ errorResult })

export const rebootMiner = actionCreator.async<{}, ApiOK, ApiError>('REBOOT_MINER')

export const changePassword = actionCreator.async<ChangePasswordParams, ApiOK, ApiError>(
  'CHANGE_PASSWORD',
)

export const setUploadFilenameAC = actionCreator<{ filename: string }>('SET_UPLOAD_FILENAME')
export const setUploadFilename = (filename: string) => setUploadFilenameAC({ filename })

export const setUploadProgressAC = actionCreator<{ percent: number }>('SET_UPLOAD_PROGRESS')
export const setUploadProgress = (percent: number) => setUploadProgressAC({ percent })

// This action is used by the UI to trigger the file reading logic that then sends fragments.
export const uploadFirmwareFile = actionCreator.async<UploadFirmwareFileParams, ApiOK, ApiError>(
  'UPLOAD_FILE',
)
