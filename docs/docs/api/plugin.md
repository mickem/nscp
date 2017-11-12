
# API Reference


<a name=".Plugin.Common"/>
## Common

`Plugin.Common` Common utility types (re-used in various messages below)

### ResultCode

A "Nagios" status result.

Possible values | Value | Description                           
--------------- | ----- | --------------------------------------
OK              | 0     | An ok status in Nagios                
WARNING         | 1     | A warning status in Nagios            
CRITICAL        | 2     | A critical status in Nagios           
UNKNOWN         | 3     | Not able to determine status in Nagios


### DataType

Type of data fields.
@deprecated in favor of checking which field is available.

Possible values | Value | Description                           
--------------- | ----- | --------------------------------------
INT             | 1     | An ok status in Nagios                
STRING          | 2     | A warning status in Nagios            
FLOAT           | 3     | A critical status in Nagios           
BOOL            | 4     | Not able to determine status in Nagios
LIST            | 5     |                                       



<a name=".Plugin.Common.AnyDataType"/>
### AnyDataType

`Plugin.Common.AnyDataType` Data type used to wrap "any" primitive type.
Used whenever the type can change.

Modifier | Type   | Key         | Description                                            
-------- | ------ | ----------- | -------------------------------------------------------
optional | string | string_data | String payload                                         
optional | int64  | int_data    | Numeric integer payload                                
optional | double | float_data  | Numeric floating point payload                         
optional | bool   | bool_data   | Boolean (true/false) payload                           
repeated | string | list_data   | A string (multiple lines are separated by list entries)

<a name=".Plugin.Common.KeyValue"/>
### KeyValue

`Plugin.Common.KeyValue` Key value pair

Modifier | Type   | Key   | Description                  
-------- | ------ | ----- | -----------------------------
required | string | key   | The key identifying the value
required | string | value | The value                    

<a name=".Plugin.Common.Host"/>
### Host

`Plugin.Common.Host` Field identifying a host entry

Modifier | Type     | Key      | Description                                                
-------- | -------- | -------- | -----------------------------------------------------------
optional | string   | id       | A unique identifier representing the host in *this* message
optional | string   | host     | The host name                                              
optional | string   | address  | The address                                                
optional | string   | protocol | The protocol used to talk whit this host.                  
optional | string   | comment  | A comment describing the host                              
repeated | KeyValue | metadata | A key value store with attributes describing this host.    
repeated | string   | tags     | A number of tags defined for this host                     

<a name=".Plugin.Common.Header"/>
### Header

`Plugin.Common.Header` A common header used in all messages.
Contains basic information about the message.

Modifier | Type     | Key            | Description                                  
-------- | -------- | -------------- | ---------------------------------------------
optional | string   | command        | Command.                                     
optional | string   | source_id      | Source (sending) system.                     
optional | string   | sender_id      | Sender is the original source of the message.
optional | string   | recipient_id   | Recipient is the final destination.          
optional | string   | destination_id | Destination (target) system.                 
optional | string   | message_id     | Message identification.                      
repeated | KeyValue | metadata       | Meta data related to the message.            
repeated | string   | tags           | A list of tags associated with the message.  
repeated | Host     | hosts          | A list of hosts.                             

<a name=".Plugin.Common.PerformanceData"/>
### PerformanceData

`Plugin.Common.PerformanceData` How metrics are encoded into check results
Nagios calls this performance data and we inherited the name.

Modifier | Type        | Key          | Description                                              
-------- | ----------- | ------------ | ---------------------------------------------------------
required | string      | alias        | The name of the value                                    
optional | IntValue    | int_value    | If the value is an integer (can be only one)             
optional | StringValue | string_value | If the value is a string (can be only one)               
optional | FloatValue  | float_value  | If the value is a floating point number (can be only one)
optional | BoolValue   | bool_value   | If the value is a boolean (can be only one)              

<a name=".Plugin.Common.PerformanceData.IntValue"/>
#### IntValue

`Plugin.Common.PerformanceData.IntValue` Numeric performance data

Modifier | Type   | Key      | Description                         
-------- | ------ | -------- | ------------------------------------
required | int64  | value    | The value we are tracking           
optional | string | unit     | The unit of all the values          
optional | int64  | warning  | The warning threshold (if available)
optional | int64  | critical | The warning critical (if available) 
optional | int64  | minimum  | The lowest possible value           
optional | int64  | maximum  | The highest possible value          

<a name=".Plugin.Common.PerformanceData.StringValue"/>
#### StringValue

`Plugin.Common.PerformanceData.StringValue` Textual performance data

Modifier | Type   | Key   | Description
-------- | ------ | ----- | -----------
required | string | value | The value  

<a name=".Plugin.Common.PerformanceData.FloatValue"/>
#### FloatValue

`Plugin.Common.PerformanceData.FloatValue` Floating point performance data

Modifier | Type   | Key      | Description                         
-------- | ------ | -------- | ------------------------------------
required | double | value    | The value we are tracking           
optional | string | unit     | The unit of all the values          
optional | double | warning  | The warning threshold (if available)
optional | double | critical | The warning critical (if available) 
optional | double | minimum  | The lowest possible value           
optional | double | maximum  | The highest possible value          

<a name=".Plugin.Common.PerformanceData.BoolValue"/>
#### BoolValue

`Plugin.Common.PerformanceData.BoolValue` Boolean performance data

Modifier | Type   | Key      | Description                         
-------- | ------ | -------- | ------------------------------------
required | bool   | value    | The value we are tracking           
optional | string | unit     | The unit of all the values          
optional | bool   | warning  | The warning threshold (if available)
optional | bool   | critical | The warning critical (if available) 

<a name=".Plugin.Common.Metric"/>
### Metric

`Plugin.Common.Metric` 
Modifier | Type        | Key   | Description
-------- | ----------- | ----- | -----------
required | string      | key   |            
required | AnyDataType | value |            
optional | string      | alias |            
optional | string      | desc  |            

<a name=".Plugin.Common.MetricsBundle"/>
### MetricsBundle

`Plugin.Common.MetricsBundle` 
Modifier | Type          | Key      | Description
-------- | ------------- | -------- | -----------
required | string        | key      |            
repeated | Metric        | value    |            
repeated | MetricsBundle | children |            
optional | string        | alias    |            
optional | string        | desc     |            

<a name=".Plugin.Common.Result"/>
### Result

`Plugin.Common.Result` 
### StatusCodeType


Possible values | Value | Description
--------------- | ----- | -----------
STATUS_OK       | 0     |            
STATUS_WARNING  | 1     |            
STATUS_ERROR    | 2     |            
STATUS_DELAYED  | 3     |            


Modifier | Type           | Key     | Description
-------- | -------------- | ------- | -----------
required | StatusCodeType | code    |            
optional | string         | message |            
optional | string         | data    |            



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
Modifier | Type   | Key  | Description
-------- | ------ | ---- | -----------
required | string | path |            
optional | string | key  |            

<a name=".Plugin.Settings.Query"/>
### Query

`Plugin.Settings.Query` 
Modifier | Type   | Key        | Description
-------- | ------ | ---------- | -----------
optional | string | expression |            

<a name=".Plugin.Settings.Information"/>
### Information

`Plugin.Settings.Information` 
Modifier | Type        | Key           | Description
-------- | ----------- | ------------- | -----------
optional | string      | title         |            
optional | string      | description   |            
optional | string      | icon          |            
optional | AnyDataType | default_value |            
optional | string      | min_version   |            
optional | string      | max_version   |            
optional | bool        | advanced      |            
optional | bool        | sample        |            
optional | bool        | is_template   |            
optional | string      | sample_usage  |            
repeated | string      | plugin        |            
optional | bool        | subkey        |            



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
Modifier | Type        | Key           | Description
-------- | ----------- | ------------- | -----------
optional | Node        | node          |            
optional | Query       | query         |            
optional | bool        | recursive     |            
optional | DataType    | type          |            
optional | AnyDataType | default_value |            

<a name=".Plugin.SettingsRequestMessage.Request.Update"/>
#### Update

`Plugin.SettingsRequestMessage.Request.Update` 
Modifier | Type        | Key   | Description
-------- | ----------- | ----- | -----------
optional | Node        | node  |            
optional | AnyDataType | value |            

<a name=".Plugin.SettingsRequestMessage.Request.Inventory"/>
#### Inventory

`Plugin.SettingsRequestMessage.Request.Inventory` 
Modifier | Type   | Key             | Description
-------- | ------ | --------------- | -----------
optional | string | plugin          |            
optional | Node   | node            |            
optional | Query  | query           |            
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
Modifier | Type        | Key   | Description
-------- | ----------- | ----- | -----------
required | Node        | node  |            
optional | AnyDataType | value |            

<a name=".Plugin.SettingsResponseMessage.Response.Update"/>
#### Update

`Plugin.SettingsResponseMessage.Response.Update` 

<a name=".Plugin.SettingsResponseMessage.Response.Inventory"/>
#### Inventory

`Plugin.SettingsResponseMessage.Response.Inventory` 
Modifier | Type        | Key   | Description
-------- | ----------- | ----- | -----------
required | Node        | node  |            
required | Information | info  |            
optional | AnyDataType | value |            

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



<a name=".Plugin.MetricsQueryMessage"/>
## MetricsQueryMessage

`Plugin.MetricsQueryMessage` Metrics message
Used for fetching and sending metrics.

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Plugin.MetricsQueryMessage.Request"/>
### Request

`Plugin.MetricsQueryMessage.Request` 
Modifier | Type   | Key  | Description
-------- | ------ | ---- | -----------
optional | int64  | id   |            
optional | string | type |            



<a name=".Plugin.MetricsMessage"/>
## MetricsMessage

`Plugin.MetricsMessage` Metrics message
Used for fetching and sending metrics.

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  |            
repeated | Response | payload |            

<a name=".Plugin.MetricsMessage.Response"/>
### Response

`Plugin.MetricsMessage.Response` 
Modifier | Type          | Key     | Description
-------- | ------------- | ------- | -----------
optional | int64         | id      |            
required | Result        | result  |            
repeated | MetricsBundle | bundles |            


