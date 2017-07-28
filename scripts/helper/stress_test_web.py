import Queue
import threading
import urllib2
import ssl
import json
import time

#
# Configuration
#
PASSWORD = "CHANGE ME"
URL = "https://localhost:8443/query/check_cpu"
THREADS = 32
COUNT_PER_THREAD = 50


ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE


def get_url(q, url):
    for x in xrange(COUNT_PER_THREAD):
        try:
            req = urllib2.Request(URL)
            req.add_header('password', PASSWORD)
            resp = urllib2.urlopen(req, context=ctx)
            data = json.loads(resp.read())
            msg = data["payload"][0]["lines"][0]["message"]
            q.put(msg)
        except Exception as e:
            q.put("%s"%e)

q = Queue.Queue()

threads = []
for u in xrange(THREADS):
    t = threading.Thread(target=get_url, args = (q,u))
    t.daemon = True
    threads.append(t)

start = time.time()
for t in threads:
    t.start()

for t in threads:
    t.join()
end = time.time()

time = end-start
sz = q.qsize()

last = ''
cnt = 0
while not q.empty():
    msg = q.get()
    cnt = cnt + 1
    if msg != last:
        print "(%d): %s"%(cnt, msg)
        last = msg
        cnt = 0
print "(%d): %s"%(cnt, last)

print "Time spent: %s calls in %d seconds %dr/s"%(sz, time, sz/time)
