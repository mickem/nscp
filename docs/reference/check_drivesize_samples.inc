Check C:
********

Check the size of C:\ and make sure it has 10% free space:

.. code-block:: bat

   check_drivesize ShowAll MinWarnFree=10% MinCritFree=5% Drive=c:
   CRITICAL C:: Total: 74.5G - Used: 71.2G (95%) - Free: 3.28G (5%) < critical,C:;5%;10;5;

Volumes 1
*********

To check the size of mounted volume c:\volumne_test and make sure it has 1M free space

.. code-block:: bat

   check_drivesize ShowAll MaxWarn=1M MaxCrit=2M Drive=c:\volumne_test
   C:: Total: 74.5G - Used: 71.2G (95%) - Free: 3.28G (5%) < critical,C:;5%;10;5;

Volumes 2
*********

To check the size of all volume has 1M free space

.. code-block:: bat

   check_drivesize ShowAll MaxWarn=1M MaxCrit=2M CheckAll=volumes
   CRITICAL C:: Total: 74.5G - Used: 71.2G (95%) - Free: 3.28G (5%) < critical,C:;5%;10;5;

All fixed and network disks
***************************

To check the size of all fixed and network drives and make sure they have at least 1gig free space

.. code-block:: bat

   check_drivesize MinWarn=50% MinCrit=25% CheckAll FilterType=FIXED FilterType=REMOTE
   CRITICAL,C:\;76514398208;1073741824;536870912; D:\;199303897088;1073741824;536870912; X:\;35467034624;1073741824;536870912; Y:\;299649466368;1073741824;536870912;

Fixed and Network (ignore some)
*******************************

Check all fixed and network drives but ignore C and F.
Not sure about this (should be simpler ways)

.. code-block:: bat

   check_drivesize,CheckAllOthers FilterType=FIXED FilterType=REMOTE MinWarn=25% MinCrit=50% Drive=C Drive=F

Checking UNC Paths
******************

.. important:: 
   Do not forget the trailing \.

.. code-block:: bat

   check_drivesize,Drive=x:\ FilterType=REMOTE ShowAll MaxWarnUsed=90% MaxCritUsed=95%

Simple Config
*************

Another example for a working config.

.. code-block:: bat

    check_drivesize,Drive=c:\\volumes\\somevolume\\ ShowAll MaxWarnUsed=90% MaxCritUsed=95%
