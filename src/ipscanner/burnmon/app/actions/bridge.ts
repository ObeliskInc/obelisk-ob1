import { actionCreator, actionCreatorVoid } from "./helpers"
import {
  Log,
  ScanData,
  BeforeScan,
  BeforeUpdate,
  IdentifyArgs
} from "../reducers/bridge"

// burnmon actions
export const startScan = actionCreator<BeforeScan>("START_SCAN")
export const identifyObelisk = actionCreator<IdentifyArgs>("IDENTIFY_OBELISK")
export const spawnMDNS = actionCreatorVoid("SPAWN_MDNS")
export const scanComplete = actionCreator<ScanData>("SCAN_COMPLETE")
export const receiveLogAction = actionCreator<Log>("RECEIVE_LOG")
export const identifyDevice = actionCreator<BeforeScan>("IDENTIFY_DEVICE")

// obupdater actions
export const startUpgrade = actionCreator<BeforeUpdate>("START_UPDATE")
