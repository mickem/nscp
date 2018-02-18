#### Performance

Order is somewhat important but mainly in the fact that some operations are more costly then others.
For instance line_count requires us to read and count the lines in each file so choosing between the following:
Fast version: `filter=creation < -2d and line_count > 100`

Slow version: `filter=line_count > 100 and creation < -2d`

The first one will be significantly faster if you have a thousand old files and 3 new ones.

On the other hand in this example `filter=creation < -2d and size > 100k` swapping them would not be noticeable.

#### Checking versions of .exe files

```
check_files path=c:/foo/ pattern=*.exe "filter=version != '1.0'" "detail-syntax=%(filename): %(version)" "warn=count > 1" show-all
L        cli WARNING: WARNING: 0/11 files (check_nrpe.exe: , nscp.exe: 0.5.0.16, reporter.exe: 0.5.0.16)
L        cli  Performance data: 'count'=11;1;0
```

#### Using the line count with limited recursion:

```
check_files path=c:/windows pattern=*.txt max-depth=1 "filter=line_count gt 100" "detail-syntax=%(filename): %(line_count)" "warn=count>0" show-all
L        cli WARNING: WARNING: 0/1 files (AsChkDev.txt: 328)
L        cli  Performance data: 'count'=1;0;0
```

#### Check file sizes

```
check_files path=c:/windows pattern=*.txt "detail-syntax=%(filename): %(size)" "warn=size>20k" max-depth=1
L        cli WARNING: WARNING: 1/6 files (AsChkDev.txt: 29738)
L        cli  Performance data: 'AsChkDev.txt size'=29.04101KB;20;0 'AsDCDVer.txt size'=0.02246KB;20;0 'AsHDIVer.txt size'=0.02734KB;20;0 'AsPEToolVer.txt size'=0.08789KB;20;0 'AsToolCDVer.txt size'=0.05273KB;20;0 'csup.txt size'=0.00976KB;20;0
```
