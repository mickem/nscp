`filter_perf` while badly named can be used to prost process performance data.

It can be useful for sorting performance data or limiting the number of performance data items shown.

In its most basic form you can run `filter_perf command=COMMAND arguments REGULAR ARGUMENTS` for example `check_process`:
```
filter_perf command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: ' ws_size'=0GB;3;5 ' ws_size'=0GB;3;5 ' ws_size'=0GB;3;5 ' ...
```

This will not do an anything by itself but we can for instance dort performance data entries by adding `sort=normal`:
```
filter_perf sort=normal command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: 'clion64.exe ws_size'=3.30851GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 ...
```

And further can also limit the number of results shown by adding `limit=5` like so:
```
filter_perf sort=normal limit=5 command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: 'clion64.exe ws_size'=3.30852GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 'msedge.exe ws_size'=0.5757GB;3;5
```
