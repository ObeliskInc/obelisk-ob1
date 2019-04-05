import { createLogic } from "redux-logic"
import * as actions from "../actions/bridge"
import { ipcRenderer } from "electron"
import { store } from "../index"

function splitAndClean(data: string) {
  const dArr = data.split("\n")
  return dArr.filter(s => s != "")
}

ipcRenderer.on("obscanner", function(_: string, data: string) {
  if (data) {
    try {
      splitAndClean(data).forEach(d => {
        console.log(d)
        const parsedData = JSON.parse(d)
        if ("level" in parsedData) {
          store.dispatch(actions.receiveLogAction(parsedData))
        } else if ("payload" in parsedData) {
          if (parsedData.payload === null) {
            parsedData.payload = []
          }
          store.dispatch(actions.scanComplete(parsedData))

          // Format the results and log them
          const entries: any = []
          if (parsedData.payload) {
            for (let i = 0; i < parsedData.payload.length; i++) {
              entries.push({
                address: parsedData.payload[i].ip,
                serial: `${i + 1}`,
                group: parsedData.payload[i].model
              })
            }

            const logMsg: any = {
              level: "info",
              msg: JSON.stringify(entries, undefined, 2),
              time: new Date().toString()
            }
            store.dispatch(actions.receiveLogAction(logMsg))
          }
        }
      })
    } catch (e) {
      console.warn("Error parsing JSON", data)
    }
  }
})

export const startScanLogic = createLogic({
  type: actions.startScan.type,
  process({ action }: any) {
    let cmd = "scan"
    if (action.payload && action.payload.subnet) {
      cmd = `scan -i ${action.payload.subnet}/${action.payload.bitmask}`
    }
    ipcRenderer.send("obexec", cmd)
  }
})

export const spawnMDNSLogic = createLogic({
  type: actions.spawnMDNS.type,
  process() {
    let cmd = "mdns"
    ipcRenderer.send("obspawn", cmd)
  }
})

export const startUpdateLogic = createLogic({
  type: actions.startUpgrade.type,
  process({ action }: any) {
    const data = JSON.stringify(action.payload)
    ipcRenderer.send("obupdate", data)
  }
})

export const identifyDeviceLogic = createLogic({
  type: actions.identifyDevice.type,
  process({ action }: any) {
    const data = JSON.stringify(action.payload)
    ipcRenderer.send("obidentify", data)
  }
})
