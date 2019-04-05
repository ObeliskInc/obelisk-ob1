import { IAction, IActionWithPayload } from "../actions/helpers"
import * as actions from "../actions/bridge"

export interface Log {
  level: string
  msg: string
  time: string
}

export interface HashboardBurninStatus {
  type: string
  status: string
  expectedHashrate: number
  actualHashrate: number
}

export interface MinerBurninStatus {
  deviceStatus: string
  hashboardStatus: HashboardBurninStatus[]
}

export interface Miner {
  ip: string
  model: string
  mac: string
  firmwareVersion: string
  firmwareUpdate?: string
  firmwareRollback?: string
  burninStatus?: MinerBurninStatus
  isUpgradeInProgress?: boolean
}

export interface ScanData {
  status: boolean
  payload: Miner[]
}

export interface BeforeScan {
  subnet?: string
  bitmask?: string
  uiuser: string
  uipass: string
}

export interface BeforeUpdate {
  model: string
  host: string
  sshuser: string
  sshpass: string
  uiuser: string
  uipass: string
}

export interface IdentifyArgs {
  host: string
  uiuser: string
  uipass: string
}

export interface IState {
  logs: Log[]
  miners: Miner[]
  loading: string
}

const INITIAL_STATE: IState = {
  logs: [],
  miners: [],
  loading: "initial"
}

export default function bridgeReducer(
  state: IState = INITIAL_STATE,
  action: any
) {
  switch (action.type) {
    case actions.startScan.type:
    case actions.spawnMDNS.type:
      return { ...state, miners: [], logs: [], loading: "started" }
    case actions.startScan.type:
      return { ...state, logs: [] }
    case actions.receiveLogAction.type:
      return { ...state, logs: [...state.logs, action.payload] }
    case actions.startUpgrade.type: {
      const ip = action.payload.host
      const index = state.miners.findIndex((miner: Miner) => miner.ip === ip)
      if (index >= 0) {
        const replaceMiner = (miners: Miner[]) => {
          const newMiners = [...miners]
          newMiners[index] = { ...newMiners[index], isUpgradeInProgress: true }
          return newMiners
        }

        return {
          ...state,
          miners: replaceMiner(state.miners)
        }
      }
      return state
    }
    case actions.scanComplete.type:
      return {
        ...state,
        miners: [...action.payload.payload],
        loading: "finished"
      }
    default:
      return state
  }
}
