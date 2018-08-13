// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import * as React from 'react'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Table } from 'semantic-ui-react'

interface Props {
  label: string
  value: any
}

type CombinedProps = Props & InjectedProps

class Stat extends React.Component<CombinedProps> {
  public static styles: InputSheet<{}> = {
    label: {
      $debugName: 'label',
      textTransform: 'uppercase',
    },
  }

  render() {
    const { classNames, label, value } = this.props

    return (
      <Table.Row>
        <Table.Cell className={classNames.label}>{label}</Table.Cell>
        <Table.Cell>{value}</Table.Cell>
      </Table.Row>
    )
  }
}

export default withStyles()<any>(Stat)
