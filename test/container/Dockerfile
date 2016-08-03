FROM debian:stable
RUN apt-get -y update &&\
    apt-get install -y python3 &&\
    apt-get install -y sudo &&\
    apt-get install -y make &&\
    apt-get install -y gcc &&\
    apt-get install -y g++ &&\
    apt-get install -y iptables

RUN apt-get install -y libcap-dev

# Add user dev with uid 1000 and password 111 :)
# Enter 111 when you run sudo in container
RUN groupadd dev && \
    useradd dev -g dev -u 1000 -G sudo -p sa72.Pi19MD8A

VOLUME ["/test/aucont", "/test/scripts", "/test/rootfs"]
WORKDIR /test/scripts
USER 1000
CMD ./test.py
