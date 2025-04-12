# Securing NSClient++

## Protocols

NSClient++ supports a multitude of protocols thus securing the server will depend on the protocol you are using.

## NRPE

> TODO: Add example of configuring certificates

## NRDP

> TODO: Add example of configuring TLS

## Http(s)/TLS

> TODO: Add example of configuring TLS
> TODO: Add example of configuring permissions

## User account

By default, NSClient++ is running as `local system` which is a easy as it is always available.
A more secure approach is to use a dedicated user account.
This user account should be a local user account with only relevant admin right given the things you check.

> TODO: Add example of creating a user account and setting up NSClient++ to use it.
> TODO: Add example of permissions needed for various checks.

## Passwords

NSClient++ has among other secrets an admin password which out-of-the box is stored in a config file.
This is insecure and not recommended.
There are two simple way to solve this:
1. Store the config file in the profile of the user.
2. Store secrets in credential manager.

### Storing config in user profile

Default the service is using the `local system` account (if you are using a different account please update the paths in below example).
To move the config file to `C:\Windows\System32\Config\systemprofile\NSClient++`.

First create the folder:
```commandline
PsExec -i -s cmd /c mkdir "C:\Windows\System32\Config\systemprofile\NSClient++"
```

Then move the settings file:
```commandline
PsExec -i -s "c:\program files\nsclient++\nscp" settings --migrate-to "ini:C:\Windows\System32\Config\systemprofile\NSClient++\nsclient.ini"
```
What this will do is update `boot.ini` to point to the new location and move all settings from the old config file to the new one.

And finally delete the old config file:
```commandline
del "C:\Program Files\NSClient++\nsclient.ini"
```

Restart NSClient++ to make sure the new config file is used.



Then you need to update the service to use the new path:
```

Then you need to update the service to use the new path:
```commandline



### Using credential manager

Using credential manager is easy you simply set:
```ini
[/settings]

; use credential manager - Store sensitive keys in use credential manager instead of ini file
use credential manager = true
```

And then save the config.
```commandline
nscp settings --update
```

Now here is the first catch, credential manager is per account so likely you are not running NSClient++ as the same user as the one you are logged in as.
Thus you can either need to first switch to that user or run the command as that user.
The default account use is local system and to switch to local system you can use [PsExec](https://docs.microsoft.com/en-us/sysinternals/downloads/psexec) like this:
```commandline
PsExec -i -s "c:\program files\nsclient++\nscp" settings --update
```

If you no open the ini file it will look like this:
```ini
[/settings/default]

; Password - Password used to authenticate against server
password = $CRED$; Se credential manager: NSClient++-/settings/default.password
```

Restart NSClient++ and you are good to go.

To restore passwords you can do the reverse:
```commandline
PsExec -i -s "c:\program files\nsclient++\nscp" settings --path /settings --key "use credential manager" --set false
PsExec -i -s "c:\program files\nsclient++\nscp" settings --update
```
