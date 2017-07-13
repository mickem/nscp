## Description

`CheckExternalScripts` is used to run scripts and programs you provide your self as opposed to internal commands provided by modules and internal scripts. You can also fond many third part generated scripts at various sites:

*   [Nagios Exchange](https://exchange.nagios.org/)
*   [Icinga Exchange](https://exchange.icinga.com/)

To use this module you need to enable it like so:

```
nscp settings --activate-module CheckExternalScripts
```

Which will add the following to your configuration:

```
[/modules]
CheckExternalScripts = enabled
```

There is an extensive guide on using external scripts with NSClient++ [here](../../howto/external_scripts.md) as well as some examples in the [samples section](#samples) of this page.
