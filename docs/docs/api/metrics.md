
# API Reference


<a name=".Metrics.Metric"/>
## Metric

`Metrics.Metric` 

Modifier | Type        | Key   | Description
-------- | ----------- | ----- | -----------
required | string      | key   |            
required | AnyDataType | value |            
optional | string      | alias |            
optional | string      | desc  |            



<a name=".Metrics.MetricsBundle"/>
## MetricsBundle

`Metrics.MetricsBundle` 

Modifier | Type          | Key      | Description
-------- | ------------- | -------- | -----------
required | string        | key      |            
repeated | Metric        | value    |            
repeated | MetricsBundle | children |            
optional | string        | alias    |            
optional | string        | desc     |            



<a name=".Metrics.MetricsQueryMessage"/>
## MetricsQueryMessage

`Metrics.MetricsQueryMessage` Metrics message
Used for fetching and sending metrics.

Modifier | Type    | Key     | Description
-------- | ------- | ------- | -----------
optional | Header  | header  |            
repeated | Request | payload |            

<a name=".Metrics.MetricsQueryMessage.Request"/>
### Request

`Metrics.MetricsQueryMessage.Request` 

Modifier | Type   | Key  | Description
-------- | ------ | ---- | -----------
optional | int64  | id   |            
optional | string | type |            



<a name=".Metrics.MetricsMessage"/>
## MetricsMessage

`Metrics.MetricsMessage` Metrics message
Used for fetching and sending metrics.

Modifier | Type     | Key     | Description
-------- | -------- | ------- | -----------
optional | Header   | header  |            
repeated | Response | payload |            

<a name=".Metrics.MetricsMessage.Response"/>
### Response

`Metrics.MetricsMessage.Response` 

Modifier | Type          | Key     | Description
-------- | ------------- | ------- | -----------
optional | int64         | id      |            
required | Result        | result  |            
repeated | MetricsBundle | bundles |            


