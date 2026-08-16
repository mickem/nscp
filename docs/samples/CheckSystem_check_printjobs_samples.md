**Default check (stuck and failing jobs):**

The default is critical on a job the spooler cannot clear on its own and warning
on one that has been waiting more than ten minutes.

```
check_printjobs
OK: No print jobs queued|'count'=0;0;0
```

```
check_printjobs
OK: OneNote (Desktop): 'document' by micha (queued, 13s)|'OneNote (Desktop)_2_age'=13s;600;0 'count'=1;0;0
```

```
check_printjobs
CRITICAL: HP LaserJet: 'quarterly.pdf' by CORP\ann (error, 240s)|'HP LaserJet_42_age'=240s;600;0 'count'=1;0;0
```

**Alert earlier on a queue that is not moving:**

```
check_printjobs "warning=age > 1"
WARNING: OneNote (Desktop): 'document' by micha (queued, 23s)|'OneNote (Desktop)_2_age'=23s;1;0 'count'=1;0;0
```

**Full per-job detail:**

```
check_printjobs warning=none critical=none "top-syntax=${list}" "detail-syntax=printer=${printer} id=${id} doc='${document}' owner=${owner} status=${status} size=${size} pages=${pages}/${pages_printed} prio=${priority} age=${age} sub=${submitted}"
printer=OneNote (Desktop) id=2 doc='document' owner=micha status=queued size=53620 pages=1/0 prio=1 age=18 sub=2026-08-16 12:10:02|'count'=1;0;0
```

`submitted` is rendered in UTC; threshold on `age` (seconds) rather than on the
timestamp.

**Find who is filling the queue:**

```
check_printjobs "filter=owner = 'CORP\\ann'" "warning=count > 20" "critical=none" "top-syntax=${count} job(s) from ann" "ok-syntax=${count} job(s) from ann"
3 job(s) from ann
```

**Alert on a single very large job:**

Size thresholds take byte units; a bare number is rejected, so write `500M`
rather than `524288000`.

```
check_printjobs "warning=none" "critical=size > 500M"
CRITICAL: HP LaserJet: 'plot.ps' by CORP\bob (spooling, 45s)|'HP LaserJet_51_size'=734003200B;0;524288000 'count'=1;0;0
```

**Only the jobs needing a person at the printer:**

```
check_printjobs "filter=user_intervention = 1 or paper_out = 1" "critical=count > 0"
CRITICAL: HP LaserJet: 'invoice.pdf' by CORP\eve (user_intervention, paper_out, 512s)
```

**Watch one queue on a print server, over NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_printjobs --argument "filter=printer = 'HP LaserJet'" --argument "warning=age > 30m"
OK: All 2 job(s) ok.
```
