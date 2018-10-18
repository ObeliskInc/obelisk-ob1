import * as React from "react"
import StayScrolled from "react-stay-scrolled"
import Message from "./messages"

import { Tab, Tabs, TabList, TabPanel } from "react-tabs"

import Loader from "./Loader"
import { Link, RouteComponentProps } from "react-router-dom"
import { Log, Miner, BeforeScan, BeforeUpdate } from "../reducers/bridge"
const LogoSVG = require("../assets/svg/logo.svg")
let styles = require("./Home.scss")

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
  const ipParts1 = miner1.ip.split(".");
  const ipParts2 = miner2.ip.split(".");
  for (let i=0; i<ipParts1.length; i++) {
    const ipPartNum1 = parseInt(ipParts1[i])
    const ipPartNum2 = parseInt(ipParts2[i])

    if (ipPartNum1 !== ipPartNum2) {
      return ipPartNum1 - ipPartNum2
    }
    // else continue to next number
  }
  return 0;  // They must be the same
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

export default class Home extends React.Component<IProps> {
  state = {
    subnetChecked: false,
    sshChecked: false,
    subnet: "",
    bitmask: "",
    sshuser: "",
    sshpass: "",
    customip: ""
  }

  componentDidMount() {}

  startScan = () => {
    const { subnetChecked, subnet, bitmask } = this.state
    if (subnetChecked && validateIP(subnet) && parseInt(bitmask, 10) != NaN) {
      this.props.startScan({
        subnet,
        bitmask
      })
    } else {
      this.props.startScan({})
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

  onSSHCheck = () => {
    this.setState({
      sshChecked: !this.state.sshChecked
    })
  }

  upgradeSingle = (ip: string, model: string) => () => {
    let sshuser = "root";
    let sshpass = "obelisk";

    if (this.state.sshChecked) {
      sshuser = this.state.sshuser
      sshpass = this.state.sshpass

      // Some users click the checkbox, but then don't enter credentials - fallback to defaults if so
      if (!sshuser || sshuser.length === 0) {
        sshuser = "root"
      }

      if (!sshpass || sshpass.length === 0) {
        sshuser = "obelisk"
      }
    }

    this.props.startUpgrade({
      host: ip,
      sshuser,
      sshpass,
      model
    })
  }

  upgradeAll = () => {
    this.props.miners.forEach(m => {
      const firmwareVersion = getFirmwareVersion(m)
      const upgradable = firmwareVersion === "v1.0.0" || firmwareVersion === "v1.1.0"
      if (upgradable) {
        this.upgradeSingle(m.ip, m.model)()
      }
    })
  }

  startManualUpdate = () => {
    const { customip } = this.state
    console.log("custom", customip, validateIP(customip))
    if (validateIP(customip)) {
      this.upgradeSingle(customip, "deprecated")()
    }
  }

  render() {
    const newVersion = "v1.2.0"
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

    const upgradeButton = (m: Miner) => (
      <button
        onClick={this.upgradeSingle(m.ip, m.model)}
        className={styles.upgradeButton}
      >
        UPGRADE TO {newVersion}
      </button>
    )

    const sortedMiners = this.props.miners.sort(compareIp);
    const sc1Count = sortedMiners.reduce((count: number, miner: Miner) => {return count + ((miner.model === "SC1") ? 1 : 0)}, 0)
    const dcr1Count = sortedMiners.reduce((count: number, miner: Miner) => {return count + ((miner.model === "DCR1") ? 1 : 0)}, 0)
    const otherCount = sortedMiners.length - sc1Count - dcr1Count
    let upgradableMinersExist = false
    let mappedMiners = sortedMiners.map((m, i) => {
      const firmwareVersion = getFirmwareVersion(m)
      const upgradable = firmwareVersion === "v1.0.0" || firmwareVersion === "v1.1.0"
      if (upgradable) {
        upgradableMinersExist = true
      }

      return (
        <tr key={i}>
          <td><a href={`http://${m.ip}`} target="_blank">{m.ip}</a></td>
          <td>{m.mac}</td>
          <td>{m.model}</td>
          <td>{firmwareVersion}</td>
          <td>
              {upgradable ? upgradeButton(m) :  undefined}
          </td>
        </tr>
      )
    })
    const renderSSH = (
      <div>
        <input
          type="checkbox"
          name="override"
          value="ssh"
          checked={this.state.sshChecked}
          onChange={this.onSSHCheck}
        />
        <span>Custom SSH Login (Default: root/obelisk)</span>
        <div
          className={`${styles.input} ${this.state.sshChecked &&
            styles.active}`}
        >
          <input
            type="text"
            placeholder="User (root)"
            value={this.state.sshuser}
            name="sshuser"
            onChange={this.handleChange}
          />
          <input
            type="text"
            placeholder="Password (obelisk)"
            name="sshpass"
            onChange={this.handleChange}
            value={this.state.sshpass}
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
          <button onClick={this.upgradeAll} className={styles.upgradeAllButton} disabled={!upgradableMinersExist}>
            UPGRADE ALL TO {newVersion}
          </button>
          <div className={styles.minerCounts}>
            <div className={styles.totalCount}>{sortedMiners.length} OBELISKS FOUND</div>
            <div className={styles.typeCounts}>{sc1Count} x SC1, {dcr1Count} x DCR1, {otherCount} x OTHER</div>
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
                  scan, or it didn't scan enough IP addresses. Try modifying the subnet
                  range or changing the subnet.
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
              <Tab>MANUAL IP ENTRY</Tab>
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
                  <input
                    type="checkbox"
                    name="override"
                    value="subnet"
                    checked={this.state.subnetChecked}
                    onChange={this.onCheck}
                  />
                  <span>Enable Manual Subnet Override</span>
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
                    />
                    <input
                      type="text"
                      placeholder="Bitmask (24)"
                      name="bitmask"
                      onChange={this.handleChange}
                      value={this.state.bitmask}
                    />
                  </div>
                  {renderSSH}
                </div>
              </TabPanel>
              <TabPanel>
                <div className={styles.manual}>
                  <input
                    type="text"
                    placeholder="Obelisk IP"
                    name="customip"
                    onChange={this.handleChange}
                    value={this.state.customip}
                  />
                </div>
                {renderSSH}
                <button
                  onClick={this.startManualUpdate}
                  className={styles.button}
                >
                  UPGRADE TO {newVersion}
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
