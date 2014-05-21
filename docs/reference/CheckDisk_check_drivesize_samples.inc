To check the size of **the C:\ drive** and **make sure it has atleast 10% free** space::

   check_drivesize "crit=free<10%" drive=c:
   L     client CRITICAL: c:: 205GB/223GB used
   L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100

To check the size of **all the drives** and make sure it has atleast 10% free space::

   check_drivesize "crit=free<10%" drive=*
   L     client OK: All drives ok
   L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100

To check the size of all the drives and **display all values, not just problems**::

   check_drivesize drive=* --show-all
   L     client CRITICAL: c:: 205GB/223GB used
   L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100


To check the size of **a mounted volume** (c:\volumne_test) and **make sure it has 1M free** space **warn if free space is less then 10M**::

   check_drivesize "crit=free<1M" "warn=free<10M" drive=c:\\volumne_test
   C:: Total: 74.5G - Used: 71.2G (95%) - Free: 3.28G (5%) < critical,C:;5%;10;5;

To check the size of **all volumes** and **make sure they have 1M space free**::

   check_drivesize "crit=free<1M" drive=all-volumes
   L     client OK: All drives ok
   L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'E:\ free'=0B;0;0;0;0 'F:\ free'=0B;0;0;0;0

To check the size of **all fixed and network drives** and make sure they have at least 1gig free space::

   check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')"
   L     client OK: All drives ok
   L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100


To check **all fixed and network drives but ignore C and F**::

   check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')" exclude=C:\\ exclude=D:\\
   L     client OK: All drives ok
   L     client  Performance data: 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100

To checking **UNC Paths**. Please note that I need to use \\ to escape the back-slashes from check_nrpe you can escape using ' instead which is simpler::

   check_drivesize drive=\\\\medin-ds\\data\\ "crit=free<10%"
   L     client CRITICAL: \\medin-ds\data\: 2.6TB/2.68TB used
   L     client  Performance data: '\\medin-ds\data\ free'=83GB;0;274;0;2746 '\\medin-ds\data\ free %'=3%;0;9;0;100

.. important:: 
   Please note that UNC paths are only available in each session meaning a user mounted share will not be visible to NSClient++ (since services run in their own session). 
   But as long as NSClient++ can access the share it still works as long as you specify the UNC path.
   In other words the following will **NOT** work::

      check_drivesize drive=m:

   But the following will::

      check_drivesize drive=\\myserver\\mydrive
   

.. important:: 
   Do not forget the trailing \.

Default via NRPE::

	check_nrpe --host 192.168.56.103 --command check_drivesize
	C:\: 205GB/223GB used, D:\: 448GB/466GB used, M:\: 2.6TB/2.68TB used|'C:\ used'=204GB;44;22;0;223 'C:\ used %'=91%;19;9;0;100 'D:\ used'=447GB;93;46;0;465...

