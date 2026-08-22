# Real Tomcat with the manager status page enabled, for
# webservers-live.test.ts. The official image ships the manager app parked
# in webapps.dist; copy it live, grant a manager-status-only user (what
# check_tomcat_status authenticates as), and drop the RemoteAddrValve from
# the manager context - it only admits 127.0.0.1 and requests arrive from
# the docker bridge.
FROM tomcat:10.1-jre21

RUN cp -r /usr/local/tomcat/webapps.dist/manager /usr/local/tomcat/webapps/manager && \
    printf '<?xml version="1.0" encoding="UTF-8"?>\n<Context antiResourceLocking="false" privileged="true" />\n' \
      > /usr/local/tomcat/webapps/manager/META-INF/context.xml && \
    printf '<?xml version="1.0" encoding="UTF-8"?>\n<tomcat-users xmlns="http://tomcat.apache.org/xml" version="1.0">\n  <user username="status" password="tomcat-status" roles="manager-status"/>\n</tomcat-users>\n' \
      > /usr/local/tomcat/conf/tomcat-users.xml

EXPOSE 8080
