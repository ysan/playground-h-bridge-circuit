# avr-libc 2.1以降でないとtinyAVR 0/1/2シリーズに対応しないため、
# bookwormではなくtrixieを使います。
FROM debian:trixie-slim

ENV DEBIAN_FRONTEND=noninteractive

# vim等で日本語 (UTF-8) が化けないように、ロケールをUTF-8にします。
# C.UTF-8はlocalesパッケージなしで使えます。
ENV LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential \
  gcc-avr \
  binutils-avr \
  avr-libc \
  avrdude \
  make \
  git \
  ca-certificates \
  sudo \
  vim \
  && rm -rf /var/lib/apt/lists/*

# ユーザーを追加します。ホスト側のUID/GIDに合わせることで、bind mount先の
# ファイルがrootで汚染されないようにします。
# dialoutグループはシリアルポート (/dev/ttyUSB*) へのアクセス用です。
ARG UID=1000
ARG GID=1000
ARG USER=builder
ARG GROUP=builder
RUN groupadd -g ${GID} ${GROUP} && \
  useradd -m -s /bin/bash -u ${UID} -g ${GID} -G sudo,dialout ${USER} && \
  echo "${USER} ALL=NOPASSWD: ALL" > /etc/sudoers.d/container_users

# ユーザーを切り替えます
USER ${USER}
WORKDIR /home/${USER}

CMD ["/bin/bash"]

# 使い方は README.md と docs/00-setup.md を参照してください。
