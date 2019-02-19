// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import Content from 'components/Content'
import { Formik, FormikProps } from 'formik'
import _ = require('lodash')
import { fetchPoolsConfig, setPoolsConfig, clearFormStatus } from 'modules/Main/actions'
import { getPoolsConfig } from 'modules/Main/selectors'
import { MAX_POOLS, PoolConfig } from 'modules/Main/types'
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import { withRouter, BrowserRouterProps } from 'react-router-dom'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Form, Header, Dimmer, Loader } from 'semantic-ui-react'
import { common } from 'styles/common'

const validUrl = require('valid-url')

interface ConnectProps {
  poolsConfig: PoolConfig[]
  poolForm: string
}

type CombinedProps = BrowserRouterProps & ConnectProps & InjectedProps & DispatchProp<any>

class PoolsPanel extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    formFieldError: { ...common.formFieldError },
  }

  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchPoolsConfig.started({}))
      this.props.dispatch(clearFormStatus())
    }
  }

  render() {
    const { classNames, poolForm } = this.props
    const renderSave = (dirty: any) => {
      switch (poolForm) {
        case 'started':
          return (
            <Dimmer active>
              <Loader />
            </Dimmer>
          )
        case 'failed':
          return <span>Failed</span>
        case 'done':
          if (dirty) {
            return <Button type="submit">SAVE</Button>
          }
          return <span>Done</span>
      }
      if (dirty) {
        return <Button type="submit">SAVE</Button>
      }
      return
    }

    return (
      <Content>
        <Header as="h1">POOLS CONFIG</Header>
        <Formik
          initialValues={{ ...(this.props.poolsConfig || {}) }}
          enableReinitialize={true}
          onSubmit={(values: PoolConfig[], formikBag: FormikProps<PoolConfig[]>) => {
            if (this.props.dispatch) {
              // Convert from objects to arrays
              const poolConfigs = _.map(values, (value: PoolConfig, index: number) => value)
              this.props.dispatch(setPoolsConfig.started(poolConfigs))
              this.props.dispatch(fetchPoolsConfig.started({}))
            }
          }}
          validateOnChange={true}
          validateOnBlur={true}
          validate={(values: PoolConfig[]): any => {
            const errors: any = {}

            for (let i = 0; i < Object.keys(values).length; i++) {
              if (values[i].url !== '' && !validUrl.isUri(values[i].url)) {
                _.set(errors, [i, 'url'], 'Invalid URL')
              }
            }
            return errors
          }}
          render={(formikBag: FormikProps<PoolConfig[]>) => {
            const formikProps = {
              ...formikBag,
              // TODO: Extract this code to a common place or HoC
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

            // Create pool inputs
            const poolFields = []
            for (let i = 0; i < MAX_POOLS; i++) {
              poolFields.push(
                <Header as="h2" key={`${i}_pool_header`}>
                  Pool {i + 1}
                </Header>,
              )

              // URL
              poolFields.push(
                <Form.Input
                  key={`${i}_url`}
                  label="URL"
                  name={`${i}.url`}
                  placeholder="URL"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={_.get(formikProps.values, [i, 'url'], '')}
                  error={!!_.get(formikProps.errors, [i, 'url'], '')}
                />,
              )
              poolFields.push(
                <div className={classNames.formFieldError} key={`${i}_url_error`}>
                  {_.get(formikProps.errors, [i, 'url'], '')}
                </div>,
              )

              // Worker
              poolFields.push(
                <Form.Input
                  key={`${i}_worker`}
                  label="WORKER"
                  name={`${i}.worker`}
                  placeholder="Worker"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={_.get(formikProps.values, [i, 'worker'], '')}
                  error={!!_.get(formikProps.errors, [i, 'worker'], '')}
                />,
              )
              poolFields.push(
                <div className={classNames.formFieldError} key={`${i}_worker_error`}>
                  {_.get(formikProps.errors, [i, 'worker'], '')}
                </div>,
              )

              // Password
              poolFields.push(
                <Form.Input
                  key={`${i}_password`}
                  label="PASSWORD"
                  name={`${i}.password`}
                  placeholder="Password"
                  onChange={formikProps.handleChange}
                  onBlur={formikProps.handleBlur}
                  value={_.get(formikProps.values, [i, 'password'], '')}
                  error={!!_.get(formikProps.errors, [i, 'password'], '')}
                />,
              )
              poolFields.push(
                <div className={classNames.formFieldError} key={`${i}_password_error`}>
                  {_.get(formikProps.errors, [i, 'password'], '')}
                </div>,
              )
            }
            return (
              <Form onSubmit={formikProps.handleSubmit}>
                {poolFields}

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
  poolsConfig: getPoolsConfig(state.Main) || [],
  poolForm: state.Main.forms.poolForm,
})

const poolsPanel = withStyles()<any>(PoolsPanel)

export default withRouter(connect<ConnectProps, any, any>(mapStateToProps)(poolsPanel))
