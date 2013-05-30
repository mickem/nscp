Syntax
******

The syntax is much like SQL where clauses for details on the syntax see the [wiki:filters] page.

Order
*****

Order is somewhat important but mainly in the fact that some operations are more costly then others.
For instance line_count requires us to read and count the lines in each file so choosing between the following:
.. code-block:: bat

   filter=create < -2d and line_count > 100

.. code-block:: bat

   filter=line_count > 100 and create < -2d

The first one will be significantly faster if you have a thousand old files and 3 new ones.
But looking at the following:

.. code-block:: bat

   filter=create < -2d and size > 100k

Swapping them would not be noticeable.

Filter Variables
****************

================= ================= ============================================================
<filter variable> Values            Description
================= ================= ============================================================
size              number            The size of the file
creation          time_expression   The date/time the file was created
written           time_expression   The date/time the file was last updated
access            time_expression   The date/time the file was last accessed
version           string_expression The exe file version
line_count        number            Number of lines (in a text file)
================= ================= ============================================================

time expression
***************

A '''time expression''' is a date/time interval as a number followed by a unit postfix (m, s, h, d, w). A few examples of time expression are: generated < -2d translates to older then 2 days where as generated > -2h means match any records newer then 2 hours. '''Warning''', the bash shell on linux interprets the "<,>,!". Use the "\" to avoid this. e.g. generated=\>2d and activate the nasty meta chars option. The new method avoids problems with escape characters and nasty meta chars. Replace > with gt and < with lt, e. g.: generated lt -2d.

Minus versus no minus:
 * Affects "what you compare to"
 * -15m will create a comparison point which is "15 minutes ago"
 * 15m will create a comparison point which is 15 minutes in the future.

And gt versus lt:
 * Affects "how you compare" (older versus newer)
 * less then means "older then"
 * greater then means "newer then"

In essence the "times" are converted to numbers representing the number of seconds (or microseconds or some such) (in UTC BTW) since <whatever>. So small number means "old file" large number means "new file".


string expression
*****************

A '''string expression''' is a key followed by a string that specifies a string expression. Example: version = '1.0.0.0'


Checking file versions
**********************

.. code-block:: bat

   check_files path=D:/tmp pattern=*.exe "filter=version != 1.0" "syntax=%filename%: %version%" MaxWarn=1
   WARNING:explorer.exe: 6.0.2900.5512, explorer.exe: 1 > warning|'explorer.exe'=1;0;0;

Checking file versions using exact bound
****************************************

.. code-block:: bat

   check_files path=D:/tmp pattern=*.exe "filter=version != 1.0" "syntax=%filename%: %version%" warn=gt:1 crit==1
   WARNING:explorer.exe: 6.0.2900.5512, explorer.exe: 1 > warning|'explorer.exe'=1;0;0;

Using the line count with limited recursion
*******************************************

.. code-block:: bat

   check_files path=D:\windows pattern=*.txt max-dir-depth=1 "filter=line_count gt 100" "syntax=%filename%: %line_count%" MaxWarn=1
   WARNING:test.txt: 3, test.txt: 1 > warning|'test.txt'=1;0;0;

Check file sizes
****************

.. code-block:: bat

   check_files path=D:\tmp pattern=*.txt "filter=size gt 20" "syntax=%filename%: %size%" MaxWarn=1
   WARNING:test.txt: 26B, found files: 1 > warning|'found files'=1;0;0;
