"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = y[op[0] & 2 ? "return" : op[0] ? "throw" : "next"]) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [0, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
};
Object.defineProperty(exports, "__esModule", { value: true });
// Copyright 2018 Ken Carpenter
var axios_1 = require("axios");
// import fs = require('fs')
var format = require("string-template");
var _ = require("lodash");
axios_1.default.defaults.withCredentials = true;
var axiosCookieJarSupport = require('axios-cookiejar-support').default;
var tough = require('tough-cookie');
axiosCookieJarSupport(axios_1.default);
var cookieJar = new tough.CookieJar();
var defaultConfig = {
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
};
// Make groups hierarchical (each group can have a parent name)
var cfg = {
    groups: {
        SC1: {
            system: { timezone: 'America/New_York' },
            network: { hostname: '{serial}' },
            mining: { rebootMinHashrate: 125 },
            pools: [
                {
                    url: 'stratum+tcp://sc-us.luxor.tech:3333',
                    worker: '80572bd701d24bbc09aab060ca6daec7af4cf1cda8a9cc44d29fe1232718fd00cbb3b931db67.{serial}',
                    password: 'x1',
                },
                {
                    url: 'stratum+tcp://sc-eu.luxor.tech:3333',
                    worker: '80572bd701d24bbc09aab060ca6daec7af4cf1cda8a9cc44d29fe1232718fd00cbb3b931db67.{serial}',
                    password: 'x1',
                },
                {
                    url: 'stratum+tcp://us-west.siamining.com:3333',
                    worker: '80572bd701d24bbc09aab060ca6daec7af4cf1cda8a9cc44d29fe1232718fd00cbb3b931db67.{serial}',
                    password: 'x1',
                },
            ],
        },
        DCR1: {
            system: { timezone: 'America/New_York' },
            network: { hostname: '{serial}' },
            mining: { rebootMinHashrate: 300 },
            pools: [
                {
                    url: 'stratum+tcp://dcr-us.luxor.tech:4445',
                    worker: 'DsooQv1cbjj5xwQxoDA5MXYUgaXXKN8DEiN.{serial}',
                    password: 'x',
                },
                {
                    url: 'stratum+tcp://dcr-eu.luxor.tech:4445',
                    worker: 'DsooQv1cbjj5xwQxoDA5MXYUgaXXKN8DEiN.{serial}',
                    password: 'x',
                },
                {
                    url: 'stratum+tcp://dcr-asia.luxor.tech:4445',
                    worker: 'DsooQv1cbjj5xwQxoDA5MXYUgaXXKN8DEiN.{serial}',
                    password: 'x1',
                },
            ],
        },
    },
    miners: [
        { address: '10.120.150.72', serial: 'SC1-002412', group: 'SC1' },
        { address: '10.120.150.73', serial: 'SC1-002375', group: 'SC1' },
        { address: '10.120.150.77', serial: 'SC1-002415', group: 'SC1' },
        { address: '10.120.150.78', serial: 'SC1-002385', group: 'SC1' },
        { address: '10.120.150.79', serial: 'SC1-002394', group: 'SC1' },
        { address: '10.120.150.80', serial: 'SC1-002392', group: 'SC1' },
        { address: '10.120.150.81', serial: 'SC1-002377', group: 'SC1' },
        { address: '10.120.150.82', serial: 'SC1-002397', group: 'SC1' },
        { address: '10.120.150.83', serial: 'SC1-002420', group: 'SC1' },
        { address: '10.120.150.84', serial: 'SC1-002405', group: 'SC1' },
        { address: '10.120.150.85', serial: 'SC1-002403', group: 'SC1' },
        { address: '10.120.150.86', serial: 'SC1-002384', group: 'SC1' },
        { address: '10.120.150.87', serial: 'SC1-002409', group: 'SC1' },
        { address: '10.120.150.88', serial: 'SC1-002393', group: 'SC1' },
        { address: '10.120.150.89', serial: 'SC1-002411', group: 'SC1' },
        { address: '10.120.150.90', serial: 'SC1-002404', group: 'SC1' },
        { address: '10.120.150.91', serial: 'SC1-002383', group: 'SC1' },
        { address: '10.120.150.92', serial: 'SC1-002386', group: 'SC1' },
        { address: '10.120.150.93', serial: 'SC1-002391', group: 'SC1' },
        { address: '10.120.150.94', serial: 'SC1-002398', group: 'SC1' },
        { address: '10.120.150.95', serial: 'SC1-002400', group: 'SC1' },
        { address: '10.120.150.96', serial: 'SC1-002390', group: 'SC1' },
        { address: '10.120.150.97', serial: 'SC1-002369', group: 'SC1' },
        { address: '10.120.150.98', serial: 'SC1-002374', group: 'SC1' },
        { address: '10.120.150.99', serial: 'SC1-002416', group: 'SC1' },
        { address: '10.120.150.100', serial: 'SC1-002380', group: 'SC1' },
        { address: '10.120.150.101', serial: 'SC1-002402', group: 'SC1' },
        { address: '10.120.150.102', serial: 'SC1-002401', group: 'SC1' },
        { address: '10.120.150.103', serial: 'SC1-002406', group: 'SC1' },
        { address: '10.120.150.104', serial: 'SC1-002388', group: 'SC1' },
        { address: '10.120.150.105', serial: 'SC1-002365', group: 'SC1' },
        { address: '10.120.150.106', serial: 'SC1-002408', group: 'SC1' },
        { address: '10.120.150.107', serial: 'SC1-002389', group: 'SC1' },
        { address: '10.120.150.108', serial: 'SC1-002387', group: 'SC1' },
        { address: '10.120.150.109', serial: 'SC1-002379', group: 'SC1' },
        { address: '10.120.150.110', serial: 'SC1-002407', group: 'SC1' },
        { address: '10.120.150.111', serial: 'SC1-002399', group: 'SC1' },
        { address: '10.120.150.112', serial: 'SC1-002414', group: 'SC1' },
        { address: '10.120.150.113', serial: 'SC1-002418', group: 'SC1' },
        { address: '10.120.150.114', serial: 'SC1-002417', group: 'SC1' },
        { address: '10.120.150.115', serial: 'SC1-002410', group: 'SC1' },
        { address: '10.120.150.116', serial: 'SC1-002370', group: 'SC1' },
        { address: '10.120.150.117', serial: 'SC1-002413', group: 'SC1' },
        { address: '10.120.150.118', serial: 'SC1-002419', group: 'SC1' },
        { address: '10.120.150.119', serial: 'SC1-002366', group: 'SC1' },
    ],
};
function getMergedConfig(minerConfig, baseConfig) {
    var mergedConfig = _.merge(defaultConfig, {});
    var groupConfig = minerConfig.group ? baseConfig.groups[minerConfig.group] : undefined;
    if (groupConfig) {
        console.log('Merging group for ' + minerConfig.group);
        mergedConfig = _.merge(mergedConfig, groupConfig);
    }
    mergedConfig = _.merge(mergedConfig, minerConfig);
    // Now do substitutions - TODO: Make this generic
    if (mergedConfig.network) {
        mergedConfig.network.hostname = format(mergedConfig.network.hostname || '', {
            serial: mergedConfig.serial,
        });
    }
    if (mergedConfig.pools) {
        for (var _i = 0, _a = mergedConfig.pools; _i < _a.length; _i++) {
            var pool = _a[_i];
            if (pool) {
                pool.worker = format(pool.worker || '', {
                    serial: mergedConfig.serial,
                });
            }
        }
    }
    return mergedConfig;
}
// Read config file
function getConfig() {
    return cfg;
    // const contents = fs.readFileSync('ob.config', 'utf8')
    // const emptyConfig = { groups: {}, miners: [] }
    // if (!contents) {
    //   return emptyConfig
    // }
    // return JSON.parse(contents) || emptyConfig
}
// Login
function login(address, username, password) {
    return __awaiter(this, void 0, void 0, function () {
        var request, result;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    request = axios_1.default;
                    return [4 /*yield*/, request.post("http://" + address + "/api/login", { username: username, password: password }, {
                            jar: cookieJar,
                            withCredentials: true,
                        })];
                case 1:
                    result = _a.sent();
                    if (result.status === 200) {
                        console.log("status=" + result.status);
                        return [2 /*return*/, true];
                    }
                    return [2 /*return*/, false];
            }
        });
    });
}
function setPoolsConfig(address, poolsConfig) {
    return __awaiter(this, void 0, void 0, function () {
        var request;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    if (!address) {
                        console.log('ERROR: Unable to set pools config.  No address provided.');
                        return [2 /*return*/, false];
                    }
                    request = axios_1.default;
                    return [4 /*yield*/, request.post("http://" + address + "/api/config/pools", poolsConfig, {
                            jar: cookieJar,
                            withCredentials: true,
                        })];
                case 1:
                    _a.sent();
                    return [2 /*return*/, true];
            }
        });
    });
}
function setMiningConfig(address, minerConfig) {
    return __awaiter(this, void 0, void 0, function () {
        var request;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    if (!address) {
                        console.log('ERROR: Unable to set mining config.  No address provided.');
                        return [2 /*return*/, false];
                    }
                    request = axios_1.default;
                    return [4 /*yield*/, request.post("http://" + address + "/api/config/mining", minerConfig, {
                            jar: cookieJar,
                            withCredentials: true,
                        })];
                case 1:
                    _a.sent();
                    return [2 /*return*/, true];
            }
        });
    });
}
function setNetworkConfig(address, networkConfig) {
    return __awaiter(this, void 0, void 0, function () {
        var request, err_1;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    if (!address) {
                        console.log('ERROR: Unable to set pools config.  No address provided.');
                        return [2 /*return*/, false];
                    }
                    request = axios_1.default;
                    _a.label = 1;
                case 1:
                    _a.trys.push([1, 3, , 4]);
                    return [4 /*yield*/, request.post("http://" + address + "/api/config/network", networkConfig, {
                            jar: cookieJar,
                            withCredentials: true,
                            timeout: 1000,
                        })];
                case 2:
                    _a.sent();
                    return [2 /*return*/, true];
                case 3:
                    err_1 = _a.sent();
                    console.log(err_1);
                    return [2 /*return*/, false];
                case 4: return [2 /*return*/];
            }
        });
    });
}
function setSystemConfig(address, systemConfig) {
    return __awaiter(this, void 0, void 0, function () {
        var request;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    if (!address) {
                        console.log('ERROR: Unable to set pools config.  No address provided.');
                        return [2 /*return*/, false];
                    }
                    request = axios_1.default;
                    return [4 /*yield*/, request.post("http://" + address + "/api/config/system", systemConfig, {
                            jar: cookieJar,
                            withCredentials: true,
                        })];
                case 1:
                    _a.sent();
                    return [2 /*return*/, true];
            }
        });
    });
}
// Go through and merge the configurations for each miner, then set the configs
function updateMiners(config) {
    return __awaiter(this, void 0, void 0, function () {
        var _i, _a, miner, mergedConfig;
        return __generator(this, function (_b) {
            switch (_b.label) {
                case 0:
                    _i = 0, _a = config.miners;
                    _b.label = 1;
                case 1:
                    if (!(_i < _a.length)) return [3 /*break*/, 11];
                    miner = _a[_i];
                    mergedConfig = getMergedConfig(miner, config);
                    console.log('mergedConfig=' + JSON.stringify(mergedConfig, null, 2));
                    if (!mergedConfig.address || !mergedConfig.username || !mergedConfig.password) {
                        return [3 /*break*/, 10];
                    }
                    return [4 /*yield*/, login(mergedConfig.address, mergedConfig.username, mergedConfig.password)];
                case 2:
                    if (!(_b.sent())) {
                        console.log("ERROR: Unable to login to " + mergedConfig.address);
                        return [3 /*break*/, 10];
                    }
                    console.log("Logged in successfully to " + mergedConfig.address);
                    if (!mergedConfig.pools) return [3 /*break*/, 4];
                    console.log('Setting pools config');
                    return [4 /*yield*/, setPoolsConfig(mergedConfig.address, mergedConfig.pools)];
                case 3:
                    _b.sent();
                    _b.label = 4;
                case 4:
                    if (!mergedConfig.mining) return [3 /*break*/, 6];
                    console.log('Setting mining config');
                    return [4 /*yield*/, setMiningConfig(mergedConfig.address, mergedConfig.mining)];
                case 5:
                    _b.sent();
                    _b.label = 6;
                case 6:
                    if (!mergedConfig.system) return [3 /*break*/, 8];
                    console.log('Setting system config');
                    return [4 /*yield*/, setSystemConfig(mergedConfig.address, mergedConfig.system)];
                case 7:
                    _b.sent();
                    _b.label = 8;
                case 8:
                    if (!mergedConfig.network) return [3 /*break*/, 10];
                    console.log("Setting network config: address=" + mergedConfig.address + " network=" + JSON.stringify(mergedConfig.network, null, 2));
                    return [4 /*yield*/, setNetworkConfig(mergedConfig.address, mergedConfig.network)];
                case 9:
                    _b.sent();
                    _b.label = 10;
                case 10:
                    _i++;
                    return [3 /*break*/, 1];
                case 11: return [2 /*return*/];
            }
        });
    });
}
var config = getConfig();
updateMiners(config);
//# sourceMappingURL=obconfig.js.map