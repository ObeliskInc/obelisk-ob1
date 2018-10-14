// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import Content from 'components/Content'
import { Formik, FormikProps } from 'formik'
import _ = require('lodash')
import {
  changePassword,
  fetchSystemConfig,
  rebootMiner,
  setSystemConfig,
  uploadFirmwareFile,
  clearFormStatus,
  resetConfig,
} from 'modules/Main/actions'
import { getLastError, getSystemConfig, getUploadStatus, getUpgradeMessage } from 'modules/Main/selectors'
import { SystemConfig, UploadStatus } from 'modules/Main/types'
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Dimmer, Form, Header, Loader, Message, Progress } from 'semantic-ui-react'
import { getHistory } from 'utils'

const history = getHistory()

const timezones = require('utils/timezones.json')
const Dropzone = require('react-dropzone').default

interface ConnectProps {
  systemConfig: SystemConfig
  uploadStatus: UploadStatus
  upgradeMessage?: string
  lastError?: string
  systemForm: string
  passwordForm: string
}

type CombinedProps = ConnectProps & InjectedProps & DispatchProp<any>

const timezoneOptions = _.map(timezones, (tz: any, index: number) => {
  const displayName = _.replace(tz.name, '_', ' ')
  return {
    text: `${displayName} (${tz.offset})`,
    value: tz.name,
    key: index,
  }
})

class SystemPanel extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    buttonGroup: {
      width: '100%',
    },
    upload: {
      width: '100%',
      height: 'auto',
      padding: 20,
      border: '2px dashed #808080',
      borderRadius: 0,
      cursor: 'pointer',
      marginBottom: 20,
    },
    uploadDesc: {
      textAlign: 'center',
    },
    uploadFilename: {
      paddingTop: 20,
      textAlign: 'center',
      fontFamily: 'Karbon Regular',
      color: 'white',
    },
    uploadError: {
      paddingTop: 20,
      textAlign: 'center',
      fontFamily: 'Karbon Regular',
      color: 'red',
    },
  }

  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchSystemConfig.started({}))
      this.props.dispatch(clearFormStatus())
    }
  }

  render() {
    const { classNames, lastError, systemForm, passwordForm, upgradeMessage } = this.props

    const renderSave = (dirty: any) => {
      switch (systemForm) {
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

    const renderChangePassword = (formikProps: any) => {
      switch (passwordForm) {
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
      return (
        <Button type="button" onClick={formikProps.handleChangePassword}>
          CHANGE PASSWORD
        </Button>
      )
    }

    return (
      <Content>
        <Header as="h1">System Config</Header>
        <Formik
          initialValues={{
            ...(this.props.systemConfig || {}),
            oldPassword: '',
            newPassword: '',
          }}
          enableReinitialize={true}
          onSubmit={(values: SystemConfig, formikBag: FormikProps<SystemConfig>) => {
            if (this.props.dispatch) {
              const valuesToSend = {
                timezone: values.timezone,
              }
              this.props.dispatch(setSystemConfig.started(valuesToSend))
            }
          }}
          validateOnChange={true}
          validateOnBlur={true}
          validate={(values: SystemConfig): any => {
            const errors: any = {}
            return errors
          }}
          render={(formikBag: FormikProps<SystemConfig>) => {
            const formikProps = {
              ...formikBag,
              handleBlur: (e: any, data: any) => {
                if (data && data.name) {
                  formikProps.setFieldValue(data.name, data.value)
                  formikProps.setFieldTouched(data.name)
                }
              },
              handleChange: (e: any, data: any) => {
                if (data && data.name) {
                  formikProps.setFieldValue(data.name, data.value)
                }
              },
              handleOptimizationModeChange: (mode: any) => {
                formikProps.setFieldValue('optimizationMode', mode)
              },

              handleReboot: () => {
                const { dispatch } = this.props

                if (dispatch) {
                  if (
                    confirm(
                      'This will immediately reboot the device and log you out of this app.\n\n' +
                        'Do you want to continue?',
                    )
                  ) {
                    dispatch(rebootMiner.started({}))
                    history.push('/login')
                  }
                }
              },

              handleConfigReset: () => {
                const { dispatch } = this.props
                if (dispatch) {
                  if (
                    confirm(
                      'This will remove your current configuration settings and reset it to system default.\n\n' +
                        'Do you want to continue?',
                    )
                  ) {
                    dispatch(resetConfig.started({}))
                    history.push('/login')
                  }
                }
              },

              handleChangePassword: () => {
                const { dispatch } = this.props
                const { oldPassword, newPassword } = formikProps.values

                if (dispatch) {
                  dispatch(
                    changePassword.started({
                      username: 'admin',
                      oldPassword: oldPassword || '',
                      newPassword: newPassword || '',
                    }),
                  )
                }
              },

              handleDrop: (files: any[]) => {
                const { dispatch } = this.props

                if (dispatch) {
                  dispatch(uploadFirmwareFile.started({ file: files[0] }))
                }
              },

              handleOpenFirmwareLink: () => {
                const win: any = window.open('http://obelisk.tech/downloads.html', '_blank')
                win.focus()
              },
            }

            const { filename, percent } = this.props.uploadStatus
            let uploadError

            // Set upload title
            let uploadTitle
            if (filename) {
              if (lastError) {
                uploadTitle = 'Error Uploading File'
                uploadError = lastError
              } else if (percent === 100) {
                uploadTitle = 'Uploading Complete'
              } else {
                uploadTitle = 'Uploading File...'
              }
            } else {
              uploadTitle =
                'Drag and drop a firmware file here, or click to select ' +
                'a file to start uploading.'
            }

            return (
              <div>
                <Form onSubmit={formikProps.handleSubmit}>
                  <Header as="h2">Time Config</Header>
                  <Form.Dropdown
                    label="TIME ZONE"
                    name="timezone"
                    placeholder="Select your time zone"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    selection={true}
                    search={true}
                    options={timezoneOptions}
                    value={formikProps.values.timezone}
                  />
                  {renderSave(formikProps.dirty)}
                </Form>

                <Form onSubmit={formikProps.handleSubmit}>
                  <Header as="h2">Change Password</Header>
                  <Form.Input label="USERNAME" name="username" value="admin" />
                  <Form.Input
                    type="password"
                    label="OLD PASSWORD"
                    name="oldPassword"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    value={formikProps.values.oldPassword}
                    error={!!_.get(formikProps.errors, ['oldPassword'], '')}
                  />
                  <Form.Input
                    type="password"
                    label="NEW PASSWORD"
                    name="newPassword"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    value={formikProps.values.newPassword}
                    error={!!_.get(formikProps.errors, ['newPassword'], '')}
                  />
                  {renderChangePassword(formikProps)}
                </Form>

                <Form>
                  <Header as="h2">Firmware</Header>
                    <Dropzone className={classNames.upload} onDrop={formikProps.handleDrop}>
                      <div className={classNames.uploadDesc}>{upgradeMessage || uploadTitle}</div>
                      {filename
                        ? [
                            <div key="filename" className={classNames.uploadFilename}>
                              {filename}
                            </div>,
                            <div key="uploadError" className={classNames.uploadError}>
                              {uploadError}
                            </div>,
                            <Progress
                              key="progress"
                              percent={percent}
                              inverted={true}
                              progress={true}
                            />,
                          ]
                        : undefined}
                    </Dropzone>
                  <Button
                    icon="cloud download"
                    onClick={formikProps.handleOpenFirmwareLink}
                    content={'FIND LATEST FIRMWARE'}
                  />
                </Form>
                <Form>
                  <Header as="h2">System Actions</Header>
                  <Message
                    icon="warning sign"
                    header="Reboot the Miner"
                    content={
                      'This will immediately reboot the miner, and you will need to reload this ' +
                      'page and login again.'
                    }
                  />
                  <Button onClick={formikProps.handleReboot}>REBOOT</Button>
                  <Message
                    icon="warning sign"
                    header="Reset the Config"
                    content={'This will reset machine config files to default settings.'}
                  />
                  <Button onClick={formikProps.handleConfigReset}>RESET</Button>
                </Form>
              </div>
            )
          }}
        />
      </Content>
    )
  }
}

const mapStateToProps = (state: any, props: any): ConnectProps => ({
  systemConfig: getSystemConfig(state.Main),
  uploadStatus: getUploadStatus(state.Main),
  upgradeMessage: getUpgradeMessage(state.Main),
  lastError: getLastError(state.Main),
  systemForm: state.Main.forms.systemForm,
  passwordForm: state.Main.forms.passwordForm,
})

const systemPanel = withStyles()<any>(SystemPanel)

export default connect<ConnectProps, any, any>(mapStateToProps)(systemPanel)
