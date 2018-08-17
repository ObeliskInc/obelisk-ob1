// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

import { createBrowserHistory, History } from 'history'
// import * as _ from 'lodash'
import * as moment from 'moment'

const formatter = new Intl.NumberFormat()

// String Enum maker
export function StrEnum<T extends string>(arr: T[]): { [K in T]: K } {
  return arr.reduce(
    (res, key) => {
      res[key] = key
      return res
    },
    // Object.create() requires null, but we don't use it elsewhere.
    // tslint:disable-next-line
    Object.create(null),
  )
}

export function ignoreCaseCompare(a: string, b: string): number {
  return a.toLowerCase().localeCompare(b.toLowerCase())
}

export function formatCurrency(n: number): string {
  return `$${formatter.format(n)}`
}

let history: History | undefined = undefined
export function getHistory(): History {
  if (!history) {
    history = createBrowserHistory()
  }
  return history
}

export function formatDate(date: string | number) {
  return moment(date)
    .utc()
    .format('YYYY-MM-DD')
}

export function formatTime(time: number) {
  return moment(time * 1000)
    .utc()
    .format('HH:mm')
}
