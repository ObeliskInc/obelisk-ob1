// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------
// User interfaces
// -------------------------------------------------------------------------------------------------
export interface UserInfo {
  username: string
}

export interface LoginRequest {
  username: string
  password: string
}

export interface LoginResponse {
  username: string
  deviceModel: string
}

// -------------------------------------------------------------------------------------------------
// Config interfaces
// -------------------------------------------------------------------------------------------------
export interface NetworkConfig {
  ipAddress: string
  subnetMask: string
  dhcpEnabled: string
  gateway: string
  dnsServer: string
  hostname: string
}

export interface PoolConfig {
  url: string
  worker: string
  password: string
}

export interface MiningConfig {
  optimizationMode: string
}

export const MAX_POOLS = 3

export interface UserInfo {
  userInfo: UserInfo
}

export interface SystemConfig {
  timezone?: string
  ntpServer?: string

  // These are here just to satisfy Formik type handling - never send them with SystemConfig
  oldPassword?: string
  newPassword?: string
}

export interface HashingConfig {
  clockrate: number
}

// -------------------------------------------------------------------------------------------------
// Status interfaces
// -------------------------------------------------------------------------------------------------
export interface HashrateEntry {
  time: number
  'Board 1'?: number
  'Board 2'?: number
  'Board 3'?: number
  Total?: number
}

export interface StatsEntry {
  name: string
  value: string
}

export interface PoolStatus {
  url: string
  worker: string
  status: string
  accepted: number
  rejected: number
}

export interface HashboardStatus {
  status: string
  accepted: number
  rejected: number
  boardTemp: number
  chipTemp: number
  powerSupplyTemp: number
  mhsAvg: number
  mhs1m: number
  mhs5m: number
  mhs15m: number
}

export interface DashboardStatus {
  hashrateData: HashrateEntry[]
  poolStatus: PoolStatus[]
  hashboardStatus: HashboardStatus[]
  systemInfo: StatsEntry[]
}

export interface HashboardStats {
  currHashrate: number // Averaged over the last second
  avgHashrate: number // Averaged over the last hour?
  asicStatus: string[] // One entry per ASIC chip on the board
}

export interface MinerStats {
  hashboardStats: HashboardStats[] // One entry per hashing board
  numBlocksFound: number
  totalHashes: number
  uptime: number
}

export interface ChangePasswordParams {
  username: string
  oldPassword: string
  newPassword: string
}

export interface UploadFirmwareFileParams {
  // TODO: This is a File object. Try to find a type for this.
  file: any
}

export interface FileFragment {
  data: string
  offset: number
}

export interface UploadStatus {
  filename?: string
  percent: number
}

export interface FormState {
  poolForm: string
  systemForm: string
  passwordForm: string
}
// Module State
export interface State {
  showSidebar: boolean
  deviceModel: string
  username: string

  // Configs
  networkConfig: NetworkConfig
  poolsConfig: PoolConfig[]
  systemConfig: SystemConfig
  miningConfig: MiningConfig

  // Statuses & Stats
  dashboardStatus: DashboardStatus
  minerStats?: MinerStats

  // Requests
  activeRequestType?: string
  lastRequestType?: string
  lastRequestParams?: any
  lastError?: string

  // Upload status
  uploadStatus: UploadStatus
  upgradeMessage?: string

  // Change routes when a request succeeds/fails
  nextRouteOnSuccess?: string
  nextRouteOnFail?: string

  // UI forms
  forms: FormState
}

export interface RunUpgradeResp {
  message: string
}
