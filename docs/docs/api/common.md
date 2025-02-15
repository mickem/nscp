
# API Reference


<a name=".Common.AnyDataType"/>
## AnyDataType

`Common.AnyDataType` Data type used to wrap "any" primitive type.
Used whenever the type can change.

Modifier | Type   | Key         | Description                                            
-------- | ------ | ----------- | -------------------------------------------------------
optional | string | string_data | String payload                                         
optional | int64  | int_data    | Numeric integer payload                                
optional | double | float_data  | Numeric floating point payload                         
optional | bool   | bool_data   | Boolean (true/false) payload                           
repeated | string | list_data   | A string (multiple lines are separated by list entries)



<a name=".Common.KeyValue"/>
## KeyValue

`Common.KeyValue` Key value pair

Modifier | Type   | Key   | Description                  
-------- | ------ | ----- | -----------------------------
required | string | key   | The key identifying the value
required | string | value | The value                    



<a name=".Common.Host"/>
## Host

`Common.Host` Field identifying a host entry

Modifier | Type     | Key      | Description                                                
-------- | -------- | -------- | -----------------------------------------------------------
optional | string   | id       | A unique identifier representing the host in *this* message
optional | string   | host     | The host name                                              
optional | string   | address  | The address                                                
optional | string   | protocol | The protocol used to talk whit this host.                  
optional | string   | comment  | A comment describing the host                              
repeated | KeyValue | metadata | A key value store with attributes describing this host.    
repeated | string   | tags     | A number of tags defined for this host                     



<a name=".Common.Header"/>
## Header

`Common.Header` A common header used in all messages.
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



<a name=".Common.PerformanceData"/>
## PerformanceData

`Common.PerformanceData` How metrics are encoded into check results
Nagios calls this performance data and we inherited the name.

Modifier | Type        | Key          | Description                                              
-------- | ----------- | ------------ | ---------------------------------------------------------
required | string      | alias        | The name of the value                                    
optional | IntValue    | int_value    | If the value is an integer (can be only one)             
optional | StringValue | string_value | If the value is a string (can be only one)               
optional | FloatValue  | float_value  | If the value is a floating point number (can be only one)
optional | BoolValue   | bool_value   | If the value is a boolean (can be only one)              

<a name=".Common.PerformanceData.IntValue"/>
### IntValue

`Common.PerformanceData.IntValue` Numeric performance data

Modifier | Type   | Key      | Description                         
-------- | ------ | -------- | ------------------------------------
required | int64  | value    | The value we are tracking           
optional | string | unit     | The unit of all the values          
optional | int64  | warning  | The warning threshold (if available)
optional | int64  | critical | The warning critical (if available) 
optional | int64  | minimum  | The lowest possible value           
optional | int64  | maximum  | The highest possible value          

<a name=".Common.PerformanceData.StringValue"/>
### StringValue

`Common.PerformanceData.StringValue` Textual performance data

Modifier | Type   | Key   | Description
-------- | ------ | ----- | -----------
required | string | value | The value  

<a name=".Common.PerformanceData.FloatValue"/>
### FloatValue

`Common.PerformanceData.FloatValue` Floating point performance data

Modifier | Type   | Key      | Description                         
-------- | ------ | -------- | ------------------------------------
required | double | value    | The value we are tracking           
optional | string | unit     | The unit of all the values          
optional | double | warning  | The warning threshold (if available)
optional | double | critical | The warning critical (if available) 
optional | double | minimum  | The lowest possible value           
optional | double | maximum  | The highest possible value          

<a name=".Common.PerformanceData.BoolValue"/>
### BoolValue

`Common.PerformanceData.BoolValue` Boolean performance data

Modifier | Type   | Key      | Description                         
-------- | ------ | -------- | ------------------------------------
required | bool   | value    | The value we are tracking           
optional | string | unit     | The unit of all the values          
optional | bool   | warning  | The warning threshold (if available)
optional | bool   | critical | The warning critical (if available) 



<a name=".Common.Result"/>
## Result

`Common.Result` 

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


