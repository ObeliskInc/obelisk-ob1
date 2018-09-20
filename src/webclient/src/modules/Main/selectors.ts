// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------
// import { createSelector } from 'reselect'

import { login } from './actions'
import {
  DashboardStatus,
  MiningConfig,
  NetworkConfig,
  PoolConfig,
  State,
  SystemConfig,
  UploadStatus,
} from './types'

export const getShowSidebar = (state: State): boolean => state.showSidebar
export const getUsername = (state: State): string | undefined => state.username

export const getDashboardStatus = (state: State): DashboardStatus => state.dashboardStatus

export const getNetworkConfig = (state: State): NetworkConfig => state.networkConfig
export const getPoolsConfig = (state: State): PoolConfig[] => state.poolsConfig
export const getSystemConfig = (state: State): SystemConfig => state.systemConfig
export const getMiningConfig = (state: State): MiningConfig => state.miningConfig

export const getIsLoginInProgress = (state: State): boolean =>
  state.activeRequestType === login.started.type

export const getUploadStatus = (state: State): UploadStatus => state.uploadStatus

export const getUpgradeMessage = (state: State): string | undefined => state.upgradeMessage

export const getActiveRequestType = (state: State): string | undefined => state.activeRequestType

export const getLastError = (state: State): string | undefined => state.lastError

export const getLastRequestParams = (state: State): string | undefined => state.lastRequestParams

export const getLastRequest = (state: State): string | undefined => state.lastRequestType

export const getFirmwareVersion = (state: State): string | undefined => state.firmwareVersion

export const getCgMinerVersions = (state: State): string | undefined => state.cgminerVersions
