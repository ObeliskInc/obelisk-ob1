import * as React from 'react'
import withStyles, { InjectedProps, InputSheet } from 'react-typestyle'

import * as Colors from 'utils/colors'

interface Props {
  title: any
  bodyContent?: any
  footerContent?: any
}

type CombinedProps = Props & InjectedProps

const dialogBorder = `2px solid ${Colors.mediumGrey}`

class DialogImpl extends React.PureComponent<CombinedProps> {
  public static styles: InputSheet<{}> = {
    dialogContainer: {
      $debugName: 'dialogContainer',
      padding: 20,
      background: 'transparent',
      borderRadius: 25,
      maxWidth: 500,
    },
    title: {
      $debugName: 'title',
      fontSize: 20,
      color: Colors.white,
      textAlign: 'center',
      fontFamily: 'Karbon Regular',
      borderTop: dialogBorder,
      borderLeft: dialogBorder,
      borderRight: dialogBorder,
      paddingTop: 12,
      paddingBottom: 8,
      background: Colors.darkGrey,
    },
    bodyContainer: {
      background: Colors.veryDarkGrey,
      borderLeft: dialogBorder,
      borderRight: dialogBorder,
      borderBottom: dialogBorder,
      borderTop: dialogBorder,
      padding: 20,
    },
    footerContainer: {
      $debugName: 'buttonContainer',
      display: 'flex',
      flexDirection: 'column',
      justifyContent: 'space-around',
      alignItems: 'center',
      borderBottom: dialogBorder,
      borderLeft: dialogBorder,
      borderRight: dialogBorder,
      paddingTop: 10,
      paddingBottom: 10,
      background: Colors.darkGrey,
    },
  }

  render() {
    const { classNames, title, footerContent, bodyContent } = this.props

    const bodyDiv = bodyContent ? (
      <div className={classNames.bodyContainer}>{bodyContent}</div>
    ) : (
      undefined
    )

    return (
      <div className={classNames.dialogContainer}>
        <div className={classNames.title}>{title}</div>
        {bodyDiv}
        <div className={classNames.footerContainer}>{footerContent}</div>
      </div>
    )
  }
}

export const Dialog = withStyles()<Props>(DialogImpl)
