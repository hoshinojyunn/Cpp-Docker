# 一、概述
Cpp-Docker是基于c++20实现的一个简单但功能完善的Linux虚拟化容器，你可以使用它定制属于自己的容器系统。
# 二、实现方案
## 1.环境隔离方案
Cpp-Docker利用Linux Namespace命名空间技术实现容器的环境隔离，使容器拥有独立的进程树、主机名、网络设备环境、文件挂载点、IPC命名空间等。
## 2.资源管理方案
Cpp-Docker利用Linux Control Groups管理工具实现容器的资源管理，限制容器的可用资源如cpu利用率、内存限制等。
## 3.网络通信环境
Cpp-Docker利用Linux Bridge以及veth pair技术实现容器网络环境的隔离与相互通信。
# 三、quick start
## 1.fetch repository
```sh
# fetch repository
sudo git clone https://github.com/hoshinojyunn/Cpp-Docker.git
# into project
cd Cpp-Docker
```
## 2.build linux bridge
进入bridge_init.sh中，修改BRIDGE_NAME以及NEW_IP为你喜欢的网桥名以及网桥ip。随后运行脚本：
```sh
sudo chmod +x bridge_init.sh && ./bridge_init.sh
```
## 3.install essential environment
在运行之前，你应该去获取一个Linux文件系统，以下为一个简单的Linux文件系统，你可以直接下载它：
```sh
# download filesystem
sudo wget http://labfile.oss.aliyuncs.com/courses/608/docker-image.tar
# uncompress tar file to ./fs dir
sudo mkdir fs && tar -xvf docker-image.tar -C ./fs
```
此外，你还需要安装cgroup工具：
```sh
sudo apt install cgroup-tools #Ubuntu and Debian
sudo yum install cgroup-tools #CentOs and RedHat
```
## 4.configure the corresponding environment variables
在 src/main.cpp中，配置你主机中相应的变量：
```c++
// 主机的主机名
std::string_view host_hostname = "your_host_name";
// 容器的主机名
std::string_view container_hostname = "container";
// 容器的根目录，设为上一步下载的文件系统目录./fs
std::string_view container_root_dir = "your_container_fs_path";
// 容器ip
std::string_view container_ip = "192.168.130.100";
// 网桥名
std::string_view bridge_name = "your_bridge_name";
// 网桥ip
std::string_view bridge_ip = "your_bridge_ip";
```
## 5.run project
```sh
mkdir build
cd build
cmake ..
make
./bin/main
```