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
import { DashboardStatus, HashboardStatus, PoolStatus, StatsEntry } from 'modules/Main/types'
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
          Pool {i + 1}
        </Table.HeaderCell>
      )
    })
    const mapBoardHeaders = dashboardStatus.hashboardStatus.map((_, i) => {
      return (
        <Table.HeaderCell key={i} textAlign="center">
          Board {i + 1}
        </Table.HeaderCell>
      )
    })

    const hashboardTableMap = {
      Hashrate: (s: HashboardStatus[]) =>
        _.map(s, (h, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.hashrate} GH/s
          </Table.Cell>
        )),
      Active: (s: HashboardStatus[]) =>
        _.map(s, (h, i) => (
          <Table.Cell key={i} textAlign="center">
            <Label color={h.status === 'Active' ? 'green' : 'red'} horizontal={true}>
              {h.status}
            </Label>
          </Table.Cell>
        )),
      ['Accepts/Rejects']: (s: HashboardStatus[]) =>
        _.map(s, (h: HashboardStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.accepted}/{h.rejected}
          </Table.Cell>
        )),
      ['Board Temp']: (s: HashboardStatus[]) =>
        _.map(s, (h: HashboardStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.boardTemp} C
          </Table.Cell>
        )),
      ['Chip Temp']: (s: HashboardStatus[]) =>
        _.map(s, (h: HashboardStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {h.chipTemp} C
          </Table.Cell>
        )),
    }

    const poolTableMap = {
      Url: (s: PoolStatus[]) =>
        _.map(s, (p: PoolStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {p.url}
          </Table.Cell>
        )),
      Worker: (s: PoolStatus[]) =>
        _.map(s, (p: PoolStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {p.worker}
          </Table.Cell>
        )),
      Status: (s: PoolStatus[]) =>
        _.map(s, (p: PoolStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            <Label color={p.status === 'Active' ? 'green' : 'red'} horizontal={true}>
              {p.status}
            </Label>
          </Table.Cell>
        )),
      ['Accepts/Rejects']: (s: PoolStatus[]) =>
        _.map(s, (p: PoolStatus, i) => (
          <Table.Cell key={i} textAlign="center">
            {p.accepted}/{p.rejected}
          </Table.Cell>
        )),
    }
    const mapPoolCols =
      dashboardStatus.poolStatus.length > 0 &&
      _.map(Object.keys(poolTableMap), (name: string, index: number) => {
        return (
          <Table.Row key={index}>
            <Table.Cell>{name}</Table.Cell>
            {poolTableMap[name](dashboardStatus.poolStatus)}
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

    // Add lines based on how many board entries are in the data in the first entry
    const firstEntry = _.get(dashboardStatus.hashrateData, 0, {})
    let keys = _.keys(firstEntry)
    keys = _.remove(keys, (s: string) => s !== 'time' && s !== 'total')
    const areas = _.map(keys, (key: string, index: number) => (
      <Area
        type="monotone"
        dataKey={key}
        stroke="#FF0000"
        fill={chartColors[index]}
        stackId="1"
        key={index}
      />
    ))

    return (
      <Content>
        <Header as="h1">Dashboard</Header>
        <Header as="h2">Hashrate</Header>
        <ResponsiveContainer width="100%" height={300}>
          <ComposedChart
            data={dashboardStatus.hashrateData}
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
        <Table definition={true} striped={true} unstackable={true} className={classNames.table}>
          <Table.Header>
            <Table.Row>
              <Table.HeaderCell />
              {mapPoolHeaders}
            </Table.Row>
          </Table.Header>
          <Table.Body>{mapPoolCols}</Table.Body>
        </Table>

        <Header as="h2">Hashing Board Info</Header>
        <Table definition={true} striped={true} unstackable={true} className={classNames.table}>
          <Table.Header>
            <Table.Row>
              <Table.HeaderCell />
              {mapBoardHeaders}
            </Table.Row>
          </Table.Header>
          <Table.Body>{mapBoardCols}</Table.Body>
        </Table>

        <Header as="h2">System Info</Header>
        <Table striped={true} unstackable={true} className={classNames.table}>
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
