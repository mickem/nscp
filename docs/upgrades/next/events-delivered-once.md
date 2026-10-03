---
icon: "📤"
modules: [WEBServer, ElasticClient, PythonScript]
action: none
---
**Each event record is delivered once.** Nothing to do. An event message can
carry several records - a real-time CPU filter sends one per core - and the
core handed the whole message to each subscriber once for every record in it.
Every subscriber walks all the records itself, so a message of N records
arrived N times over: `/api/v2/events` stored each record N times, ElasticClient
sent N copies of each document, and a script's `Registry.event` handler saw
each record N times. Each subscriber now gets each message once. Expect fewer
events and documents where real-time filters match more than one record at a
time; the ones that remain are the real ones.
