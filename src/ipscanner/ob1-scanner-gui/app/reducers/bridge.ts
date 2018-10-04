import { IAction, IActionWithPayload } from '../actions/helpers'
import * as actions from '../actions/bridge'

export interface Log {
  level: string
  msg: string
  time: string
}

export interface Miner {
  ip: string
  model: string
  mac: string
  firmwareVersion: string
}

export interface ScanData {
  status: boolean
  payload: Miner[]
}

export interface BeforeScan {
  subnet?: string
  bitmask?: string
}

export interface BeforeUpdate {
  model: string
  host: string
  sshuser: string
  sshpass: string
}

export interface IState {
  logs: Log[]
  miners: Miner[]
  loading: string
}

const INITIAL_STATE: IState = {
  logs: [],
  miners: [],
  loading: 'initial'
}

export default function bridgeReducer(
  state: IState = INITIAL_STATE,
  action: any
) {
  switch (action.type) {
    case actions.startScan.type:
    case actions.spawnMDNS.type:
      return { ...state, miners: [], logs: [], loading: 'started' }
    case actions.startScan.type:
      return { ...state, logs: [] }
    case actions.receiveLogAction.type:
      return { ...state, logs: [...state.logs, action.payload] }
    case actions.scanComplete.type:
      return {
        ...state,
        miners: [...action.payload.payload],
        loading: 'finished'
      }
    default:
      return state
  }
}
