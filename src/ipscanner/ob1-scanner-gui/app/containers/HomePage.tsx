import Home from '../components/Home'
import { connect, Dispatch } from 'react-redux'
import { IState } from '../reducers'
import { bindActionCreators } from 'redux'
import * as BridgeActions from '../actions/bridge'
import { IProps } from '../components/Home'

const mapStateToProps = (state: IState): Partial<IProps> => {
  return {
    logs: state.bridge.logs,
    miners: state.bridge.miners,
    loading: state.bridge.loading,
  }
}

const mapDispatchToProps = (dispatch: Dispatch<IState>) => {
  return bindActionCreators(BridgeActions as any, dispatch)
}

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(Home)
