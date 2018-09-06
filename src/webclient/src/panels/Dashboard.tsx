// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import _ = require('lodash')
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import {
  Area,
  CartesianGrid,
  ComposedChart,
  Line,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { Header, Label, Table } from 'semantic-ui-react'

import Content from 'components/Content'
import Stat from 'components/Stat'
import { fetchDashboardStatus } from 'modules/Main/actions'
import { getDashboardStatus } from 'modules/Main/selectors'
import { DashboardStatus, HashboardStatus, PoolStatus, StatsEntry, HashrateEntry } from 'modules/Main/types'
import { formatTime } from 'utils'

interface ConnectProps {
  dashboardStatus: DashboardStatus
}

type CombinedProps = ConnectProps & InjectedProps & DispatchProp<any>

const chartColors = [
  '#FF6060', // board1
  '#FF4040', // board2
  '#FF2020', // board3
]

class Dashboard extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    table: {
      $debugName: 'table',
      fontFamily: 'Karbon Regular',
      tableLayout: 'fixed',
    },
  }

  timer: any

  refreshData = () => {
    const { dispatch } = this.props
    if (dispatch) {
      dispatch(fetchDashboardStatus.started({}))
    }
  }

  componentWillMount() {
    this.timer = setInterval(this.refreshData, 5000)
    this.refreshData()
  }

  componentWillUnmount() {
    if (this.timer) {
      clearInterval(this.timer)
    }
  }

  render() {
    const { classNames, dashboardStatus } = this.props

    const systemStats = _.map(dashboardStatus.systemInfo, (info: StatsEntry) => (
      <Stat label={info.name} value={info.value} key={info.name} />
    ))

    const mapPoolHeaders = dashboardStatus.poolStatus.map((_, i) => {
      return (
        <Table.HeaderCell key={i} textAlign="center">
          POOL {i + 1}
        </Table.HeaderCell>
      )
    })
    const mapBoardHeaders = dashboardStatus.hashboardStatus.map((_, i) => {
      return (
        <Table.HeaderCell key={i} textAlign="center">
          BOARD {i + 1}
        </Table.HeaderCell>
      )
    })

    const hashboardTableMap = {
      STATUS: (s: HashboardStatus[]) =>
        _.map(s, (h, i) => (
          <Table.Cell key={i} textAlign="center">
            <Label color={h.status === 'Alive' ? 'green' : 'red'} horizontal={true}>
              {h.status}
            </Label>
          </Table.Cell>
        )),
      'ACCEPTS/REJECTS': (s: HashboardStatus[]) =>
        _.map(s, (h: HashboardStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.accepted}/{h.rejected}
          </Table.Cell>
        )),
      'BOARD TEMP': (s: HashboardStatus[]) =>
        _.map(s, (h: HashboardStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.boardTemp} C
          </Table.Cell>
        )),
      'CHIP TEMP.': (s: HashboardStatus[]) =>
        _.map(s, (h: HashboardStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.chipTemp} C
          </Table.Cell>
        )),
      'POWER SUPPLY TEMP.': (s: HashboardStatus[]) =>
        _.map(s, (h: HashboardStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.powerSupplyTemp} C
          </Table.Cell>
        )),
      'HASHRATE: AVG.': (s: HashboardStatus[]) =>
        _.map(s, (h, i) => (
          <Table.Cell key={i} textAlign="center">
            {Number(h.mhsAvg / 1000).toFixed(1)} GH/s
          </Table.Cell>
        )),
      'HASHRATE: 1 MIN.': (s: HashboardStatus[]) =>
        _.map(s, (h, i) => (
          <Table.Cell key={i} textAlign="center">
            {Number(h.mhs1m / 1000).toFixed(1)} GH/s
          </Table.Cell>
        )),
      'HASHRATE: 5 MIN.': (s: HashboardStatus[]) =>
        _.map(s, (h, i) => (
          <Table.Cell key={i} textAlign="center">
            {Number(h.mhs5m / 1000).toFixed(1)} GH/s
          </Table.Cell>
        )),
      'HASHRATE: 15 MIN.': (s: HashboardStatus[]) =>
        _.map(s, (h, i) => (
          <Table.Cell key={i} textAlign="center">
            {Number(h.mhs15m / 1000).toFixed(1)} GH/s
          </Table.Cell>
        )),
    }

    const poolTableMap = {
      URL: (s: PoolStatus[], i: number) => <Table.Cell textAlign="center">{s[i].url}</Table.Cell>,
      WORKER: (s: PoolStatus[], i: number) => (
        <Table.Cell textAlign="center">{s[i].worker}</Table.Cell>
      ),
      STATUS: (s: PoolStatus[], i: number) => (
        <Table.Cell textAlign="center">
          <Label color={s[i].status === 'Alive' ? 'green' : 'red'} horizontal={true}>
            {s[i].status}
          </Label>
        </Table.Cell>
      ),
      'ACCEPTS/REJECTS': (s: PoolStatus[], i: number) => (
        <Table.Cell textAlign="center">
          {s[i].accepted}/{s[i].rejected}
        </Table.Cell>
      ),
    }

    const mapPoolField = (i: number) =>
      dashboardStatus.poolStatus.length > 0 &&
      _.map(Object.keys(poolTableMap), (name: string, index: number) => {
        return (
          <Table.Row key={index}>
            <Table.Cell>{name}</Table.Cell>
            {poolTableMap[name](dashboardStatus.poolStatus, i)}
          </Table.Row>
        )
      })

    const mapBoardCols =
      dashboardStatus.hashboardStatus.length > 0 &&
      _.map(Object.keys(hashboardTableMap), (name: string, index: number) => {
        return (
          <Table.Row key={index}>
            <Table.Cell>{name}</Table.Cell>
            {hashboardTableMap[name](dashboardStatus.hashboardStatus)}
          </Table.Row>
        )
      })

    // Add lines based on how many board entries are in the data
    // Filter entries with no board data, because sometimes cgminer is not ready when apiserver polls (e.g., when a pool is down).
    let areas = undefined
    const hashrateEntries = _.filter(dashboardStatus.hashrateData, (entry: HashrateEntry) => entry.board0 !== undefined)
    if (hashrateEntries.length > 0) {
      const firstEntry = hashrateEntries[0]
      let keys = _.keys(firstEntry)
      console.log('keys before remove =' + keys)
      keys = _.remove(keys, (s: string) => s !== 'time' && s !== 'total')
      console.log('keys after remove  =' + keys)
      areas = _.map(keys, (key: string, index: number) => (
        <Area
          type="monotone"
          dataKey={key}
          stroke="#FF0000"
          fill={chartColors[index]}
          stackId="1"
          key={index}
        />
      ))
    }

    const renderPools =
      dashboardStatus.poolStatus.length > 0 &&
      dashboardStatus.poolStatus.map((_, i) => (
        <Table definition={true} striped={true} unstackable={true} className={classNames.table} key={i}>
          <Table.Header>
            <Table.Row>
              <Table.HeaderCell width={4} />
              {mapPoolHeaders[i]}
            </Table.Row>
          </Table.Header>
          <Table.Body>{mapPoolField(i)}</Table.Body>
        </Table>
      ))

    return (
      <Content>
        <Header as="h1">Dashboard</Header>
        <Header as="h2">Hashrate</Header>
        <ResponsiveContainer width="100%" height={300}>
          <ComposedChart
            data={hashrateEntries}
            margin={{ top: 5, right: 20, bottom: 5, left: 0 }}
          >
            <Line type="monotone" dataKey="total" stroke="#FF0000" />
            {areas}
            <CartesianGrid stroke="#ccc" strokeDasharray="5 5" />
            <XAxis
              dataKey="time"
              tick={{ fill: 'white', stroke: 'white' }}
              domain={['auto', 'auto']}
              tickFormatter={formatTime}
              scale="time"
              type="number"
            />
            <YAxis tick={{ fill: 'white', stroke: 'white' }} />
            <Tooltip
              isAnimationActive={false}
              wrapperStyle={{ background: '#202020' }}
              labelFormatter={formatTime}
            />
          </ComposedChart>
        </ResponsiveContainer>

        <Header as="h2">Pool Info</Header>
        {renderPools}

        <Header as="h2">Hashboard Info</Header>
        <Table definition={true} striped={true} unstackable={true} className={classNames.table}>
          <Table.Header>
            <Table.Row>
              <Table.HeaderCell width={4} />
              {mapBoardHeaders}
            </Table.Row>
          </Table.Header>
          <Table.Body>{mapBoardCols}</Table.Body>
        </Table>

        <Header as="h2">System Info</Header>
        <Table definition={true} striped={true} unstackable={true} className={classNames.table}>
          <Table.Body>{systemStats}</Table.Body>
        </Table>
      </Content>
    )
  }
}

const mapStateToProps = (state: any, props: any): ConnectProps => ({
  dashboardStatus: getDashboardStatus(state.Main),
})

const dashboard = withStyles()<any>(Dashboard)

export default connect<ConnectProps, any, any>(mapStateToProps)(dashboard)
