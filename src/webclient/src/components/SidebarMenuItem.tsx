// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import * as React from 'react'
import { Icon, Menu } from 'semantic-ui-react'

interface Props {
  active: boolean
  name: string
  label: string
  iconName: string
  onClick: () => void
}

export default class SidebarMenuItem extends React.Component<Props> {
  render() {
    const { active, name, iconName, label, onClick } = this.props

    const iconAny: any = iconName // Work around stupid type definitions for Icon
    return (
      <Menu.Item name={name} active={active} onClick={onClick}>
        <Icon name={iconAny} />
        {label}
      </Menu.Item>
    )
  }
}
