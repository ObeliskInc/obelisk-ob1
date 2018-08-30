import * as React from 'react'
import Loader from './Loader'
import { Link, RouteComponentProps } from 'react-router-dom'
import { ipcRenderer } from 'electron'
import { receiveLogAction, spawnMDNS } from '../actions/bridge'
// import { receivelog } from '../actions'
import { Log, Miner, BeforeScan } from '../reducers/bridge'
const LogoSVG = require('../assets/svg/logo.svg')
let styles = require('./Home.scss')

export interface IProps extends RouteComponentProps<any> {
  startScan(payload: BeforeScan): void
  spawnMDNS(): void
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

export default class Home extends React.Component<IProps> {
  state = {
    checked: false,
    subnet: '',
    bitmask: '',
  }
  componentDidMount() {}
  startScan = () => {
    const { checked, subnet, bitmask } = this.state
    if (checked && validateIP(subnet) && parseInt(bitmask, 10) != NaN) {
      this.props.startScan({
        subnet,
        bitmask,
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
      [e.target.name]: e.target.value,
    })
  }
  onCheck = () => {
    this.setState({
      checked: !this.state.checked,
    })
  }
  render() {
    const mappedLogs = this.props.logs.map((l, i) => {
      return (
        <pre key={i}>
          <strong>{l.level}</strong> {l.msg} {l.time}
        </pre>
      )
    })
    const mappedMiners = this.props.miners.map((m, i) => {
      return (
        <tr key={i}>
          <td>{m.ip}</td>
          <td>{m.mac}</td>
          <td>{m.model}</td>
        </tr>
      )
    })
    let renderLoadingOrResults = (
      <table className={styles.table}>
        <thead>
          <tr>
            <th>IP Address</th>
            <th>MAC</th>
            <th>Model</th>
          </tr>
        </thead>
        <tbody>{mappedMiners}</tbody>
      </table>
    )
    switch (this.props.loading) {
      case 'started':
        renderLoadingOrResults = (
          <div className={styles.loading}>
            <Loader />
          </div>
        )
        break
      case 'finished':
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
                  scan, or it didn't scan enough ips. Try modifying the subnet
                  range or changing the subnet.
                </span>
              </div>
            </div>
          )
        }
        break
    }

    return (
      <div>
        <div className={styles.container} data-tid="container">
          <div className={styles.left}>
            <h2>Obelisk Scanner</h2>
            <span className={styles.subheading}>
              Find Obelisk Machines on your subnet.
            </span>
            <div>
              <button onClick={this.startScan} className={styles.button}>
                Start Smart Scan
              </button>
              <button onClick={this.spawnServer} className={styles.button2}>
                IP Reporter mDNS
              </button>
            </div>
            <div className={styles.settings}>
              <input
                type="checkbox"
                name="override"
                value="subnet"
                checked={this.state.checked}
                onChange={this.onCheck}
              />
              <span>Enable Manual Subnet Override (For Colos)</span>
              <div
                className={`${styles.input} ${this.state.checked &&
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
            </div>
            <div className={styles.terminal}>
              <span>
                <strong>Logging</strong>
              </span>
              <div className={styles.terminalbox}>{mappedLogs}</div>
            </div>
          </div>
          <div>
            <div className={styles.card}>{renderLoadingOrResults}</div>
          </div>
        </div>
      </div>
    )
  }
}
