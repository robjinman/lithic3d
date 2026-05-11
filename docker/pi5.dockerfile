FROM ubuntu:24.04
RUN userdel -r ubuntu

ARG UID=1000
ARG GID=1000

RUN groupadd -g $GID -o builduser
RUN useradd -m -u $UID -g $GID -o -s /bin/bash builduser

RUN DEBIAN_FRONTEND=noninteractive apt-get update

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential gcc-14-aarch64-linux-gnu g++-14-aarch64-linux-gnu wget git curl zip unzip tar \
  ninja-build python3 python3-jinja2 python3-setuptools

USER builduser
WORKDIR /home/builduser

RUN wget https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-linux-x86_64.sh
RUN chmod +x cmake-3.28.1-linux-x86_64.sh

USER root
RUN ./cmake-3.28.1-linux-x86_64.sh --skip-license --prefix=/usr/local

USER builduser
RUN git clone https://github.com/microsoft/vcpkg.git
RUN cd vcpkg && git checkout 5fa0f075 && ./bootstrap-vcpkg.sh

ADD ./pi5/vulkan/1.4.341.1 /vulkan
ADD ./pi5/sysroot /sysroot

USER root
RUN cat <<'EOF' > /etc/bash.bashrc
export VCPKG_ROOT=/home/builduser/vcpkg
export PATH=$PATH:$VCPKG_ROOT

source /vulkan/setup-env.sh
EOF

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
  pkg-config autoconf libtool libevdev-dev
