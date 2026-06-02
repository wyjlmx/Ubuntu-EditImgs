# 使用 Ubuntu 26.04 LTS 官方基础镜像以确保长期支持与最新工具链
FROM ubuntu:26.04

# 避免安装过程中的交互式选择提示
ENV DEBIAN_FRONTEND=noninteractive

# 更新系统并安装现代 C++ 开发工具链、依赖库与常用工具（包含 wget、vim 与 iproute2）
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    vim \
    iproute2 \
    iputils-ping \
    openssh-server \
    libopencv-dev \
    pkg-config \
    gdb \
    && rm -rf /var/lib/apt/lists/*

# 纯文本直接修改 shadow 与 ssh 配置文件（彻底绕过所有构建期二进制程序和 PAM 限制，并将 root 密码设为 root）
RUN mkdir -p /var/run/sshd /etc/ssh/sshd_config.d && \
    sed -i 's/^root:[^:]*:/root:$1$root$9gr5KxwuEdiI80GtIzd.U0:/' /etc/shadow && \
    sed -i 's/^#\?PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    echo "PermitRootLogin yes" > /etc/ssh/sshd_config.d/root-login.conf

# 设置容器内的默认工作目录
WORKDIR /app

# 启动 SSH 服务并使其保持前台运行以维持容器生命周期
CMD ["/usr/sbin/sshd", "-D"]
