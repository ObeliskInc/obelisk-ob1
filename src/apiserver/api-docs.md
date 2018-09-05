# API Docs

The following are simple docs for a few APIs that will be useful to colos.  Proper docs for the full API will follow soon.

## Unauthenticated Enpoints

### Info
Retrieve some basic info about the unit.  This endpoint does not require authentication.

**Request Type:** GET

**URL:** `/api/info`

**Response:**
```
{
    "macAddress": "80:00:27:2e:bc:69",
    "ipAddress": "192.168.1.123",
    "model": "SC1/DCR1"   // The plan is to make the dynamic
    "vendor": "Obelisk"
  }
```

## IP Discovery
The miners have a front button that can be pressed to send out an mDNS packet.  Software exists that can listen for these messages, or you can write your own.  Once an IP address has been discovered, you can send the `/api/info` command to get more info on the device or use `/api/login`, and then send any API requests yo uwish.

## Authentication

### Login
Login the user to create a session.  Upon successful authentication, a `sessionid` cookie is added to the response.  Make sure to include that cookie value with all future requests until you issue an `/api/logout` request.

**Request Type:** POST

**URL:** `/api/Login`

**JSON Request Body:**
```
  {
    "username": "admin",
    "password": "test"
  }
```

**Response:**
```
  200 OK
  401 Unauthorized
```

### Logout
Log the current user out.  The session will become invalid.

**Request Type:** POST

**URL:** `/api/logout`

**Response:**
```
  200 OK
```


## Get/Set Requests

### Get Inventory/Config/Status
Get various information from the miner.  The <path> in the URL below can be replaced with one of the following:

#### Inventory
Inventory endpoints return information about the device and system itself (number of hashboards inserted, version numbers,
board revisions, etc.).

* `inventory/versions`
* `inventory/devices`
* `inventory/system`

#### Config
Config endpoints return information about the current configuration of the system.  These are user-modifiable settings.

* `config/config`
* `config/pools`
* `config/network`
* `config/system`
* `config/mining`

#### Status
Status endpoints return information about the current state of the system.  These are read-only values.

* `status/dashboard`
* `status/memory`

**Request Type:** GET

**URL:** `/api/<path>`

**Response:**
```
TBD: Various responses
```

### Set Config
Set the configurable parameters of the miner.  The <path> can be replaced with one of the following:

* `config/pools`
* `config/network`
* `config/system`
* `config/mining`

**Request Type:** POST

**URL:** `/api/<path>`

**JSON Request Body:**
```
TBD: The adventurous can try the corresponding GET requests above
to see the format of the responses.  The request body of the corresponding
POST will be the same as the GET response.
```

**Response:**
```
<various responses TBD>
```

## Actions


### Reboot

Reboot the miner.

**Request Type:** POST

**URL:** `/api/action/reboot`

**Response:**
```
  200 OK
  401 Unauthorized - if not logged in
```

### Change Password
Change the user's password.

**Request Type:** POST

**JSON Request Body:**
```
  {
    "username": "admin",
    "oldPassword": "hello",
    "newPassword": "world"
  }
```

**Response:**
```
  200 OK
  401 Unuathorized - not logged in or oldPassword is incorrect
```

