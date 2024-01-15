#include "container.h"
#include "logger.h"
#include <memory>
#include <string_view>
#include <unistd.h>
#include <sys/fsuid.h>
#include "networktool/network.h"
// #include <string>

int main(){
    printf("%d\n", getpid());
    std::string_view host_hostname = "your_host_name";
    std::string_view container_hostname = "container";
    std::string_view container_root_dir = "your_container_fs_path";
    std::string_view container_ip = "192.168.130.100";
    std::string_view bridge_name = "your_bridge_name";
    std::string_view bridge_ip = "your_bridge_ip";

    sethostname(host_hostname.data(), host_hostname.size());
    log.info("start container...");
    docker::ContainerConfig container_config{
        container_hostname,
        container_root_dir,
        container_ip,
        bridge_name,
        bridge_ip
    };
    docker::CGroupConfig cgroup_config{
        "/sys/fs/cgroup/cpu/cpp_docker",
        "/sys/fs/cgroup/memory/cpp_docker",
        1000000,
        200000,  // cpu时间限制 200000/1000000 = 20%
        100*1021*1024 // 内存限制100M
    };
    auto con_ptr = std::make_shared<docker::Container>(container_config,cgroup_config);
    con_ptr->start();
    log.info("stop container...");
    return 0;
}