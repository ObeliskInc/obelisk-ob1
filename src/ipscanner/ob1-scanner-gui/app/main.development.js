const { app, BrowserWindow, Menu, shell, ipcMain } = require("electron")
const obscanner = require("./exec")

let menu
let template
let mainWindow = null
const appRootDir = require("app-root-dir")
const { join, dirname } = require("path")
// var log = require('electron-log')

// require("electron-debug")({ enabled: true, showDevTools: true }) // eslint-disable-line global-require

if (process.env.NODE_ENV === "production") {
  const sourceMapSupport = require("source-map-support") // eslint-disable-line
  sourceMapSupport.install()
}

if (process.env.NODE_ENV === "development") {
  require("electron-debug")() // eslint-disable-line global-require
  const path = require("path") // eslint-disable-line
  const p = path.join(__dirname, "..", "app", "node_modules") // eslint-disable-line
  require("module").globalPaths.push(p) // eslint-disable-line
}

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit()
})

const installExtensions = () => {
  if (process.env.NODE_ENV === "development") {
    const installer = require("electron-devtools-installer") // eslint-disable-line global-require

    const extensions = ["REACT_DEVELOPER_TOOLS", "REDUX_DEVTOOLS"]
    const forceDownload = !!process.env.UPGRADE_EXTENSIONS
    return Promise.all(
      extensions.map(name => installer.default(installer[name], forceDownload))
    )
  }

  return Promise.resolve([])
}

app.on("ready", () =>
  installExtensions().then(() => {
    mainWindow = new BrowserWindow({
      show: false,
      width: 1424,
      height: 728
    })

    mainWindow.loadURL(`file://${__dirname}/app.html`)

    mainWindow.webContents.on("did-finish-load", () => {
      mainWindow.show()
      mainWindow.focus()
    })

    mainWindow.on("closed", () => {
      mainWindow = null
    })

    // Setup Obscanner
    let currMDNSProc = null
    let currScanProc = null
    ipcMain.on("obexec", (event, data) => {
      if (currMDNSProc) {
        currMDNSProc.kill("SIGINT")
      }
      if (currScanProc) {
        currScanProc.kill("SIGINT")
      }
      currScanProc = obscanner.obscannerSpawn(data, mainWindow.webContents)
    })
    ipcMain.on("obspawn", (event, data) => {
      if (currMDNSProc) {
        currMDNSProc.kill("SIGINT")
      }
      if (currScanProc) {
        currScanProc.kill("SIGINT")
      }
      currMDNSProc = obscanner.obscannerSpawn(data, mainWindow.webContents)
    })

    ipcMain.on("obupdate", (event, data) => {
      if (currMDNSProc) {
        currMDNSProc.kill("SIGINT")
      }
      if (currScanProc) {
        currScanProc.kill("SIGINT")
      }
      const parseData = JSON.parse(data)
      const { sshuser, sshpass, uiuser, uipass, host, model } = parseData

      if (model === "SC1" || model === "DCR1") {
        // Gen 1
        const isProd = process.env.NODE_ENV === "production"
        const execPath = isProd
          ? join(process.resourcesPath, "firmware")
          : join(appRootDir.get(), "bin", "firmware")
        const detectpath = join(execPath, "detect")
        const scpath = join(execPath, "sc1-v1.3.0.tar.gz")
        const dcrpath = join(execPath, "dcr1-v1.3.0.tar.gz")

        const cmd = [
          "upgrade-gen1",
          "-z",
          detectpath,
          "-s",
          scpath,
          "-d",
          dcrpath,
          "-u",
          sshuser,
          "-p",
          sshpass,
          "-i",
          host
        ]
        // This should run until the end
        obscanner.obscannerSpecialSpawn(cmd, mainWindow.webContents)
      } else {
        // Gen 2
        const cmd = ["upgrade-gen2", "-i", host, "-u", uiuser, "-p", uipass]
        // This should run until the end
        obscanner.obscannerSpecialSpawn(cmd, mainWindow.webContents)
      }
    })

    if (process.env.NODE_ENV === "development") {
      mainWindow.openDevTools()
      mainWindow.webContents.on("context-menu", (e, props) => {
        const { x, y } = props

        Menu.buildFromTemplate([
          {
            label: "Inspect element",
            click() {
              mainWindow.inspectElement(x, y)
            }
          }
        ]).popup(mainWindow)
      })
    }

    if (process.platform === "darwin") {
      template = [
        {
          label: "Obelisk Scanner",
          submenu: [
            {
              label: "About Obelisk Scanner",
              selector: "orderFrontStandardAboutPanel:"
            },
            {
              type: "separator"
            },
            {
              label: "Services",
              submenu: []
            },
            {
              type: "separator"
            },
            {
              label: "Hide Obelisk Scanner",
              accelerator: "Command+H",
              selector: "hide:"
            },
            {
              label: "Hide Others",
              accelerator: "Command+Shift+H",
              selector: "hideOtherApplications:"
            },
            {
              label: "Show All",
              selector: "unhideAllApplications:"
            },
            {
              type: "separator"
            },
            {
              label: "Quit",
              accelerator: "Command+Q",
              click() {
                app.quit()
              }
            }
          ]
        },
        {
          label: "Edit",
          submenu: [
            {
              label: "Undo",
              accelerator: "Command+Z",
              selector: "undo:"
            },
            {
              label: "Redo",
              accelerator: "Shift+Command+Z",
              selector: "redo:"
            },
            {
              type: "separator"
            },
            {
              label: "Cut",
              accelerator: "Command+X",
              selector: "cut:"
            },
            {
              label: "Copy",
              accelerator: "Command+C",
              selector: "copy:"
            },
            {
              label: "Paste",
              accelerator: "Command+V",
              selector: "paste:"
            },
            {
              label: "Select All",
              accelerator: "Command+A",
              selector: "selectAll:"
            }
          ]
        },
        {
          label: "View",
          submenu:
            process.env.NODE_ENV === "development"
              ? [
                  {
                    label: "Reload",
                    accelerator: "Command+R",
                    click() {
                      mainWindow.webContents.reload()
                    }
                  },
                  {
                    label: "Toggle Full Screen",
                    accelerator: "Ctrl+Command+F",
                    click() {
                      mainWindow.setFullScreen(!mainWindow.isFullScreen())
                    }
                  },
                  {
                    label: "Toggle Developer Tools",
                    accelerator: "Alt+Command+I",
                    click() {
                      mainWindow.toggleDevTools()
                    }
                  }
                ]
              : [
                  {
                    label: "Toggle Full Screen",
                    accelerator: "Ctrl+Command+F",
                    click() {
                      mainWindow.setFullScreen(!mainWindow.isFullScreen())
                    }
                  }
                ]
        },
        {
          label: "Window",
          submenu: [
            {
              label: "Minimize",
              accelerator: "Command+M",
              selector: "performMiniaturize:"
            },
            {
              label: "Close",
              accelerator: "Command+W",
              selector: "performClose:"
            },
            {
              type: "separator"
            },
            {
              label: "Bring All to Front",
              selector: "arrangeInFront:"
            }
          ]
        },
        {
          label: "Help",
          submenu: [
            {
              label: "Learn More",
              click() {
                shell.openExternal("http://obelisk.tech")
              }
            },
            {
              label: "Support",
              click() {
                shell.openExternal("http://support.obelisk.tech/")
              }
            }
          ]
        }
      ]

      menu = Menu.buildFromTemplate(template)
      Menu.setApplicationMenu(menu)
    } else {
      template = [
        {
          label: "&File",
          submenu: [
            {
              label: "&Open",
              accelerator: "Ctrl+O"
            },
            {
              label: "&Close",
              accelerator: "Ctrl+W",
              click() {
                mainWindow.close()
              }
            }
          ]
        },
        {
          label: "&View",
          submenu:
            process.env.NODE_ENV === "development"
              ? [
                  {
                    label: "&Reload",
                    accelerator: "Ctrl+R",
                    click() {
                      mainWindow.webContents.reload()
                    }
                  },
                  {
                    label: "Toggle &Full Screen",
                    accelerator: "F11",
                    click() {
                      mainWindow.setFullScreen(!mainWindow.isFullScreen())
                    }
                  },
                  {
                    label: "Toggle &Developer Tools",
                    accelerator: "Alt+Ctrl+I",
                    click() {
                      mainWindow.toggleDevTools()
                    }
                  }
                ]
              : [
                  {
                    label: "Toggle &Full Screen",
                    accelerator: "F11",
                    click() {
                      mainWindow.setFullScreen(!mainWindow.isFullScreen())
                    }
                  }
                ]
        },
        {
          label: "Help",
          submenu: [
            {
              label: "Learn More",
              click() {
                shell.openExternal("http://obelisk.tech")
              }
            },
            {
              label: "Support",
              click() {
                shell.openExternal("http://support.obelisk.tech/")
              }
            }
          ]
        }
      ]
      menu = Menu.buildFromTemplate(template)
      mainWindow.setMenu(menu)
    }
  })
)
