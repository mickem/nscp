
# API Reference


<a name=".Plugin.QueryRequestMessage"/>
## QueryRequestMessage

`Plugin.QueryRequestMessage` Query request
Used for querying the client this is the "normal" check_nrpe message request.
Associated response is :py:class:`Plugin.QueryResponseMessage`

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Plugin.QueryRequestMessage.Request"/>
### Request

`Plugin.QueryRequestMessage.Request` 

Modifier | Type   | Key       | Description
-------- | ------ | --------- | -----------
optional | int32  | id        |            
optional | string | target    |            
required | string | command   |            
optional | string | alias     |            
repeated | string | arguments |            



<a name=".Plugin.QueryResponseMessage"/>
## QueryResponseMessage

`Plugin.QueryResponseMessage` Query response
Used for querying the client this is the "normal" check_nrpe message request.
Associated request is `[QueryRequestMessage](#.Plugin.QueryRequestMessage)`

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  |            
repeated | Response | payload |            

<a name=".Plugin.QueryResponseMessage.Response"/>
### Response

`Plugin.QueryResponseMessage.Response` 

Modifier | Type       | Key       | Description
-------- | ---------- | --------- | -----------
optional | int32      | id        |            
optional | string     | source    |            
optional | string     | command   |            
optional | string     | alias     |            
repeated | string     | arguments |            
required | ResultCode | result    |            
repeated | Line       | lines     |            
optional | bytes      | data      |            

<a name=".Plugin.QueryResponseMessage.Response.Line"/>
#### Line

`Plugin.QueryResponseMessage.Response.Line` 

Modifier | Type            | Key     | Description
-------- | --------------- | ------- | -----------
required | string          | message |            
repeated | PerformanceData | perf    |            



<a name=".Plugin.ExecuteRequestMessage"/>
## ExecuteRequestMessage

`Plugin.ExecuteRequestMessage` Execute command request and response.
Used for executing commands on clients similar to :py:class:`Plugin.QueryRequestMessage` but wont return Nagios check data
Associated response is :py:class:`Plugin.ExecuteResponseMessage`

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Plugin.ExecuteRequestMessage.Request"/>
### Request

`Plugin.ExecuteRequestMessage.Request` 

Modifier | Type   | Key       | Description
-------- | ------ | --------- | -----------
optional | int32  | id        |            
required | string | command   |            
repeated | string | arguments |            



<a name=".Plugin.ExecuteResponseMessage"/>
## ExecuteResponseMessage

`Plugin.ExecuteResponseMessage` 

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  |            
repeated | Response | payload |            

<a name=".Plugin.ExecuteResponseMessage.Response"/>
### Response

`Plugin.ExecuteResponseMessage.Response` 

Modifier | Type       | Key       | Description
-------- | ---------- | --------- | -----------
optional | int32      | id        |            
required | string     | command   |            
repeated | string     | arguments |            
required | ResultCode | result    |            
required | string     | message   |            
optional | bytes      | data      |            



<a name=".Plugin.SubmitRequestMessage"/>
## SubmitRequestMessage

`Plugin.SubmitRequestMessage` Submit result request message.
Used for submitting a passive check results.
The actual payload (Request) is a normal :py:class:`Plugin.QueryResponseMessage.Response`.
Associated response is :py:class:`Plugin.SubmitResponseMessage`

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  | The header 
required | string   | channel |            
repeated | Response | payload |            



<a name=".Plugin.SubmitResponseMessage"/>
## SubmitResponseMessage

`Plugin.SubmitResponseMessage` Submit result response message.
Response from submitting a passive check results.
Associated request is :py:class:`Plugin.SubmitRequestMessage`

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  |            
repeated | Response | payload |            

<a name=".Plugin.SubmitResponseMessage.Response"/>
### Response

`Plugin.SubmitResponseMessage.Response` 

Modifier | Type   | Key     | Description
-------- | ------ | ------- | -----------
optional | int32  | id      |            
required | string | command |            
required | Result | result  |            



<a name=".Plugin.EventMessage"/>
## EventMessage

`Plugin.EventMessage` Execute command request and response.
Used for executing commands on clients similar to :py:class:`Plugin.QueryRequestMessage` but wont return Nagios check data
Associated response is :py:class:`Plugin.ExecuteResponseMessage`

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Plugin.EventMessage.Request"/>
### Request

`Plugin.EventMessage.Request` 

Modifier | Type     | Key       | Description
-------- | -------- | --------- | -----------
optional | string   | event     |            
repeated | string   | arguments |            
repeated | KeyValue | data      |            



<a name=".Plugin.Registry"/>
## Registry

`Plugin.Registry` Registration is an internal message.
It is not used to submit checks or query status instead it is used so register modules, plug-ins, command.
As well as query for them.
The registry is a central component inside NSClient++ and this is the way to interact with the registry.

### ItemType


Possible values | Value | Description
--------------- | ----- | -----------
QUERY           | 1     |            
COMMAND         | 2     |            
HANDLER         | 3     |            
PLUGIN          | 4     |            
QUERY_ALIAS     | 5     |            
ROUTER          | 6     |            
MODULE          | 7     |            
SCHEDULE        | 8     |            
EVENT           | 9     |            
ALL             | 99    |            


### Command


Possible values | Value | Description
--------------- | ----- | -----------
LOAD            | 1     |            
UNLOAD          | 2     |            
RELOAD          | 3     |            
ENABLE          | 4     |            
DISABLE         | 5     |            



<a name=".Plugin.Registry.Query"/>
### Query

`Plugin.Registry.Query` 

Modifier | Type   | Key        | Description
-------- | ------ | ---------- | -----------
optional | string | expression |            

<a name=".Plugin.Registry.Information"/>
### Information

`Plugin.Registry.Information` 

Modifier | Type     | Key         | Description
-------- | -------- | ----------- | -----------
optional | string   | title       |            
optional | string   | description |            
repeated | KeyValue | metadata    |            
optional | string   | min_version |            
optional | string   | max_version |            
optional | bool     | advanced    |            
repeated | string   | plugin      |            

<a name=".Plugin.Registry.KeyWordDescription"/>
### KeyWordDescription

`Plugin.Registry.KeyWordDescription` 

Modifier | Type   | Key               | Description
-------- | ------ | ----------------- | -----------
required | string | parameter         |            
optional | string | context           |            
required | string | key               |            
optional | string | short_description |            
optional | string | long_description  |            

<a name=".Plugin.Registry.ParameterDetail"/>
### ParameterDetail

`Plugin.Registry.ParameterDetail` 

Modifier | Type               | Key               | Description
-------- | ------------------ | ----------------- | -----------
optional | string             | name              |            
optional | string             | default_value     |            
optional | bool               | required          |            
optional | bool               | repeatable        |            
optional | DataType           | content_type      |            
optional | string             | short_description |            
optional | string             | long_description  |            
repeated | KeyWordDescription | keyword           |            

<a name=".Plugin.Registry.FieldDetail"/>
### FieldDetail

`Plugin.Registry.FieldDetail` 

Modifier | Type   | Key               | Description
-------- | ------ | ----------------- | -----------
optional | string | name              |            
optional | string | short_description |            
optional | string | long_description  |            

<a name=".Plugin.Registry.ParameterDetails"/>
### ParameterDetails

`Plugin.Registry.ParameterDetails` 

Modifier | Type            | Key       | Description
-------- | --------------- | --------- | -----------
repeated | ParameterDetail | parameter |            
repeated | FieldDetail     | fields    |            

<a name=".Plugin.Registry.Schedule"/>
### Schedule

`Plugin.Registry.Schedule` 

Modifier | Type   | Key      | Description
-------- | ------ | -------- | -----------
optional | string | id       |            
optional | string | cron     |            
optional | string | interval |            



<a name=".Plugin.RegistryRequestMessage"/>
## RegistryRequestMessage

`Plugin.RegistryRequestMessage` 

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Plugin.RegistryRequestMessage.Request"/>
### Request

`Plugin.RegistryRequestMessage.Request` 

Modifier | Type         | Key          | Description
-------- | ------------ | ------------ | -----------
optional | int64        | id           |            
optional | Registration | registration |            
optional | Inventory    | inventory    |            
optional | Control      | control      |            

<a name=".Plugin.RegistryRequestMessage.Request.Registration"/>
#### Registration

`Plugin.RegistryRequestMessage.Request.Registration` 

Modifier | Type        | Key        | Description
-------- | ----------- | ---------- | -----------
optional | int32       | plugin_id  |            
required | ItemType    | type       |            
required | string      | name       |            
optional | Information | info       |            
optional | bool        | unregister |            
repeated | string      | alias      |            
repeated | Schedule    | schedule   |            

<a name=".Plugin.RegistryRequestMessage.Request.Inventory"/>
#### Inventory

`Plugin.RegistryRequestMessage.Request.Inventory` 

Modifier | Type     | Key               | Description
-------- | -------- | ----------------- | -----------
optional | string   | plugin            |            
repeated | ItemType | type              |            
optional | string   | name              |            
optional | bool     | fetch_all         |            
optional | bool     | fetch_information |            

<a name=".Plugin.RegistryRequestMessage.Request.Control"/>
#### Control

`Plugin.RegistryRequestMessage.Request.Control` 

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
required | Command  | command |            
required | ItemType | type    |            
optional | string   | name    |            
optional | string   | alias   |            



<a name=".Plugin.RegistryResponseMessage"/>
## RegistryResponseMessage

`Plugin.RegistryResponseMessage` 

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  |            
repeated | Response | payload |            

<a name=".Plugin.RegistryResponseMessage.Response"/>
### Response

`Plugin.RegistryResponseMessage.Response` 

Modifier | Type         | Key          | Description
-------- | ------------ | ------------ | -----------
optional | int64        | id           |            
required | Result       | result       |            
optional | Registration | registration |            
repeated | Inventory    | inventory    |            
optional | Control      | control      |            

<a name=".Plugin.RegistryResponseMessage.Response.Registration"/>
#### Registration

`Plugin.RegistryResponseMessage.Response.Registration` 

Modifier | Type  | Key     | Description
-------- | ----- | ------- | -----------
optional | int32 | item_id |            

<a name=".Plugin.RegistryResponseMessage.Response.Inventory"/>
#### Inventory

`Plugin.RegistryResponseMessage.Response.Inventory` 

Modifier | Type             | Key        | Description
-------- | ---------------- | ---------- | -----------
repeated | string           | plugin     |            
required | ItemType         | type       |            
required | string           | name       |            
optional | string           | id         |            
optional | Information      | info       |            
optional | ParameterDetails | parameters |            
repeated | Schedule         | schedule   |            

<a name=".Plugin.RegistryResponseMessage.Response.Control"/>
#### Control

`Plugin.RegistryResponseMessage.Response.Control` 




<a name=".Plugin.ScheduleNotificationMessage"/>
## ScheduleNotificationMessage

`Plugin.ScheduleNotificationMessage` Schedule Notification commands
Used when a schedule is executed

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Plugin.ScheduleNotificationMessage.Request"/>
### Request

`Plugin.ScheduleNotificationMessage.Request` A request message of a schule notification

Modifier | Type        | Key       | Description
-------- | ----------- | --------- | -----------
optional | int64       | id        |            
required | int32       | plugin_id |            
optional | Information | info      |            
optional | Schedule    | schedule  |            



<a name=".Plugin.Settings"/>
## Settings

`Plugin.Settings` Settings is an internal message.
It is not used to submit checks or query status instead it is used to interact with the settings store.
The settings is a central component inside NSClient++ and this is the way to interact with it.

### Command


Possible values | Value | Description
--------------- | ----- | -----------
LOAD            | 1     |            
SAVE            | 2     |            
RELOAD          | 3     |            



<a name=".Plugin.Settings.Node"/>
### Node

`Plugin.Settings.Node` 

Modifier | Type   | Key   | Description
-------- | ------ | ----- | -----------
required | string | path  |            
optional | string | key   |            
optional | string | value |            

<a name=".Plugin.Settings.Information"/>
### Information

`Plugin.Settings.Information` 

Modifier | Type   | Key           | Description
-------- | ------ | ------------- | -----------
optional | string | title         |            
optional | string | description   |            
optional | string | icon          |            
optional | string | default_value |            
optional | bool   | advanced      |            
optional | bool   | sample        |            
optional | bool   | is_template   |            
optional | string | sample_usage  |            
repeated | string | plugin        |            
optional | bool   | subkey        |            



<a name=".Plugin.SettingsRequestMessage"/>
## SettingsRequestMessage

`Plugin.SettingsRequestMessage` 

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Plugin.SettingsRequestMessage.Request"/>
### Request

`Plugin.SettingsRequestMessage.Request` 

Modifier | Type         | Key          | Description
-------- | ------------ | ------------ | -----------
optional | int64        | id           |            
required | int32        | plugin_id    |            
optional | Registration | registration |            
optional | Query        | query        |            
optional | Update       | update       |            
optional | Inventory    | inventory    |            
optional | Control      | control      |            
optional | Status       | status       |            

<a name=".Plugin.SettingsRequestMessage.Request.Registration"/>
#### Registration

`Plugin.SettingsRequestMessage.Request.Registration` 

Modifier | Type        | Key    | Description
-------- | ----------- | ------ | -----------
optional | Node        | node   |            
optional | Information | info   |            
optional | string      | fields |            

<a name=".Plugin.SettingsRequestMessage.Request.Query"/>
#### Query

`Plugin.SettingsRequestMessage.Request.Query` 

Modifier | Type   | Key           | Description
-------- | ------ | ------------- | -----------
optional | Node   | node          |            
optional | bool   | recursive     |            
optional | bool   | include_keys  |            
optional | string | default_value |            

<a name=".Plugin.SettingsRequestMessage.Request.Update"/>
#### Update

`Plugin.SettingsRequestMessage.Request.Update` 

Modifier | Type | Key  | Description
-------- | ---- | ---- | -----------
optional | Node | node |            

<a name=".Plugin.SettingsRequestMessage.Request.Inventory"/>
#### Inventory

`Plugin.SettingsRequestMessage.Request.Inventory` 

Modifier | Type   | Key             | Description
-------- | ------ | --------------- | -----------
optional | string | plugin          |            
optional | Node   | node            |            
optional | bool   | recursive_fetch |            
optional | bool   | fetch_keys      |            
optional | bool   | fetch_paths     |            
optional | bool   | fetch_samples   |            
optional | bool   | fetch_templates |            
optional | bool   | descriptions    |            

<a name=".Plugin.SettingsRequestMessage.Request.Control"/>
#### Control

`Plugin.SettingsRequestMessage.Request.Control` 

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
required | Command | command |            
optional | string  | context |            

<a name=".Plugin.SettingsRequestMessage.Request.Status"/>
#### Status

`Plugin.SettingsRequestMessage.Request.Status` 




<a name=".Plugin.SettingsResponseMessage"/>
## SettingsResponseMessage

`Plugin.SettingsResponseMessage` 

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  |            
repeated | Response | payload |            

<a name=".Plugin.SettingsResponseMessage.Response"/>
### Response

`Plugin.SettingsResponseMessage.Response` 

Modifier | Type         | Key          | Description
-------- | ------------ | ------------ | -----------
optional | int64        | id           |            
required | Result       | result       |            
optional | Registration | registration |            
optional | Query        | query        |            
optional | Update       | update       |            
repeated | Inventory    | inventory    |            
optional | Control      | control      |            
optional | Status       | status       |            

<a name=".Plugin.SettingsResponseMessage.Response.Registration"/>
#### Registration

`Plugin.SettingsResponseMessage.Response.Registration` 


<a name=".Plugin.SettingsResponseMessage.Response.Query"/>
#### Query

`Plugin.SettingsResponseMessage.Response.Query` 

Modifier | Type | Key   | Description
-------- | ---- | ----- | -----------
required | Node | node  |            
repeated | Node | nodes |            

<a name=".Plugin.SettingsResponseMessage.Response.Update"/>
#### Update

`Plugin.SettingsResponseMessage.Response.Update` 


<a name=".Plugin.SettingsResponseMessage.Response.Inventory"/>
#### Inventory

`Plugin.SettingsResponseMessage.Response.Inventory` 

Modifier | Type        | Key  | Description
-------- | ----------- | ---- | -----------
required | Node        | node |            
required | Information | info |            

<a name=".Plugin.SettingsResponseMessage.Response.Control"/>
#### Control

`Plugin.SettingsResponseMessage.Response.Control` 


<a name=".Plugin.SettingsResponseMessage.Response.Status"/>
#### Status

`Plugin.SettingsResponseMessage.Response.Status` 

Modifier | Type   | Key         | Description
-------- | ------ | ----------- | -----------
optional | string | context     |            
optional | string | type        |            
optional | bool   | has_changed |            



<a name=".Plugin.LogEntry"/>
## LogEntry

`Plugin.LogEntry` LogEntry is used to log status information.

Modifier | Type  | Key   | Description
-------- | ----- | ----- | -----------
repeated | Entry | entry |            

<a name=".Plugin.LogEntry.Entry"/>
### Entry

`Plugin.LogEntry.Entry` 

### Level


Possible values | Value | Description
--------------- | ----- | -----------
LOG_TRACE       | 1000  |            
LOG_DEBUG       | 500   |            
LOG_INFO        | 150   |            
LOG_WARNING     | 50    |            
LOG_ERROR       | 10    |            
LOG_CRITICAL    | 1     |            


Modifier | Type   | Key     | Description
-------- | ------ | ------- | -----------
required | Level  | level   |            
optional | string | sender  |            
optional | string | file    |            
optional | int32  | line    |            
optional | string | message |            
optional | int32  | date    |            



<a name=".Plugin.Storage"/>
## Storage

`Plugin.Storage` Storage Structure Disk


<a name=".Plugin.Storage.Entry"/>
### Entry

`Plugin.Storage.Entry` 

Modifier | Type        | Key          | Description
-------- | ----------- | ------------ | -----------
optional | string      | context      |            
optional | string      | key          |            
optional | AnyDataType | value        |            
optional | bool        | binary_data  |            
optional | bool        | private_data |            

<a name=".Plugin.Storage.Block"/>
### Block

`Plugin.Storage.Block` 

Modifier | Type   | Key     | Description
-------- | ------ | ------- | -----------
optional | string | owner   |            
optional | int64  | version |            
optional | Entry  | entry   |            

<a name=".Plugin.Storage.File"/>
### File

`Plugin.Storage.File` 

Modifier | Type  | Key     | Description
-------- | ----- | ------- | -----------
optional | int64 | version |            
optional | int64 | entries |            



<a name=".Plugin.StorageRequestMessage"/>
## StorageRequestMessage

`Plugin.StorageRequestMessage` Storeage Request Message
Used to save/load data in the NSClient++ local storage

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
repeated | Request | payload |            

<a name=".Plugin.StorageRequestMessage.Request"/>
### Request

`Plugin.StorageRequestMessage.Request` 

Modifier | Type  | Key       | Description
-------- | ----- | --------- | -----------
optional | int64 | id        |            
optional | int32 | plugin_id |            
optional | Put   | put       |            
optional | Get   | get       |            

<a name=".Plugin.StorageRequestMessage.Request.Put"/>
#### Put

`Plugin.StorageRequestMessage.Request.Put` 

Modifier | Type  | Key   | Description
-------- | ----- | ----- | -----------
optional | Entry | entry |            

<a name=".Plugin.StorageRequestMessage.Request.Get"/>
#### Get

`Plugin.StorageRequestMessage.Request.Get` 

Modifier | Type   | Key     | Description
-------- | ------ | ------- | -----------
optional | string | context |            
optional | string | key     |            



<a name=".Plugin.StorageResponseMessage"/>
## StorageResponseMessage

`Plugin.StorageResponseMessage` Storeage Response Message
Used to save/load data in the NSClient++ local storage

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
repeated | Response | payload |            

<a name=".Plugin.StorageResponseMessage.Response"/>
### Response

`Plugin.StorageResponseMessage.Response` 

Modifier | Type   | Key    | Description
-------- | ------ | ------ | -----------
optional | int64  | id     |            
optional | Result | result |            
optional | Put    | put    |            
optional | Get    | get    |            

<a name=".Plugin.StorageResponseMessage.Response.Put"/>
#### Put

`Plugin.StorageResponseMessage.Response.Put` 

Modifier | Type   | Key     | Description
-------- | ------ | ------- | -----------
optional | string | context |            
optional | string | key     |            
optional | string | error   |            

<a name=".Plugin.StorageResponseMessage.Response.Get"/>
#### Get

`Plugin.StorageResponseMessage.Response.Get` 

Modifier | Type  | Key   | Description
-------- | ----- | ----- | -----------
repeated | Entry | entry |            


