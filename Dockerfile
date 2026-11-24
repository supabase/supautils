ARG BASE_IMAGE=rockylinux:9
FROM ${BASE_IMAGE}

RUN dnf --assumeyes update

RUN source /etc/os-release && \
	case "${ROCKY_SUPPORT_PRODUCT}" in \
		Rocky-Linux-9) \
			dnf --assumeyes install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm \
				&& dnf config-manager --enable crb ;; \
		Rocky-Linux-8) \
			dnf install --assumeyes https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm \
				&& dnf config-manager --enable powertools \
				&& dnf -qy module disable postgresql ;; \
	esac
		
# install non postgres stuff
RUN dnf install --assumeyes make
RUN dnf install --assumeyes gcc
RUN dnf install --assumeyes redhat-rpm-config
RUN dnf install --assumeyes clang
RUN dnf install --assumeyes sudo
RUN dnf install --assumeyes llvm-devel


RUN source /etc/os-release && \
	case "${ROCKY_SUPPORT_PRODUCT}" in \
		Rocky-Linux-9) \
			dnf install --assumeyes https://download.postgresql.org/pub/repos/yum/reporpms/EL-9-$(uname -m)/pgdg-redhat-repo-latest.noarch.rpm ;; \
		Rocky-Linux-8) \
			dnf install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-$(uname -m)/pgdg-redhat-repo-latest.noarch.rpm ;; \
	esac


# install postgres stuff
RUN dnf install --assumeyes postgresql17-devel
RUN dnf install --assumeyes postgresql17-server
RUN dnf install --assumeyes postgresql17-contrib
RUN dnf install --assumeyes pg_tle_17

# setup supautils user
RUN echo 'postgres ALL=(ALL:ALL) NOPASSWD: ALL' >> /etc/sudoers

USER postgres
ENV PATH=/usr/pgsql-17/bin/:${PATH}
WORKDIR /home/postgres/supautils
ENTRYPOINT ["bash"]
CMD ["-c", "bash test/init.sh; cd /home/postgres/supautils; PATH=/usr/pgsql-17/bin/:$PATH make; sudo PATH=/usr/pgsql-17/bin/:$PATH make install; sudo chown -R postgres /home/postgres/supautils; PATH=/usr/pgsql-17/bin/:$PATH make installcheck REGRESS_OPTS='--inputdir=test --temp-instance=temp --temp-config=./test/init.conf --dbname=contrib_regression'"]


