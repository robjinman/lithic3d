FROM ubuntu:22.04

RUN useradd -ms /bin/bash builduser

RUN DEBIAN_FRONTEND=noninteractive apt-get update

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential wget git curl zip unzip tar ninja-build python3 python3-jinja2 python3-setuptools

USER builduser
WORKDIR /home/builduser

RUN wget https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-linux-x86_64.sh
RUN chmod +x cmake-3.28.1-linux-x86_64.sh

USER root
RUN ./cmake-3.28.1-linux-x86_64.sh --skip-license --prefix=/usr/local

USER builduser
RUN git clone https://github.com/microsoft/vcpkg.git
RUN cd vcpkg && git checkout 5fa0f075 && ./bootstrap-vcpkg.sh

USER root
RUN mkdir /opt/vulkan
RUN chown -R builduser:builduser /opt/vulkan

USER builduser
RUN cd /opt/vulkan && wget https://sdk.lunarg.com/sdk/download/1.4.341.1/linux/vulkansdk-linux-x86_64-1.4.341.1.tar.xz
RUN cd /opt/vulkan && tar xf ./vulkansdk-linux-x86_64-1.4.341.1.tar.xz

USER root
RUN cat <<'EOF' > /etc/bash.bashrc
export VCPKG_ROOT=/home/builduser/vcpkg
export PATH=$PATH:$VCPKG_ROOT

source /opt/vulkan/1.4.341.1/setup-env.sh
EOF

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
  libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config autoconf libtool bison
