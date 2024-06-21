# ДЗ12 - сборка CURL

## Задание
Скачать curl последней версии и собрать его в любой UNIX-подобной ОС с поддержкой лишь
трёх протоколов: HTTP, HTTPS и TELNET.

## Цель задания
Получить опыт сборки программ для UNIX-подобных ОС.

## Задачи
1. Работа осуществляется в UNIX-подобной ОС (любой дистрибутив Linux, любая BSD-система, MacOS).
2. Скачан и распакован исходный код curl.
3. Сборка сконфигурирована с поддержкой лишь трёх протоколов HTTP, HTTPS и TELNET.
4. Осуществлена сборка (установку в систему осуществлять не требуется и не рекомендуется).
5. Собранный curl запущен с ключом --version для подтверждения корректности сборки.

## Решение

Для сборки скачал и распаковал исходный код curl (https://github.com/curl/curl).
Целевая система: Ubuntu 23.10, x64, ядро 6.5.0-41, gcc 13.2.0, libc 2.38.

### configure-скрипт
```
autoreconf -fi

./configure --with-openssl --without-msh3 --without-librtmp --without-winidn --without-libidn2 \
	--without-nghttp2 --without-ngtcp2 --without-openssl-quic --without-nghttp3 --without-quiche \
	--without-zsh-functions-dir --without-fish-functions-dir --without-libpsl --without-libgsasl \
	--without-brotli --without-zstd --without-hyper --disable-websockets  --disable-largefile \
	--disable-ares --disable-httpsrr --disable-hsts --disable-ech --disable-ftp --disable-file \
	--disable-ldap --disable-ldaps --disable-rtsp --disable-proxy --disable-dict --disable-tftp \
	--disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --disable-mqtt \
	--disable-ipv6 --disable-sspi --disable-aws --disable-ntlm --disable-tls-srp --disable-unix-sockets \
	--disable-cookies --disable-socketpair --disable-http-auth --disable-doh --disable-mime \
	--disable-bindlocal --disable-form-api --disable-dateparse --disable-netrc --disable-dnsshuffle \
	--disable-alt-svc --disable-headers-api --disable-hsts --disable-websockets --disable-docs \
	--disable-manual --disable-threaded-resolver --disable-pthreads --disable-rt
```

Вывод:
```
configure: Configured to build curl/libcurl:

  Host setup:       x86_64-pc-linux-gnu
  Install prefix:   /usr/local
  Compiler:         gcc
   CFLAGS:          -Werror-implicit-function-declaration -O2 -Wno-system-headers
   CPPFLAGS:        
   LDFLAGS:         
   LIBS:            -lssl -lcrypto -lssl -lcrypto -lz

  curl version:     8.9.0-DEV
  SSL:              enabled (OpenSSL v3+)
  SSH:              no      (--with-{libssh,libssh2})
  zlib:             enabled
  brotli:           no      (--with-brotli)
  zstd:             no      (--with-zstd)
  GSS-API:          no      (--with-gssapi)
  GSASL:            no      (--with-gsasl)
  TLS-SRP:          no      (--enable-tls-srp)
  resolver:         default (--enable-ares / --enable-threaded-resolver)
  IPv6:             no      (--enable-ipv6)
  Unix sockets:     no      (--enable-unix-sockets)
  IDN:              no      (--with-{libidn2,winidn})
  Build docs:       no
  Build libcurl:    Shared=yes, Static=yes
  Built-in manual:  no      (--enable-manual)
  --libcurl option: enabled (--disable-libcurl-option)
  Verbose errors:   enabled (--disable-verbose)
  Code coverage:    disabled
  SSPI:             no      (--enable-sspi)
  ca cert bundle:   /etc/ssl/certs/ca-certificates.crt
  ca cert path:     /etc/ssl/certs
  ca fallback:      no
  LDAP:             no      (--enable-ldap / --with-ldap-lib / --with-lber-lib)
  LDAPS:            no      (--enable-ldaps)
  RTSP:             no      (--enable-rtsp)
  RTMP:             no      (--with-librtmp)
  PSL:              no      (--with-libpsl)
  Alt-svc:          no
  Headers API:      no      (--enable-headers-api)
  HSTS:             no      (--enable-hsts)
  HTTP1:            enabled (internal)
  HTTP2:            no      (--with-nghttp2)
  HTTP3:            no      (--with-ngtcp2 --with-nghttp3, --with-quiche, --with-openssl-quic, --with-msh3)
  ECH:              no      (--enable-ech)
  WebSockets:       no      (--enable-websockets)
  Protocols:        HTTP HTTPS IPFS IPNS TELNET
  Features:         Largefile SSL libz threadsafe
```

### Результат сборки
```
./src/curl --version
curl 8.9.0-DEV (x86_64-pc-linux-gnu) libcurl/8.9.0-DEV OpenSSL/3.0.10 zlib/1.2.13
Release-Date: [unreleased]
Protocols: http https ipfs ipns telnet
Features: Largefile libz SSL threadsafe
```

### Поддержка протоколов IPFS и IPNS
Начиная с версии 8.4.0 в Curl добавлена поддержка протоколов IPFS и IPNS, которая включается автоматически при выборе HTTP.
На настоящий момент отключить флагом configure её нельзя. Как написал автор Curl (https://curl.se/mail/archive-2023-12/0027.html),
необходимсоти в этом нет, т.к. в libcurl эта фича не попадает.

Если очень надо, то можно найти коммит, который добавлял эту поддержку, добавить опцию в скрипты сборки и обернуть соответсвующий код
в исходниках в #ifdef'ы.