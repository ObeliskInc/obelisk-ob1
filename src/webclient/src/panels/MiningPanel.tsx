// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import Content from 'components/Content'
import { Formik, FormikProps } from 'formik'
import { fetchMiningConfig, setMiningConfig } from 'modules/Main/actions'
import { getLastError, getMiningConfig } from 'modules/Main/selectors'
import { MiningConfig } from 'modules/Main/types'
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Form, Header, Message } from 'semantic-ui-react'

interface ConnectProps {
  miningConfig: MiningConfig
  lastError?: string
}

type CombinedProps = ConnectProps & InjectedProps & DispatchProp<any>

class MiningPanel extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    buttonGroup: {
      width: '100%',
      marginBottom: '20px !important',
    },
  }

  constructor(props: CombinedProps) {
    super(props)
    this.state = {
      optimizationMode: props.miningConfig.optimizationMode,
    }
  }

  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchMiningConfig.started({}))
    }
  }

  render() {
    const { classNames } = this.props

    return (
      <Content>
        <Header as="h1">Mining Config</Header>
        <Formik
          initialValues={this.props.miningConfig}
          enableReinitialize={true}
          onSubmit={(values: MiningConfig, formikBag: FormikProps<MiningConfig>) => {
            if (this.props.dispatch) {
              this.props.dispatch(setMiningConfig.started(values))
            }
          }}
          validateOnChange={true}
          validateOnBlur={true}
          validate={(values: MiningConfig): any => {
            const errors: any = {}
            return errors
          }}
          render={(formikBag: FormikProps<MiningConfig>) => {
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
            }

            const { optimizationMode } = formikProps.values

            let optimizationMsg: any
            switch (optimizationMode) {
              case 'efficiency':
                optimizationMsg = (
                  <Message
                    icon="leaf"
                    header="Efficiency"
                    content={
                      'This setting optimizes for lower electricity usage while still producing' +
                      ' good hashrate.'
                    }
                  />
                )
                break

              case 'balanced':
                optimizationMsg = (
                  <Message
                    icon="balance"
                    header="Balanced"
                    content={
                      'This setting optimizes for a balance between electricity use and ' +
                      'higher hashrate.'
                    }
                  />
                )
                break

              case 'hashrate':
                optimizationMsg = (
                  <Message
                    icon="lightning"
                    header="Best Hashrate"
                    content={
                      'This setting optimizes for more hashrate at the expense of ' +
                      'using substantially more electricity.'
                    }
                  />
                )
                break

              default:
                break
            }

            return (
              <div>
                <Form onSubmit={formikProps.handleSubmit}>
                  <Header as="h2">Optimization Mode</Header>
                  {optimizationMsg}
                  <Button.Group className={classNames.buttonGroup}>
                    {optimizationMode === 'efficiency' ? (
                      <Button color="red">Efficiency</Button>
                    ) : (
                      <Button
                        onClick={formikProps.handleOptimizationModeChange.bind(this, 'efficiency')}
                      >
                        Efficiency
                      </Button>
                    )}
                    {optimizationMode === 'balanced' ? (
                      <Button color="red">Balanced</Button>
                    ) : (
                      <Button
                        onClick={formikProps.handleOptimizationModeChange.bind(this, 'balanced')}
                      >
                        Balanced
                      </Button>
                    )}
                    {optimizationMode === 'hashrate' ? (
                      <Button color="red">Hashrate</Button>
                    ) : (
                      <Button
                        onClick={formikProps.handleOptimizationModeChange.bind(this, 'hashrate')}
                      >
                        Hashrate
                      </Button>
                    )}
                  </Button.Group>
                  <Button type="Submit">SAVE</Button>
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
  miningConfig: getMiningConfig(state.Main),
  lastError: getLastError(state.Main),
})

const miningPanel = withStyles()<any>(MiningPanel)

export default connect<ConnectProps, any, any>(mapStateToProps)(miningPanel)
