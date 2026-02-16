# PythonScript

Loads and processes internal Python scripts



## Enable module

To enable this module and and allow using the commands you need to ass `PythonScript = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
PythonScript = enabled
```




## Configuration



| Path / Section                              | Description    |
|---------------------------------------------|----------------|
| [/settings/python](#)                       |                |
| [/settings/python/scripts](#python-scripts) | Python scripts |



### /settings/python <a id="/settings/python"></a>






| Key                           | Default Value | Description  |
|-------------------------------|---------------|--------------|
| [python cache](#python-cache) |               | Python cache |



```ini
# 
[/settings/python]

```





#### Python cache <a id="/settings/python/python cache"></a>

Override python cache folder.






| Key            | Description                           |
|----------------|---------------------------------------|
| Path:          | [/settings/python](#/settings/python) |
| Key:           | python cache                          |
| Default value: | _N/A_                                 |
| Used by:       | PythonScript                          |


**Sample:**

```
[/settings/python]
# Python cache
python cache=
```


### Python scripts <a id="/settings/python/scripts"></a>

A list of scripts available to run from the PythonScript module.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






