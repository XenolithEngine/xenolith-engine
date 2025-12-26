# Для сборки поместите дистрибутив [Alpine Linux](https://www.alpinelinux.org/downloads/)
# в виде архива (alpine-minirootfs-3.20.3-x86_64.tar.gz), в текущую директорию.
#
# wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz

FROM scratch
ADD alpine-minirootfs-3.20.3-x86_64.tar.gz /
RUN \
	apk add make cmake gcc g++ git pkgconfig perl linux-headers binutils-gold \
	&& cd /opt \
	&& git clone https://github.com/libstappler/libstappler-root.git \
	&& cd libstappler-root \
	&& git submodule update --init \
	&& rm -rf doc tests .git \
	&& cd deps \
	&& rm -rf android windows \
	&& make \
	&& cd linux \
	&& make clean \
	&& make x86_64 \
	&& rm -rf x86_64/bin \
	&& cd .. \
	&& rm -rf src android windows mac \
	&& apk del make cmake gcc g++ git pkgconfig perl linux-headers binutils-gold

CMD ["/bin/sh"]
