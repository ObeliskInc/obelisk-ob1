// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

// Common style objects
import * as Colors from 'utils/colors'

export const common: any = {
  panelContainer: {
    $debugName: 'panelContainer',
    fontSize: 16,
    margin: 20,
    color: Colors.mutedText,
    width: '100%',
    display: 'flex',
    flex: 1,
    flexDirection: 'column',
    overflowY: 'scroll',
  },
  header: {
    $debugName: 'header',
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 20,
    height: 40,
  },
  heading: {
    $debugName: 'heading',
    fontSize: 30,
    color: Colors.white,
    top: 5,
    position: 'relative',
  },

  // Details Container
  detailsContainer: {
    $debugName: 'detailsContainer',
    fontSize: 20,
    margin: 20,
    color: Colors.mutedText,
    width: '100%',
    flex: 1,
    overflowY: 'auto',
    overflowX: 'hidden',
    position: 'relative',
  },
  divider: {
    $debugName: 'divider',
    height: 1,
    width: '100%',
    background: 'linear-gradient(to right, #fd0000 0%,#1e2d2a 50%,#000000 100%)',
    marginBottom: 0,
    position: 'relative',
    top: -10,
  },
  errorMessage: {
    $debugName: 'errorMessage',
    color: Colors.brightRed,
    marginBottom: 20,
  },

  formField: {
    $debugName: 'formField',
    color: Colors.white,
    background: 'transparent',
    border: 'none',
    paddingLeft: 10,
    fontFamily: 'Karbon Regular',
    fontSize: 20,
    marginRight: 15,
    flex: 1,
    $nest: {
      '&:disabled': {
        color: '#b0b0b0',
      },
    },
  },

  selectField: {
    $debugName: 'selectField',
    background: 'transparent',
    color: Colors.white,
    border: 'none',
    paddingLeft: 2,
    fontSize: 20,
    fontFamily: 'Karbon Regular',
    maxWidth: 400,
    $nest: {
      '&:disabled': {
        color: '#b0b0b0',
      },
    },
  },

  formFieldError: {
    $debugName: 'formFieldError',
    color: '#ed1d25',
    position: 'relative',
    top: -11,
    height: 13,
    fontFamily: 'Karbon Regular',
  },
}
