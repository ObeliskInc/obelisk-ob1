// Copied from https://github.com/sindresorhus/ip-regex
// That code causes a minify error during production builds, so it was copied here.

const word = '[a-fA-F\\d:]'
const b = `(?:(?<=\\s|^)(?=${word})|(?<=${word})(?=\\s|$))`

const v4 =
  '(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)(?:\\.(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)){3}'

const v6seg = '[a-fA-F\\d]{1,4}'
const v6 = `
(
(?:${v6seg}:){7}(?:${v6seg}|:)|
(?:${v6seg}:){6}(?:${v4}|:${v6seg}|:)|
(?:${v6seg}:){5}(?::${v4}|(:${v6seg}){1,2}|:)|
(?:${v6seg}:){4}(?:(:${v6seg}){0,1}:${v4}|(:${v6seg}){1,3}|:)|
(?:${v6seg}:){3}(?:(:${v6seg}){0,2}:${v4}|(:${v6seg}){1,4}|:)|
(?:${v6seg}:){2}(?:(:${v6seg}){0,3}:${v4}|(:${v6seg}){1,5}|:)|
(?:${v6seg}:){1}(?:(:${v6seg}){0,4}:${v4}|(:${v6seg}){1,6}|:)|
(?::((?::${v6seg}){0,5}:${v4}|(?::${v6seg}){1,7}|:))
)(%[0-9a-zA-Z]{1,})?
`
  .replace(/\s*\/\/.*$/gm, '')
  .replace(/\n/g, '')
  .trim()

const ipRegex: any = (opts: any) =>
  opts && opts.exact
    ? new RegExp(`(?:^${v4}$)|(?:^${v6}$)`)
    : new RegExp(`(?:${b}${v4}${b})|(?:${b}${v6}${b})`, 'g')

ipRegex.v4 = (opts: any) =>
  opts && opts.exact ? new RegExp(`^${v4}$`) : new RegExp(`${b}${v4}${b}`, 'g')
ipRegex.v6 = (opts: any) =>
  opts && opts.exact ? new RegExp(`^${v6}$`) : new RegExp(`${b}${v6}${b}`, 'g')

export const isIp: any = (x: any) => ipRegex({ exact: true }).test(x)
isIp.v4 = (x: any) => ipRegex.v4({ exact: true }).test(x)
isIp.v6 = (x: any) => ipRegex.v6({ exact: true }).test(x)
