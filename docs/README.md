# NSClient++ documentation

## Index

- Manual:
  - [NSClient++](docs/index.md)
  - [Getting Started](docs/getting_started.md)
  - [Theory](docs/theory.md)
  - [Checking Things](docs/checks.md)
  - [Installing NSClient++](docs/installing.md)
  - [Configuration](docs/settings.md)
  - [FAQ](docs/faq.md)
- About:
  - [Documentation](docs/about/index.md)
  - [Whats New](docs/)
    - [0.4.0](docs/about/whatsnew/0.4.0.md)
    - [0.4.1](docs/about/whatsnew/0.4.1.md)
    - [0.4.2](docs/about/whatsnew/0.4.2.md)
    - [0.4.3](docs/about/whatsnew/0.4.3.md)
    - [0.4.4](docs/about/whatsnew/0.4.4.md)
    - [0.5.0](docs/about/whatsnew/0.5.0.md)
  - [Building NSClient++](docs/about/build.md)
  - [Reporting Bugs](docs/about/bugs.md)
  - [Copyright](docs/about/copyright.md)
  - [License](docs/about/license.md)
- Tutorial: tutorial/index.md
- Howto:
  - [Checking things](docs/howto/checks.md)
  - [Performance Counters](docs/howto/counters.md)
  - [External scripts](docs/howto/external_scripts.md)
  - [Running commands](docs/howto/run_commands.md)
  - [NRPE](docs/howto/nrpe.md)
  - [NSCA](docs/howto/nsca.md)
  - [Migrate from 0.3](docs/howto/03x_migration.md)
- Reference:
  - [Reference](docs/reference/index.md)
  - Check commands:
    - [CheckDisk (Windows)](docs/reference/windows/CheckDisk.md)
    - [CheckExternalScripts](docs/reference/check/CheckExternalScripts.md)
    - [CheckEventLog (Windows)](docs/reference/windows/CheckEventLog.md)
    - [CheckHelpers](docs/reference/check/CheckHelpers.md)
    - [CheckLogFile](docs/reference/check/CheckLogFile.md)
    - [CheckMKClient](docs/reference/check/CheckMKClient.md)
    - [CheckMKServer](docs/reference/check/CheckMKServer.md)
    - [CheckNet](docs/reference/misc/CheckNet.md)
    - [CheckNSCP](docs/reference/check/CheckNSCP.md)
    - [CheckSystemUnix (unix)](docs/reference/unix/CheckSystemUnix.md)
    - [CheckSystem (Windows)](docs/reference/windows/CheckSystem.md)
    - [CheckTaskSched (Windows)](docs/reference/windows/CheckTaskSched.md)
    - [CheckWMI (Windows)](docs/reference/windows/CheckWMI.md)
  - Clients & Servers:
    - [WEBServer](docs/reference/generic/WEBServer.md)
    - [WEBClient](docs/reference/misc/WEBClient.md)
    - [NRPEServer](docs/reference/client/NRPEServer.md)
    - [NRPEClient](docs/reference/client/NRPEClient.md)
    - [NSClientServer](docs/reference/windows/NSClientServer.md)
    - [NSCAServer](docs/reference/client/NSCAServer.md)
    - [NSCAClient](docs/reference/client/NSCAClient.md)
    - [NRDPClient](docs/reference/client/NRDPClient.md)
    - [SMTPClient](docs/reference/client/SMTPClient.md)
    - [GraphiteClient](docs/reference/client/GraphiteClient.md)
    - [SyslogClient](docs/reference/client/SyslogClient.md)
    - [CollectdClient](docs/reference/misc/CollectdClient.md)
  - Helper modules:
    - [Scheduler](docs/reference/generic/Scheduler.md)
    - [SimpleFileWriter](docs/reference/generic/SimpleFileWriter.md)
    - [CommandClient](docs/reference/generic/CommandClient.md)
    - [SimpleCache](docs/reference/generic/SimpleCache.md)
    - [PythonScript](docs/reference/generic/PythonScript.md)
    - [LUAScript](docs/reference/generic/LUAScript.md)
    - [DotnetPlugins](docs/reference/windows/DotnetPlugins.md)
  - API:
    - [API](docs/api/index.md)
    - [Plugin](docs/api/plugin.md)

## About documentation

The NSClient++ documentation is written in GitHub flavored [Markdown](https://guides.github.com/features/mastering-markdown/).
It is located in the `docs/` directory and can be edited with your preferred editor. You can also
edit it online on GitHub.

```
vim docs/installing.md
```

In order to review and test changes, you can install the [mkdocs](http://www.mkdocs.org) Python library.

```
pip install mkdocs
```

Note: Use at least 0.16, 0.14 has this [bug](https://github.com/mkdocs/mkdocs/issues/1213).

This allows you to start a local mkdocs viewer instance on http://localhost:8000

```
mkdocs serve
```

Changes on the chapter layout can be done inside the `mkdocs.yml` file in the main tree.

