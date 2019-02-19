// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

// Common dialog style objects
import * as Colors from 'utils/colors'

export const dialogStyles: any = {
  container: {
    $debugName: 'container',
    display: 'flex',
    flex: 1,
    background: Colors.black,
    color: Colors.mutedText,
    overflowY: 'hidden',
    borderBottomLeftRadius: 16,
    borderBottomRightRadius: 16,
    justifyContent: 'space-around',
    alignItems: 'center',
  },
  fieldContainer: {
    $debugName: 'fieldContainer',
    display: 'flex',
    alignItems: 'center',
    marginTop: 8,
  },
  fieldLabel: {
    $debugName: 'fieldLabel',
    fontSize: 18,
    color: Colors.white,
    fontFamily: 'Karbon Regular',
    marginRight: 20,
    width: 84,
  },
  fieldInput: {
    $debugName: 'fieldInput',
    color: Colors.white,
    background: 'transparent',
    border: 'none',
    paddingLeft: 10,
    fontFamily: 'Karbon Regular',
    fontSize: 18,
    marginRight: 15,
    flex: 1,
    borderBottom: `1px solid ${Colors.brightGrey}`,
  },
  message: {
    $debugName: 'message',
    fontFamily: 'Karbon Regular',
    marginBottom: 20,
    fontSize: 16,
  },
  successMessage: {
    $debugName: 'loggingInMessage',
    fontFamily: 'Karbon Regular',
    marginTop: 20,
    fontSize: 16,
    color: '#008000',
  },
  textLink: {
    $debugName: 'textLink',
    textDecoration: 'underline',
    fontFamily: 'Karbon Regular',
    fontSize: 12,
    cursor: 'pointer',
  },
  errorMessage: {
    $debugName: 'errorMessage',
    fontFamily: 'Karbon Regular',
    marginTop: 20,
    fontSize: 16,
    color: Colors.brightRed,
  },
}
