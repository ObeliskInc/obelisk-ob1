// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import Content from 'components/Content'
import { Formik, FormikProps } from 'formik'
import { fetchMiningConfig, setMiningConfig } from 'modules/Main/actions'
import { getLastError, getMiningConfig } from 'modules/Main/selectors'
import { MiningConfig } from 'modules/Main/types'
import { OptimizationModeHashrate, OptimizationModeBalanced, OptimizationModeEfficiency} from 'utils/optmizationMode'
import * as React from 'react'
import { connect, DispatchProp } from 'react-redux'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'
import { Button, Dimmer, Form, Header, Loader, Message } from 'semantic-ui-react'

interface ConnectProps {
  miningConfig: MiningConfig
  lastError?: string
  miningForm: string
}

type CombinedProps = ConnectProps & InjectedProps & DispatchProp<any>

const fanSpeedOptions = (() => {
  const result = []
  for (let i=10; i<=100; i+=5) {
    result.push(  { text: `${i}%`, value: i, key: i })
  }
  return result
})()

const hotChipTempOptions = (() => {
  const result = []
  for (let i=80; i<=105; i++) {
    result.push(  { text: `${i}° C`, value: i, key: i })
  }
  return result
})()


const rebootIntervalOptions = (() => {
  // First three entries are special
  const result = [
    { text: 'NEVER', value: 0, key: 0},
    { text: '0.5 hours', value: 30, key: 30},
    { text: '1 hour', value: 60, key: 60}
  ]

  // Add the rest
  for (let i=2; i<=24; i++) {
    const minutes = i * 60
    result.push(  { text: `${i} hours`, value: minutes, key: minutes })
  }
  return result
})()

const rebootHashrateOptions = (() => {
  const result = [{ text: 'NEVER', value: 0, key: 0}]
  for (let i=100; i<=400; i += 25) {
    result.push(  { text: `${i} GH/s`, value: i, key: i })
  }
  return result
})()

const geneticAlgoOptions = [
  { text: "Enabled", value: false, key: 0 },
  { text: "Disabled", value: true, key: 1 },
]

class MiningPanel extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    buttonGroup: {
      width: '100%',
      marginBottom: '20px !important',
    },
  }

  componentWillMount() {
    if (this.props.dispatch) {
      this.props.dispatch(fetchMiningConfig.started({}))
    }
  }

  render() {
    const { classNames, miningForm } = this.props

    
    const renderSave = (dirty: any) => {
      switch (miningForm) {
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
      return undefined
    }

    return (
      <Content>
        <Header as="h1">Mining Config</Header>
        <Formik
          initialValues={this.props.miningConfig}
          enableReinitialize={true}
          onSubmit={(values: MiningConfig, formikBag: FormikProps<MiningConfig>) => {
            if (this.props.dispatch) {
              this.props.dispatch(setMiningConfig.started(values))
              this.props.dispatch(fetchMiningConfig.started({}))
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
              handleOptimizationModeChange: (mode: number) => {
                formikProps.setFieldValue('optimizationMode', mode)
              },
            }

            const { optimizationMode } = formikProps.values

            let optimizationMsg: any
            switch (optimizationMode) {
              case OptimizationModeEfficiency:
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

              case OptimizationModeBalanced:
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

              case OptimizationModeHashrate:
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
                  { /*
                  <Header as="h2">Optimization Mode</Header>
                  {optimizationMsg}
                  <Button.Group className={classNames.buttonGroup}>
                    {optimizationMode === OptimizationModeEfficiency ? (
                      <Button color="red">Efficiency</Button>
                    ) : (
                      <Button
                        onClick={formikProps.handleOptimizationModeChange.bind(this, OptimizationModeEfficiency)}
                      >
                        Efficiency
                      </Button>
                    )}
                    {optimizationMode === OptimizationModeBalanced ? (
                      <Button color="red">Balanced</Button>
                    ) : (
                      <Button
                        onClick={formikProps.handleOptimizationModeChange.bind(this, OptimizationModeBalanced)}
                      >
                        Balanced
                      </Button>
                    )}
                    {optimizationMode === OptimizationModeHashrate ? (
                      <Button color="red">Hashrate</Button>
                    ) : (
                      <Button
                        onClick={formikProps.handleOptimizationModeChange.bind(this, OptimizationModeHashrate)}
                      >
                        Hashrate
                      </Button>
                    )}
                  </Button.Group>
                    */ }

                  <Header as="h2">Advanced Controls</Header>
                  <Form.Dropdown
                    options={fanSpeedOptions}
                    selection={true}
                    label="MIN. FAN SPEED"
                    name="minFanSpeedPercent"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    value={formikProps.values.minFanSpeedPercent}
                    error={formikProps.errors.minFanSpeedPercent}
                  />

                  <Form.Dropdown
                    options={hotChipTempOptions}
                    selection={true}
                    label="MAX. HOT CHIP TEMP."
                    name="maxHotChipTempC"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    value={formikProps.values.maxHotChipTempC}
                    error={formikProps.errors.maxHotChipTempC}
                  />

                  <Form.Dropdown
                    options={rebootIntervalOptions}
                    selection={true}
                    label="REBOOT EVERY"
                    name="rebootIntervalMins"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    value={formikProps.values.rebootIntervalMins}
                    error={formikProps.errors.rebootIntervalMins}
                  />

                  <Form.Dropdown
                    options={rebootHashrateOptions}
                    selection={true}
                    label="REBOOT IF HASHRATE OF ANY BOARD IS LESS THAN"
                    name="rebootMinHashrate"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    value={formikProps.values.rebootMinHashrate}
                    error={formikProps.errors.rebootMinHashrate}
                  />

                  <Form.Dropdown
                    options={geneticAlgoOptions}
                    selection={true}
                    label="GENETIC ALGORITHM (ATTEMPTS TO OPTIMIZE HASHRATE)"
                    name="disableGeneticAlgo"
                    onChange={formikProps.handleChange}
                    onBlur={formikProps.handleBlur}
                    value={formikProps.values.disableGeneticAlgo}
                    error={formikProps.errors.disableGeneticAlgo}
                  />

                    {renderSave(formikProps.dirty)}
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
  miningForm: state.Main.forms.miningForm,
})

const miningPanel = withStyles()<any>(MiningPanel)

export default connect<ConnectProps, any, any>(mapStateToProps)(miningPanel)
