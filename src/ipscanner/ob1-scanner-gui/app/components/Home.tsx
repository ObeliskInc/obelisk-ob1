import * as _ from "lodash"
import * as React from "react"
import StayScrolled from "react-stay-scrolled"
import Message from "./messages"

import { Tab, Tabs, TabList, TabPanel } from "react-tabs"

import Loader from "./Loader"
import { Link, RouteComponentProps } from "react-router-dom"
import { Log, Miner, BeforeScan, BeforeUpdate } from "../reducers/bridge"
const LogoSVG = require("../assets/svg/logo.svg")
let styles = require("./Home.scss")

const GEN1_LATEST_FIRMWARE_VERSION = "v1.3.0"

export interface IProps extends RouteComponentProps<any> {
  startScan(payload: BeforeScan): void
  spawnMDNS(): void
  startUpgrade(payload: BeforeUpdate): void
  logs: Log[]
  miners: Miner[]
  loading: string
}

function validateIP(ip: string) {
  if (
    /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(
      ip
    )
  ) {
    return true
  }
  return false
}

function compareIp(miner1: Miner, miner2: Miner) {
  const ipParts1 = miner1.ip.split(".")
  const ipParts2 = miner2.ip.split(".")
  for (let i = 0; i < ipParts1.length; i++) {
    const ipPartNum1 = parseInt(ipParts1[i])
    const ipPartNum2 = parseInt(ipParts2[i])

    if (ipPartNum1 !== ipPartNum2) {
      return ipPartNum1 - ipPartNum2
    }
    // else continue to next number
  }
  return 0 // They must be the same
}

function getFirmwareVersion(miner: Miner) {
  let firmwareVersion = miner.firmwareVersion
  if (!firmwareVersion || firmwareVersion.length === 0) {
    firmwareVersion = "v1.0.0"
  } else {
    const parts = firmwareVersion.split(" ")
    if (parts.length === 1) {
      firmwareVersion = parts[0]
    } else if (parts.length >= 2) {
      firmwareVersion = parts[1]
    }
  }
  return firmwareVersion
}

function isSCModel(model: string): boolean {
  return model === "SC1" || model == "SC1 Slim"
}

function isDCRModel(model: string): boolean {
  return model === "DCR1" || model == "DCR1 Slim"
}

function isGen1(model: string): boolean {
  return model === "SC1" || model == "DCR1"
}

function isGen2(model: string): boolean {
  return model === "SC1 Slim" || model == "DCR1 Slim"
}

export default class Home extends React.Component<IProps> {
  state = {
    subnetChecked: false,
    subnet: "",
    bitmask: "",

    sshAuthChecked: false,
    sshuser: "",
    sshpass: "",

    uiAuthChecked: false,
    uiuser: "",
    uipass: "",

    customip: ""
  }

  componentDidMount() {}

  startScan = () => {
    const { subnetChecked, subnet, bitmask, uiuser, uipass } = this.state
    if (subnetChecked && validateIP(subnet) && parseInt(bitmask, 10) != NaN) {
      this.props.startScan({
        subnet,
        bitmask,
        uiuser,
        uipass
      })
    } else {
      this.props.startScan({
        subnet: undefined,
        bitmask: undefined,
        uiuser,
        uipass
      })
    }
  }

  spawnServer = () => {
    this.props.spawnMDNS()
  }

  handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    this.setState({
      [e.target.name]: e.target.value
    })
  }

  onCheck = () => {
    this.setState({
      subnetChecked: !this.state.subnetChecked
    })
  }

  onSSHAuthCheck = () => {
    this.setState({
      sshAuthChecked: !this.state.sshAuthChecked
    })
  }

  onUIAuthCheck = () => {
    this.setState({
      uiAuthChecked: !this.state.uiAuthChecked
    })
  }

  upgradeSingle = (miner: Miner) => () => {
    const ip = miner.ip
    const model = miner.model
    const firmwareVersion = getFirmwareVersion(miner) || "v1.0.0"

    if (miner.isUpgradeInProgress) {
      return
    }

    if (isGen1(model)) {
      let sshuser = "root"
      let sshpass = "obelisk"

      if (this.state.sshAuthChecked) {
        sshuser = this.state.sshuser
        sshpass = this.state.sshpass

        // Some users click the checkbox, but then don't enter credentials - fallback to defaults if so
        if (!sshuser || sshuser.length === 0) {
          sshuser = "root"
        }

        if (!sshpass || sshpass.length === 0) {
          sshpass = "obelisk"
        }
      }

      this.props.startUpgrade({
        host: ip,
        sshuser,
        sshpass,
        model,
        uiuser: "",
        uipass: ""
      })
    } else {
      let uiuser = "admin"
      let uipass = "admin"

      if (this.state.uiAuthChecked) {
        uiuser = this.state.uiuser
        uipass = this.state.uipass

        // Some users click the checkbox, but then don't enter credentials - fallback to defaults if so
        if (!uiuser || uiuser.length === 0) {
          uiuser = "admin"
        }

        if (!uipass || uipass.length === 0) {
          uipass = "admin"
        }
      }

      this.props.startUpgrade({
        host: ip,
        sshuser: "",
        sshpass: "",
        model,
        uiuser,
        uipass
      })
    }
  }

  upgradeAll = () => {
    console.log("upgradeAll() called")
    this.props.miners.forEach(miner => {
      const firmwareVersion = getFirmwareVersion(miner)
      const upgradable = firmwareVersion !== GEN1_LATEST_FIRMWARE_VERSION
      if (upgradable) {
        this.upgradeSingle(miner)()
      }
    })
  }

  // ONLY FOR GEN 1!!!!
  startManualUpdate = () => {
    const { customip } = this.state
    console.log("custom", customip, validateIP(customip))
    if (validateIP(customip)) {
      const miner: Miner = {
        ip: customip,
        model: "SC1", // This just gets it into the Gen 1 path - works for both SC1 and DCR1
        firmwareVersion: "v1.0.0",
        mac: ""
      }
      this.upgradeSingle(miner)()
    }
  }

  render() {
    const mappedLogs = this.props.logs.map((l, i) => {
      return (
        <Message
          key={i}
          text={
            <pre key={i}>
              <strong>{l.level}</strong> {l.msg} {l.time}
            </pre>
          }
        />
      )
    })

    const upgradeButton = (miner: Miner) => {
      if (isGen1(miner.model)) {
        const firmwareVersion = getFirmwareVersion(miner)
        if (firmwareVersion === GEN1_LATEST_FIRMWARE_VERSION) {
          return undefined
        }

        if (miner.isUpgradeInProgress) {
          return <span>Upgrading to {GEN1_LATEST_FIRMWARE_VERSION}...</span>
        }

        return (
          <button
            onClick={this.upgradeSingle(miner)}
            className={styles.upgradeButton}
          >
            UPGRADE TO {GEN1_LATEST_FIRMWARE_VERSION}
          </button>
        )
      } else {
        const availVersion = miner.firmwareUpdate
        if (!availVersion) {
          return undefined
        }

        if (miner.isUpgradeInProgress) {
          return <span>Upgrading to {availVersion}...</span>
        }

        return (
          <button
            onClick={this.upgradeSingle(miner)}
            className={styles.upgradeButton}
          >
            UPGRADE TO {availVersion}
          </button>
        )
      }
    }

    console.log("isArray(this.props.miners) = " + _.isArray(this.props.miners))

    const sortedMiners = this.props.miners.sort(compareIp)
    console.log("isArray(sortedMiners) = " + _.isArray(sortedMiners))
    const sc1Count = sortedMiners.reduce((count: number, miner: Miner) => {
      return count + (isSCModel(miner.model) ? 1 : 0)
    }, 0)
    const dcr1Count = sortedMiners.reduce((count: number, miner: Miner) => {
      return count + (isDCRModel(miner.model) ? 1 : 0)
    }, 0)
    const otherCount = sortedMiners.length - sc1Count - dcr1Count

    let upgradableMinersExist = false
    let mappedMiners = sortedMiners.map((miner, i) => {
      const firmwareVersion = getFirmwareVersion(miner)
      const upgradable = true
      // firmwareVersion === "v1.0.0" || firmwareVersion === "v1.1.0"
      if (upgradable) {
        upgradableMinersExist = true
      }

      return (
        <tr key={i}>
          <td>
            <a href={`http://${miner.ip}`} target="_blank">
              {miner.ip}
            </a>
          </td>
          <td>{miner.mac}</td>
          <td>{miner.model}</td>
          <td>{firmwareVersion}</td>
          <td>{upgradable ? upgradeButton(miner) : undefined}</td>
        </tr>
      )
    })

    const renderSSHAuth = (
      <div>
        <div className={styles.infoMessage}>
          If you have modified the default SSH login credentials, ensure that
          you enter the new username and password below.
        </div>

        <input
          type="checkbox"
          name="override"
          value="ssh"
          checked={this.state.sshAuthChecked}
          onChange={this.onSSHAuthCheck}
          className={styles.indent}
        />
        <span className={styles.checkboxLabel}>
          Custom SSH Login (Default: root/obelisk)
        </span>
        <div
          className={`${styles.input} ${this.state.sshAuthChecked &&
            styles.active}`}
        >
          <input
            type="text"
            placeholder="User (root)"
            value={this.state.sshuser}
            name="sshuser"
            onChange={this.handleChange}
            className={styles.indent}
          />
          <input
            type="text"
            placeholder="Password (obelisk)"
            name="sshpass"
            onChange={this.handleChange}
            value={this.state.sshpass}
            className={styles.indent}
          />
        </div>
      </div>
    )

    const renderUIAuth = (
      <div>
        <div className={styles.infoMessage}>
          If you have modified the default web GUI login credentials, ensure
          that you enter the new username and password below. (Only applies to
          Gen 2 Obelisks)
        </div>
        <input
          type="checkbox"
          name="override"
          value="ssh"
          checked={this.state.uiAuthChecked}
          onChange={this.onUIAuthCheck}
          className={styles.indent}
        />
        <span className={styles.checkboxLabel}>
          Custom GUI Login (Default: admin/admin)
        </span>
        <div
          className={`${styles.input} ${this.state.uiAuthChecked &&
            styles.active}`}
        >
          <input
            type="text"
            name="uiuser"
            placeholder="User (admin)"
            value={this.state.uiuser}
            onChange={this.handleChange}
            className={styles.indent}
          />
          <input
            type="text"
            name="uipass"
            placeholder="Password (admin)"
            value={this.state.uipass}
            onChange={this.handleChange}
            className={styles.indent}
          />
        </div>
      </div>
    )

    let renderLoadingOrResults = (
      <div className={styles.resultsContainer}>
        <table className={styles.table}>
          <tbody>{mappedMiners}</tbody>
        </table>
      </div>
    )

    const upgradeAll = (
      <div className={styles.upgradeAll}>
        <button
          onClick={this.upgradeAll}
          className={styles.upgradeAllButton}
          disabled={!upgradableMinersExist}
        >
          UPGRADE ALL
        </button>
        <div className={styles.minerCounts}>
          <div className={styles.totalCount}>
            {sortedMiners.length} OBELISKS FOUND
          </div>
          <div className={styles.typeCounts}>
            {sc1Count} x SC1, {dcr1Count} x DCR1, {otherCount} x OTHER
          </div>
        </div>
      </div>
    )

    switch (this.props.loading) {
      case "started":
        renderLoadingOrResults = (
          <div className={styles.loading}>
            <Loader />
          </div>
        )
        break

      case "finished":
        if (this.props.miners.length < 1) {
          renderLoadingOrResults = (
            <div className="notFound">
              <div>
                <div className="notFoundSVG">
                  <object data={LogoSVG} type="image/svg+xml" />
                </div>
                <h3>No Obelisks Found.</h3>
                <span>
                  It's possible our subnet detector used the wrong subnet to
                  scan, or it didn't scan enough IP addresses. Try modifying the
                  subnet range or changing the subnet.
                </span>
              </div>
            </div>
          )
        }
        break
    }

    return (
      <div className={styles.container} data-tid="container">
        <div className={styles.left}>
          <h2>Obelisk Scanner</h2>
          <span className={styles.subheading}>
            Find & Upgrade Obelisk Machines on your subnet
          </span>
          <Tabs selectedTabClassName={styles.tabactive}>
            <TabList className={styles.tablist}>
              <Tab>SCANNER</Tab>
              <Tab>FORCED UPGRADE</Tab>
            </TabList>
            <div className={styles.tabwrap}>
              <TabPanel>
                <div className={styles.buttonsRow}>
                  <button onClick={this.startScan} className={styles.button}>
                    START SMART SCAN
                  </button>
                  <button onClick={this.spawnServer} className={styles.button2}>
                    IP REPORTER (mDNS)
                  </button>
                </div>
                <div className={styles.settings}>
                  <div className={styles.infoMessage}>
                    If your network is not detected automatically or you are
                    connecting over a VPN, click the Enable Manual Subnet
                    Override checkbox and enter your base network address and the
                    subnet mask.
                  </div>
                  <input
                    type="checkbox"
                    name="override"
                    value="subnet"
                    checked={this.state.subnetChecked}
                    onChange={this.onCheck}
                    className={styles.indent}
                  />

                  <span className={styles.checkboxLabel}>
                    Enable Manual Subnet Override
                  </span>
                  <div
                    className={`${styles.input} ${this.state.subnetChecked &&
                      styles.active}`}
                  >
                    <input
                      type="text"
                      placeholder="Subnet (192.168.0.1)"
                      value={this.state.subnet}
                      name="subnet"
                      onChange={this.handleChange}
                      className={styles.indent}
                    />
                    <input
                      type="text"
                      placeholder="Bitmask (24)"
                      name="bitmask"
                      onChange={this.handleChange}
                      value={this.state.bitmask}
                      className={styles.indent}
                    />
                  </div>
                  {renderSSHAuth}
                  {renderUIAuth}
                </div>
              </TabPanel>
              <TabPanel>
                <div className={styles.manual}>
                  <div className={styles.infoMessage}>
                    If your Gen 1 Obelisk does not appear in the Scanner, or you
                    want to reinstall the latest firmware version, you can
                    perform a Forced Upgrade by entering the miner's IP address
                    below, and clicking the FORCED UPGRADE button.
                  </div>
                  <div className={styles.manualWarning}>
                    NOTE: FORCED UPGRADE IS FOR GEN 1 OBELISKS ONLY!
                  </div>
                  <input
                    type="text"
                    placeholder="Gen 1 Obelisk IP"
                    name="customip"
                    onChange={this.handleChange}
                    value={this.state.customip}
                  />
                </div>
                {renderSSHAuth}
                <button
                  onClick={this.startManualUpdate}
                  className={styles.button}
                >
                  FORCED UPGRADE TO {GEN1_LATEST_FIRMWARE_VERSION}
                </button>
              </TabPanel>
            </div>
          </Tabs>
          <div className={styles.terminal}>
            <span>
              <strong>Logging</strong>
            </span>

            <StayScrolled className={styles.terminalbox} component="div">
              {mappedLogs}
            </StayScrolled>
            {/* <div className={styles.terminalbox}>{mappedLogs}</div> */}
          </div>
        </div>
        <div className={styles.right}>
          <div className={styles.card}>
            <table className={styles.tableHeader}>
              <thead>
                <tr>
                  <th>IP ADDRESS</th>
                  <th>MAC</th>
                  <th>MODEL</th>
                  <th>CURR. VERSION</th>
                  <th>ACTION</th>
                </tr>
              </thead>
            </table>

            {renderLoadingOrResults}
            {upgradeAll}
          </div>
        </div>
      </div>
    )
  }
}
