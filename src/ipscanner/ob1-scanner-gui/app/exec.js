const { exec, spawn } = require("child_process")
const { join, dirname } = require("path")
const appRootDir = require("app-root-dir")
const { app } = require("electron")

const getPlatform = require("./get-platform")

const isProd = process.env.NODE_ENV === "production"
const execPath = isProd
  ? join(process.resourcesPath, "bin")
  : join(appRootDir.get(), "bin", getPlatform())


const cmd = `${join(execPath, "ob1-scanner")}`

function obscannerExec(args, webContents) {
  return new Promise((resolve, reject) => {
    exec(`${cmd} ${args} -j`, (err, stdout, stderr) => {
      if (err) {
        console.error("Error with Obscanner", err)
        reject(err)
      }
      if (stdout) {
        resolve(stdout)
      }
      if (stderr) {
        resolve(stderr)
      }
      webContents.send("obscanner", stdout)
      webContents.send("obscanner", stderr)
    })
  })
}

function obscannerSpawn(args, webContents) {
  const argArr = args.split(" ")
  const child = spawn(cmd, [...argArr, "-j"])
  child.stdout.on("data", data => {
    webContents.send("obscanner", data.toString())
  })
  child.stderr.on("data", data => {
    webContents.send("obscanner", data.toString())
  })
  return child
}

function obscannerSpecialSpawn(args, webContents) {
  const child = spawn(cmd, [...args, "-j"])
  child.stdout.on("data", data => {
    webContents.send("obscanner", data.toString())
  })
  child.stderr.on("data", data => {
    webContents.send("obscanner", data.toString())
  })
  return child
}

module.exports = {
  obscannerExec,
  obscannerSpawn,
  obscannerSpecialSpawn
}
