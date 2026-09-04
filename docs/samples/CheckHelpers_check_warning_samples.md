**Return WARNING with the default message:**

```
check_warning
WARNING: No message
```

**Return WARNING with your own message:**

```
check_warning "message=Disk approaching capacity"
WARNING: Disk approaching capacity
```

**Verifying that a monitoring server renders each state correctly:**

Running `check_ok`, `check_warning` and `check_critical` in turn is the quickest
way to confirm that a newly configured service actually shows all three states,
and that notifications fire for the ones you expect.

```
check_nrpe --host 192.168.56.103 --command check_warning --arguments "message=state test"
WARNING: state test
```
