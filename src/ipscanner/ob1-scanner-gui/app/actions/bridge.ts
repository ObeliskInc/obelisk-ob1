import { actionCreator, actionCreatorVoid } from './helpers'
import { Log, ScanData, BeforeScan } from '../reducers/bridge'

// obscanner actions
export const startScan = actionCreator<BeforeScan>('START_SCAN')
export const spawnMDNS = actionCreatorVoid('SPAWN_MDNS')
export const scanComplete = actionCreator<ScanData>('SCAN_COMPLETE')
export const receiveLogAction = actionCreator<Log>('RECEIVE_LOG')

//

// export function incrementIfOdd() {
//   return (dispatch: Function, getState: Function) => {
//     const { counter } = getState()

//     if (counter % 2 === 0) {
//       return
//     }

//     dispatch(increment())
//   }
// }

// export function incrementAsync(delay: number = 1000) {
//   return (dispatch: Function) => {
//     setTimeout(() => {
//       dispatch(increment())
//     }, delay)
//   }
// }
