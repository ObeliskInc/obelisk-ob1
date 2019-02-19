// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import Content from 'components/Content'
import { Formik, FormikProps } from 'formik'
import _ = require('lodash')
import { fetchNetworkConfig, setNetworkConfig } from 'modules/Main/actions'
import { getNetworkConfig } from 'modules/Main/selectors'
import { NetworkConfig } from 'modules/Main/types'
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import { withRouter, BrowserRouterProps } from 'react-router-dom'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Dimmer, Form, Header, Loader } from 'semantic-ui-react'

import { common } from 'styles/common'
import { getHistory } from 'utils'

const history = getHistory()

import { isIp as isValidIp } from 'utils/ip'

interface ConnectProps {
  networkConfig?: NetworkConfig
  networkForm: string
}

type CombinedProps = BrowserRouterProps & ConnectProps & InjectedProps & DispatchProp<any>

const dhcpOptions = [
  { text: 'Enabled', value: 'enabled', key: 0 },
  { text: 'Disabled', value: 'disabled', key: 1 },
]

class NetworkPanel extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    formFieldError: { ...common.formFieldError },
  }

  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchNetworkConfig.started({}))
    }
  }

  render() {
    const { classNames, networkForm } = this.props

    const renderSave = (dirty: any) => {
      switch (networkForm) {
        case 'started':
          return (
            <Dimmer active>
              <Loader />
            </Dimmer>
          )
        case 'failed':
          return <span>Failed</span>
        case 'done':
          return <span>Done</span>
      }
      if (dirty) {
        return <Button type="submit">SAVE</Button>
      }
      return undefined
    }

    return (
      <Content>
        <Header as="h1">NETWORK CONFIG</Header>
        <Formik
          initialValues={{ ...(this.props.networkConfig || {}) }}
          enableReinitialize={true}
          onSubmit={(values: NetworkConfig, formikBag: FormikProps<NetworkConfig>) => {
            if (this.props.dispatch) {
              if (
                confirm(
                  'This will save the settings to the device and then reboot it.  ' +
                    'You will be logged out of this app.\n\n' +
                    'If your settings are incorrect or the device is assigned a new ' +
                    'IP address by the DHCP server, you may lose the ability to ' +
                    'reconnect to the device.\n\n' +
                    'Do you want to continue?',
                )
              ) {
                this.props.dispatch(setNetworkConfig.started(values))
                this.props.dispatch(fetchNetworkConfig.started({}))
                history.push('/login')
              }
            }
          }}
          validateOnChange={true}
          validateOnBlur={true}
          validate={(values: NetworkConfig): any => {
            const errors: any = {}
            if (values.hostname.length === 0) {
              errors.hostname = 'Hostname must be provided'
            }

            if (values.dhcpEnabled === 'enabled') {
              return errors
            }

            // Only check these if DHCP is disabled
            if (!isValidIp(values.ipAddress)) {
              errors.ipAddress = 'Invalid IP address'
            }

            if (!isValidIp(values.subnetMask)) {
              errors.subnetMask = 'Invalid subnet mask'
            }

            if (!isValidIp(values.gateway)) {
              errors.gateway = 'Invalid gateway address'
            }

            if (!isValidIp(values.dnsServer)) {
              errors.dnsServer = 'Invalid DNS server address'
            }

            return errors
          }}
          render={(formikBag: FormikProps<NetworkConfig>) => {
            const formikProps = {
              ...formikBag,
              handleBlur: (e: any, data: any) => {
                if (data && data.name) {
                  formikProps.setFieldValue(data.name, data.value)
                  formikProps.setFieldTouched(data.name)
                }
              },
              handleChange: (ea: any, data: any) => {
                if (data && data.name) {
                  formikProps.setFieldValue(data.name, data.value)
                }
              },
            }
            const disableNetworkFields = formikProps.values.dhcpEnabled === 'enabled'

            return (
              <Form onSubmit={formikProps.handleSubmit}>
                <Form.Input
                  label="HOSTNAME"
                  name="hostname"
                  placeholder="Hostname"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={formikProps.values.hostname}
                  error={formikProps.errors.hostname}
                />
                <div className={classNames.formFieldError}>
                  {_.get(formikProps.errors, ['hostname'], ' ')}
                </div>
                <Form.Dropdown
                  options={dhcpOptions}
                  selection={true}
                  label="DHCP"
                  name="dhcpEnabled"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={formikProps.values.dhcpEnabled}
                  error={formikProps.errors.dhcpEnabled}
                />
                <div className={classNames.formFieldError}>
                  {_.get(formikProps.errors, ['dhcpEnabled'], ' ')}
                </div>
                <Form.Input
                  label="IP ADDRESS"
                  name="ipAddress"
                  placeholder="IP Address"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={formikProps.values.ipAddress}
                  error={formikProps.errors.ipAddress}
                  disabled={disableNetworkFields}
                />
                <div className={classNames.formFieldError}>
                  {_.get(formikProps.errors, ['ipAddress'], ' ')}
                </div>
                <Form.Input
                  label="SUBNET MASK"
                  name="subnetMask"
                  placeholder="Subnet Mask"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={formikProps.values.subnetMask}
                  error={formikProps.errors.subnetMask}
                  disabled={disableNetworkFields}
                />
                <div className={classNames.formFieldError}>
                  {_.get(formikProps.errors, ['subnetMask'], ' ')}
                </div>
                <Form.Input
                  label="GATEWAY"
                  name="gateway"
                  placeholder="Gateway"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={formikProps.values.gateway}
                  error={formikProps.errors.gateway}
                  disabled={disableNetworkFields}
                />
                <div className={classNames.formFieldError}>
                  {_.get(formikProps.errors, ['gateway'], ' ')}
                </div>
                <Form.Input
                  label="DNS SERVER"
                  name="dnsServer"
                  placeholder="DNS Server"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={formikProps.values.dnsServer}
                  error={formikProps.errors.dnsServer}
                  disabled={disableNetworkFields}
                />
                <div className={classNames.formFieldError}>
                  {_.get(formikProps.errors, ['dnsServer'], ' ')}
                </div>
                {renderSave(formikProps.dirty)}
              </Form>
            )
          }}
        />
      </Content>
    )
  }
}

const mapStateToProps = (state: any, props: any): ConnectProps => ({
  networkConfig: getNetworkConfig(state.Main),
  networkForm:   state.Main.forms.networkForm,

})

const networkPanel = withStyles()<any>(NetworkPanel)

export default withRouter(connect<ConnectProps, any, any>(mapStateToProps)(networkPanel))
