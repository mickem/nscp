`CheckDisk` is provides two disk related checks one for checking size of drives and the other for checking status of files and folders.

!!! danger
    Please note that UNC and network paths are only available in each session meaning a user mounted share will not be visible to NSClient++ (since services run in their own session).
    But as long as NSClient++ can access the share you can still check it as you specify the UNC path.
    In other words the following will **NOT** work: `check_drivesize drive=m:` But the following will: `check_drivesize drive=\\myserver\\mydrive`
