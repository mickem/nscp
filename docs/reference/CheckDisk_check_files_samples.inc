Order is somewhat important but mainly in the fact that some operations are more costly then others.
For instance line_count requires us to read and count the lines in each file so choosing between the following:
Fast version::

   filter=create < -2d and line_count > 100

Show version::

   filter=line_count > 100 and create < -2d

The first one will be significantly faster if you have a thousand old files and 3 new ones.
But looking at the following::

   filter=create < -2d and size > 100k

Swapping them would not be noticeable.


Checking file versions::

   check_files path=D:/tmp pattern=*.exe "filter=version != 1.0" "syntax=%filename%: %version%" MaxWarn=1
   WARNING:explorer.exe: 6.0.2900.5512, explorer.exe: 1 > warning|'explorer.exe'=1;0;0;

Checking file versions using exact bound::

   check_files path=D:/tmp pattern=*.exe "filter=version != 1.0" "syntax=%filename%: %version%" warn=gt:1 crit==1
   WARNING:explorer.exe: 6.0.2900.5512, explorer.exe: 1 > warning|'explorer.exe'=1;0;0;

Using the line count with limited recursion::

   check_files path=D:\windows pattern=*.txt max-dir-depth=1 "filter=line_count gt 100" "syntax=%filename%: %line_count%" MaxWarn=1
   WARNING:test.txt: 3, test.txt: 1 > warning|'test.txt'=1;0;0;

Check file sizes::

   check_files path=D:\tmp pattern=*.txt "filter=size gt 20" "syntax=%filename%: %size%" MaxWarn=1
   WARNING:test.txt: 26B, found files: 1 > warning|'found files'=1;0;0;
