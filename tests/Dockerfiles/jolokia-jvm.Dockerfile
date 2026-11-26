# A real JVM with the Jolokia JVM agent, for webservers-live.test.ts
# (check_jvm). Tomcat is the JVM so the check has application MBeans to read
# beyond java.lang: the docs webapp is deployed so a
# Catalina:type=Manager,context=/docs MBean exists - a context path with a
# slash, which exercises check_jvm's `!/` escape against the real agent.
#
# The agent binds to 127.0.0.1 by default; 0.0.0.0 lets the test reach it
# through the published port. With no jolokia-access.xml the agent allows
# every read, which is what a stock install does.
FROM tomcat:10.1-jre21

ARG JOLOKIA_VERSION=2.1.2
ARG JOLOKIA_SHA256=38be4960fce575b355e9142f3c3cb89e54fb52bbbef4e1d48a5a6a1df8adb9bf
ADD https://repo1.maven.org/maven2/org/jolokia/jolokia-agent-jvm/${JOLOKIA_VERSION}/jolokia-agent-jvm-${JOLOKIA_VERSION}-javaagent.jar /opt/jolokia/jolokia-agent.jar
RUN echo "${JOLOKIA_SHA256}  /opt/jolokia/jolokia-agent.jar" | sha256sum -c - && \
    chmod 644 /opt/jolokia/jolokia-agent.jar && \
    cp -r /usr/local/tomcat/webapps.dist/docs /usr/local/tomcat/webapps/docs

ENV CATALINA_OPTS="-javaagent:/opt/jolokia/jolokia-agent.jar=port=8778,host=0.0.0.0"

EXPOSE 8080 8778
