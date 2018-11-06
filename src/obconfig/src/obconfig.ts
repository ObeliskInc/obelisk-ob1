// Copyright 2018 Ken Carpenter
import axios from 'axios'
// import fs = require('fs')
import * as format from 'string-template'
import * as _ from 'lodash'

axios.defaults.withCredentials = true

const axiosCookieJarSupport = require('axios-cookiejar-support').default
const tough = require('tough-cookie')

axiosCookieJarSupport(axios)

const cookieJar = new tough.CookieJar()

interface NetworkConfig {
  ipAddress?: string
  subnetMask?: string
  dhcpEnabled?: boolean
  gateway?: string
  dnsServer?: string
  hostname?: string
}

interface PoolConfig {
  url?: string
  worker?: string
  password?: string
}

interface MiningConfig {
  optimizationMode?: number
  minFanSpeedPercent?: number
  maxHotChipTempC?: number
  rebootIntervalMins?: number
  rebootMinHashrate?: number
  disableGeneticAlgo?: boolean
}

interface SystemConfig {
  timezone?: string
}

interface MinerConfig {
  id?: number
  address?: string
  serial?: string
  model?: string
  group?: string
  username?: string
  password?: string
  network?: NetworkConfig
  system?: SystemConfig
  pools?: PoolConfig[]
  mining?: MiningConfig
}

interface Config {
  groups: { [name: string]: MinerConfig }
  miners: MinerConfig[]
}

const defaultConfig: MinerConfig = {
  username: 'admin',
  password: 'admin',
  network: { dhcpEnabled: true, ipAddress: '', subnetMask: '', gateway: '', dnsServer: '' },
  mining: {
    optimizationMode: 2,
    minFanSpeedPercent: 100,
    maxHotChipTempC: 105,
    rebootIntervalMins: 60 * 8,
    rebootMinHashrate: 150,
    disableGeneticAlgo: false,
  },
}

// Make groups hierarchical (each group can have a parent name)
const cfg: Config = {
  groups: {
    SC1: {
      system: { timezone: 'America/New_York' },
      network: { hostname: '{serial}' },
      mining: { rebootMinHashrate: 125 },
      pools: [
        {
          url: 'stratum+tcp://sc-us.luxor.tech:3333',
          worker:
            '80572bd701d24bbc09aab060ca6daec7af4cf1cda8a9cc44d29fe1232718fd00cbb123456789.{serial}',
          password: 'x1',
        },
        {
          url: 'stratum+tcp://sc-eu.luxor.tech:3333',
          worker:
            '80572bd701d24bbc09aab060ca6daec7af4cf1cda8a9cc44d29fe1232718fd00cbb123456789.{serial}',
          password: 'x1',
        },
        {
          url: 'stratum+tcp://us-west.siamining.com:3333',
          worker:
            '80572bd701d24bbc09aab060ca6daec7af4cf1cda8a9cc44d29fe1232718fd00cbb123456789.{serial}',
          password: 'x1',
        },
      ],
    },
  },
  miners: [
    { address: '10.120.154.172', serial: 'SC1-001213', group: 'SC1' },
    { address: '10.120.154.173', serial: 'SC1-001214', group: 'SC1' },

  ],
}

function getMergedConfig(minerConfig: MinerConfig, baseConfig: Config) {
  let mergedConfig: MinerConfig = _.merge(defaultConfig, {})
  const groupConfig = minerConfig.group ? baseConfig.groups[minerConfig.group] : undefined
  if (groupConfig) {
    console.log('Merging group for ' + minerConfig.group)
    mergedConfig = _.merge(mergedConfig, groupConfig)
  }

  mergedConfig = _.merge(mergedConfig, minerConfig)

  // Now do substitutions - TODO: Make this generic
  if (mergedConfig.network) {
    mergedConfig.network.hostname = format(mergedConfig.network.hostname || '', {
      serial: mergedConfig.serial,
    })
  }
  if (mergedConfig.pools) {
    for (const pool of mergedConfig.pools) {
      if (pool) {
        pool.worker = format(pool.worker || '', {
          serial: mergedConfig.serial,
        })
      }
    }
  }

  return mergedConfig
}

// Read config file
function getConfig(): Config {
  return cfg
  // const contents = fs.readFileSync('ob.config', 'utf8')
  // const emptyConfig = { groups: {}, miners: [] }
  // if (!contents) {
  //   return emptyConfig
  // }

  // return JSON.parse(contents) || emptyConfig
}

// Login
async function login(address: string, username: string, password: string): Promise<boolean> {
  const request: any = axios
  const result = await request.post(
    `http://${address}/api/login`,
    { username, password },
    {
      jar: cookieJar,
      withCredentials: true,
    },
  )
  if (result.status === 200) {
    console.log(`status=${result.status}`)
    return true
  }
  return false
}

async function setPoolsConfig(
  address: string | undefined,
  poolsConfig: PoolConfig[],
): Promise<boolean> {
  if (!address) {
    console.log('ERROR: Unable to set pools config.  No address provided.')
    return false
  }
  const request: any = axios
  await request.post(`http://${address}/api/config/pools`, poolsConfig, {
    jar: cookieJar,
    withCredentials: true,
  })
  return true
}

async function setMiningConfig(
  address: string | undefined,
  minerConfig: MiningConfig,
): Promise<boolean> {
  if (!address) {
    console.log('ERROR: Unable to set mining config.  No address provided.')
    return false
  }
  const request: any = axios
  await request.post(`http://${address}/api/config/mining`, minerConfig, {
    jar: cookieJar,
    withCredentials: true,
  })
  return true
}

async function setNetworkConfig(
  address: string | undefined,
  networkConfig: NetworkConfig,
): Promise<boolean> {
  if (!address) {
    console.log('ERROR: Unable to set pools config.  No address provided.')
    return false
  }
  const request: any = axios
  try {
    await request.post(`http://${address}/api/config/network`, networkConfig, {
      jar: cookieJar,
      withCredentials: true,
      timeout: 1000,
    })
    return true
  } catch (err) {
    console.log(err)
    return false
  }
}

async function setSystemConfig(
  address: string | undefined,
  systemConfig: SystemConfig,
): Promise<boolean> {
  if (!address) {
    console.log('ERROR: Unable to set pools config.  No address provided.')
    return false
  }
  const request: any = axios
  await request.post(`http://${address}/api/config/system`, systemConfig, {
    jar: cookieJar,
    withCredentials: true,
  })
  return true
}

// Go through and merge the configurations for each miner, then set the configs
async function updateMiners(config: any) {
  for (const miner of config.miners) {
    const mergedConfig = getMergedConfig(miner, config)
    console.log('mergedConfig=' + JSON.stringify(mergedConfig, null, 2))

    if (!mergedConfig.address || !mergedConfig.username || !mergedConfig.password) {
      continue
    }

    if (!(await login(mergedConfig.address, mergedConfig.username, mergedConfig.password))) {
      console.log(`ERROR: Unable to login to ${mergedConfig.address}`)
      continue
    }
    console.log(`Logged in successfully to ${mergedConfig.address}`)

    // We are logged in, so send the configs if present
    // TODO: Check to see if we need to check if the objects have all required fields (i.e., does the apiserver care if some or all fields are missing?)
    if (mergedConfig.pools) {
      console.log('Setting pools config')
      await setPoolsConfig(mergedConfig.address, mergedConfig.pools)
    }

    if (mergedConfig.mining) {
      console.log('Setting mining config')
      await setMiningConfig(mergedConfig.address, mergedConfig.mining)
    }

    if (mergedConfig.system) {
      console.log('Setting system config')
      await setSystemConfig(mergedConfig.address, mergedConfig.system)
    }

    // NOTE: We do this one last, because it will cause a reboot
    // TODO: Add a flag to this API endpoint so it does not reboot (or just require a separate reboot API call after)
    if (mergedConfig.network) {
      console.log(
        `Setting network config: address=${mergedConfig.address} network=${JSON.stringify(
          mergedConfig.network,
          null,
          2,
        )}`,
      )
      await setNetworkConfig(mergedConfig.address, mergedConfig.network)
    }
  }
}

const config = getConfig()
updateMiners(config)
