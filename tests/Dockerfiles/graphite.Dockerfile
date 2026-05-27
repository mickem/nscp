# Minimal Graphite / carbon line-protocol receiver used by graphite-submit.test.ts.
# Listens on TCP 2003 and appends every received byte to /data/metrics.txt; the
# test reads that file back via `docker exec` (containerReadAll) and asserts on
# the metric lines. This stands in for a full carbon+whisper+graphite-web stack,
# which the test does not need.
FROM python:3.12-slim

RUN mkdir -p /data

COPY Dockerfiles/entrypoints/graphite-receiver.py /graphite-receiver.py

EXPOSE 2003

ENTRYPOINT ["python3", "/graphite-receiver.py"]
