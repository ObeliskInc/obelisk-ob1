// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

export interface ApiOK {
  ok: 'ok'
  newId?: number
}

export interface ApiError {
  error: string
  context?: any // An optional object with additional context for the error
}
