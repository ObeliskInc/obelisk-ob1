import { createLogic } from 'redux-logic'
import * as actions from '../actions/bridge'
import { ipcRenderer } from 'electron'
import { store } from '../index'

function splitAndClean(data: string) {
  const dArr = data.split('\n')
  return dArr.filter(s => s != '')
}

ipcRenderer.on('obscanner', function(_: string, data: string) {
  if (data) {
    try {
      splitAndClean(data).forEach(d => {
        console.log(d)
        const parsedData = JSON.parse(d)
        if ('level' in parsedData) {
          store.dispatch(actions.receiveLogAction(parsedData))
        } else if ('payload' in parsedData) {
          if (parsedData.payload === null) {
            parsedData.payload = []
          }
          store.dispatch(actions.scanComplete(parsedData))
        }
      })
    } catch (e) {
      console.warn('Error parsing JSON', data)
    }
  }
})

export const startScanLogic = createLogic({
  type: actions.startScan.type,
  process({ action }: any) {
    let cmd = 'scan'
    if (action.payload && action.payload.subnet) {
      cmd = `scan -i ${action.payload.subnet}/${action.payload.bitmask}`
    }
    ipcRenderer.send('obexec', cmd)
  }
})

export const spawnMDNSLogic = createLogic({
  type: actions.spawnMDNS.type,
  process() {
    let cmd = 'mdns'
    ipcRenderer.send('obspawn', cmd)
  }
})

export const startUpdateLogic = createLogic({
  type: actions.startUpgrade.type,
  process({ action }: any) {
    const data = JSON.stringify(action.payload)
    ipcRenderer.send('obupdate', data)
  }
})
