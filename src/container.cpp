#include "container.h"
#include "logger.h"
#include <arpa/inet.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <net/if.h>
#include <netinet/in.h>
#include <sched.h>
// #include <string>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <string.h>
#include "networktool/network.h"
#include <format>

void docker::Container::start(){
    struct ChildArgs{
        int read_fd;
        std::shared_ptr<docker::Container>container;
    };

    auto setup = [](void* args) -> int{
        printf("In child,child_pid=%d\n",getpid());
        // read(, nullptr, 1);
        // sleep(2);
        auto* child_args = reinterpret_cast<ChildArgs*>(args);
        auto _this = child_args->container;
        auto read_pipe_fd = child_args->read_fd;
        _this->set_hostname();
        _this->set_root();
        _this->set_procsys();
        char buf[1];
        if(auto e = read(read_pipe_fd, buf, 1)==-1){
            log.error("fail to read");
            exit(-1);
        }
        close(read_pipe_fd);
        _this->init_network("eth0");
        log.info("start bash...");
        _this->run_cmd("/bin/bash", nullptr);
        return docker::proc_status::proc_wait;
    };
    
    auto [veth0, veth1] = create_veth_pair();
    this->veth0 = veth0;
    this->veth1 = veth1;
    int p_fd[2];
    if(auto e = pipe(p_fd)!=0){
        log.error("fail to create pipe");
        return;
    }
    ChildArgs args{
        p_fd[0], 
        getThis() // enable_shared_from_this要求当前对象正在被shared_ptr管理
    };
    // 设置命名空间 父进程与子进程命名空间隔离
    /*
        flags: 
        1. CLONE_NEWUTS:允许创建一个拥有独立主机名和域名的UTS命名空间。
        2. CLONE_NEWPID:允许在一个进程内创建一个全新的进程树，拥有独立的进程ID。
        3. CLONE_NEWNET:允许创建一个拥有独立网络设备、IP地址、端口等的网络命名空间。
        4. CLONE_NEWNS:允许创建一个拥有独立的文件系统挂载点和文件系统层次结构的挂载命名空间。
        5. CLONE_NEWIPC:允许创建一个拥有独立的System V IPC（信号量、消息队列、共享内存）的IPC命名空间。
    */
    // if(unshare(CLONE_NEWUTS) == -1){
    //     perror("invoking unshare fail\n");
    //     return;
    // }
    docker::process_pid child_pid = clone(
        setup, // 子进程执行函数 void(*fn)(void*) 
        this->child_stack+STACK_SIZE, // 子进程调用栈指针置于栈底：child_stack+STACK_SIZE;
        CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | SIGCHLD , 
        &args
    );
    printf("In parent,child_pid=%d,parent_pid=%d\n", child_pid, getpid());
    log.info("start to move veth0 to container");
    move_veth2container(this->veth0, child_pid);
    // cgroup
    if(this->cgroup_config.isEnable()){
        set_cpu_quota(child_pid);
        set_memory_quota(child_pid);
    }
    // 告知子进程veth0已经移到容器中，可以开始网络配置了
    if(auto e = write(p_fd[1], " ", 1) == -1){
        log.error("fail to write");
        exit(1);
    }
    close(p_fd[1]);
    // activate_container(child_pid);
    // 不关心子进程返回状态 第二、三参数如下设置
    waitpid(child_pid, nullptr, 0);

}

void docker::Container::run_cmd(std::string_view cmd, char* const args[]){
    /*
        如果execv成功执行 那么它将取代当前进程内存映像 
        当前进程将不会继续往下执行
        所以不能在当前进程执行 要另开一个进程
    */
    char* const c_cmd = new char[cmd.size()+1];
    strcpy(c_cmd, cmd.data());
    char* const child_args[] = {c_cmd, nullptr};
    execv(child_args[0], child_args);
    // 如果execv执行失败 当前进程会继续往下执行
    perror("fail to execv\n");
    delete[]c_cmd;
}

void docker::Container::set_hostname(){
    if(this->container_config.host_name.empty()){
        log.info("Configuration's hostname is empty");
        return;
    }
    sethostname(
        this->container_config.host_name.data(), 
        this->container_config.host_name.size()
    );
}

void docker::Container::set_root(){
    // cd: 当前进程切换到path目录下
    chdir(this->container_config.root_dir.data());
    // 将当前目录作为根目录
    chroot(CURRENT_DIR); 
}

void docker::Container::set_procsys(){
    char* virtual_source = (char*)("none"); // 挂载虚拟文件系统的source名为none
    /*
        proc文件系统是一个虚拟文件系统，提供了对内核和进程信息的访问。
        有些命令需要proc文件系统的支持(如ps)
        因为是自己做的文件系统 需要手动挂载
    */
    mount(
        virtual_source, 
        "/proc", 
        "proc",
        0,
        nullptr);
    /*
        sysfs 是一个虚拟文件系统，提供了一种统一的、文件式的访问方式，
        使用户空间可以通过文件操作的方式来获取和配置内核中的信息。
        如：查看CPU的工作状态：
            cat /sys/devices/system/cpu/cpu0/online
    */
    mount(
        virtual_source, 
        "/sys",
        "sysfs",
        0,
        nullptr);

}

std::pair<char*, char*> docker::Container::create_veth_pair(){
    /* 
        创建container的veth pair
        其中veth0在容器中 veth1在网桥上
        veth0与veth1的名称均以container0为前缀 
        lxc_mkifname需要在设备名后添加'X'表示随机创建虚拟网络设备
    */
    char veth0buf[IFNAMSIZ] = "container0X";
    char veth1buf[IFNAMSIZ] = "container0X";
    char* veth0 = lxc_mkifname(veth0buf);
    char* veth1 = lxc_mkifname(veth1buf);
    if(auto e = lxc_veth_create(veth0, veth1) != 0 ){
        throw std::runtime_error{"veth_create fail\n"};
    }
    // 给网桥上的veth1设置MAC地址(hardware address)
    if(auto e = setup_private_host_hw_addr(veth1) != 0){
        throw std::runtime_error{"setup veth1 host_hw_addr fail\n"};
    }
    // 将veth1放到网桥上
    if(auto e = lxc_bridge_attach(this->container_config.bridge_name.data(), veth1) != 0){
        throw std::runtime_error{"attach veth1 to bridge fail\n"};
    }
    // 激活veth1
    if(auto e = lxc_netdev_up(veth1)!=0){
        throw std::runtime_error{"veth1 netdev_up fail\n"};
    }
    return {veth0, veth1};
}

void docker::Container::move_veth2container(
            const char*veth_name, 
            pid_t container_pid, const char*new_name){
    if(auto e = lxc_netdev_move_by_name(this->veth0, container_pid, new_name)!=0){
        throw std::runtime_error{"move veth0 to container fail\n"};
    }
}

void docker::Container::init_network(const char* if_name) {
    uint if_index;
    if((if_index = if_nametoindex(if_name)) == 0){
        throw std::runtime_error{"if_nametoindex fail. net dev may not exist\n"};
    }

    struct in_addr ipv4;
    struct in_addr broad_cast_addr;
    // 容器的gateway设为网桥bridge ip
    struct in_addr gateway_addr;
    // 将ip地址点分十进制转换为结构体(二进制)
    inet_pton(AF_INET, this->container_config.ip.data(), &ipv4);
    inet_pton(AF_INET, "255.255.255.0", &broad_cast_addr);
    inet_pton(AF_INET, this->container_config.bridge_ip.data(), &gateway_addr);
    // std::string s = std::format("{},{},{},{}", if_index, 
    //     ipv4.s_addr, 
    //     broad_cast_addr.s_addr, 
    //     gateway_addr.s_addr);
    // std::cout << s << std::endl;
    // 给veth0(在容器中名称默认为eth0)设置ip以及广播地址
    log.info("start to set veth0 ip and broadcast...");
    if(auto e = lxc_ipv4_addr_add(if_index, &ipv4, &broad_cast_addr, 16)!=0){
        throw std::runtime_error{"set veth0 ip and broadcast fail\n"};
    }
    // 激活容器lo口
    log.info("start to activate lo...");
    if(auto e = lxc_netdev_up("lo")!=0){
        throw std::runtime_error{"lo netdev_up fail\n"};
    }
    // 激活eth0
    log.info("start to activate eth0...");
    if(auto e = lxc_netdev_up(if_name) != 0){
        throw std::runtime_error{"veth0(eth0 in container) netdev_up fail\n"};
    }
    // 设置网关
    log.info("start to set the gateway of eth0...");
    if(auto e = lxc_ipv4_gateway_add(if_index, &gateway_addr)!=0){
        throw std::runtime_error{"veth0(eth0 in container) set gateway fail\n"};
    }

    // 给eth0设置mac地址
    char mac[18];
    log.info("start to generate a hardware address...");
    new_hwaddr(mac);
    log.info("start to setup hardware address to eth0...");
    if(auto e = setup_hw_addr(mac, if_name)!=0){
        throw std::runtime_error{"veth0(eth0 in container) set hw_addr fail\n"};
    }
}

void docker::Container::set_cpu_quota(pid_t pid){
    auto cpu_period = this->cgroup_config.cpu_period_us;
    auto cpu_quota = this->cgroup_config.cpu_quota_us;
    auto cgroup_path = this->cgroup_config.cpu_cgroup_path;

    // cpu控制组路径
    auto path = std::filesystem::path(cgroup_path);    
    if(!std::filesystem::exists(path)){
        log.info("cpu control group not exist, default to create");
        if(auto ret = std::filesystem::create_directory(path)==false){
            log.error("fail to create cpu dir");
            return;
        }
        return;
    }
    auto cpu_period_path = path / std::filesystem::path(this->cgroup_config.cpu_cfs_period_us_filename);
    auto cpu_quota_path = path / std::filesystem::path(this->cgroup_config.cpu_cfs_quota_us_filename);
    auto tasks_pid_path = path / std::filesystem::path(this->cgroup_config.tasks_filename);
    auto s = std::format("cpu_period_path={}\ncpu_quota_path={}\ncpu_tasks_path={}\n",cpu_period_path.string(), cpu_quota_path.string(),tasks_pid_path.string());
    log.info(s);
    std::string set_period_cmd = "echo " + std::to_string(cpu_period) + ">" + cpu_period_path.string();
    std::string set_quota_cmd = "echo " + std::to_string(cpu_quota) + ">" + cpu_quota_path.string();
    std::string set_tasks_cmd = "echo " + std::to_string(pid) + ">" +  tasks_pid_path.string();
    
    auto s1=std::format("set_cpu_period_cmd={}\nset_cpu_quota_cmd={}\nset_cpu_tasks={}",set_period_cmd,set_quota_cmd,set_tasks_cmd);
    log.info(s1);

    if(auto ret = std::system(set_period_cmd.c_str())!=0){
        log.error("fail to set cpu period");
        return;
    }
    if(auto ret = std::system(set_quota_cmd.c_str())!=0){
        log.error("fail to set cpu quota");
        return;
    }
    if(auto ret = std::system(set_tasks_cmd.c_str())!=0){
        log.error("fail to set cpu tasks");
        return;
    }
}

void docker::Container::set_memory_quota(pid_t pid){
    auto path = std::filesystem::path(this->cgroup_config.memory_cgroup_path);
    if(!std::filesystem::exists(path)){
        log.info("memory control group not exist, default to create");
        if(auto ret = std::filesystem::create_directory(path)==false){
            log.error("fail to create memory dir");
            return;
        }
        return;
    }
    auto memory_limits_filename = path / std::filesystem::path(this->cgroup_config.memory_limit_in_bytes_filename);
    auto tasks_pid_path = path / std::filesystem::path(this->cgroup_config.tasks_filename);
    auto s = std::format("memory_limits_filename={}\nmemory_tasks={}\n", memory_limits_filename.string(), tasks_pid_path.string());
    log.info(s);
    
    std::string set_quota_cmd = "echo " + std::to_string(this->cgroup_config.memory_quota) + ">" + memory_limits_filename.string();
    std::string set_tasks_cmd = "echo " + std::to_string(pid) + ">" +  tasks_pid_path.string();
    auto s1 = std::format("set_memory_quota_cmd={}\nset_memory_tasks={}", set_quota_cmd, set_tasks_cmd);
    log.info(s1);
    
    if(auto ret = std::system(set_quota_cmd.c_str())!=0){
        log.error("fail to set memory limits");
        return;
    }
    if(auto ret = std::system(set_tasks_cmd.c_str())!=0){
        log.error("fail to set memory tasks");
        return;
    }
}

docker::Container::~Container(){
    lxc_netdev_delete_by_name(this->veth0);
    lxc_netdev_delete_by_name(this->veth1);
    auto cpu_path = std::filesystem::path(this->cgroup_config.cpu_cgroup_path);
    auto memory_path = std::filesystem::path(this->cgroup_config.memory_cgroup_path);
    auto cpu_tasks_pid_path = cpu_path / std::filesystem::path(this->cgroup_config.tasks_filename);
    auto memory_tasks_pid_path = memory_path / std::filesystem::path(this->cgroup_config.tasks_filename);
    // std::string rm_cpu_cmd = "rm -rf " + std::string{cpu_path};
    // std::string rm_memory_cmd = "rm -rf " + std::string{memory_path};
    // 截断模式std::ios::trunc打开文件即把文件清空
    std::ofstream cpu_tasks_file{cpu_tasks_pid_path, std::ios::trunc};
    std::ofstream memory_tasks_file{memory_tasks_pid_path, std::ios::trunc};

}


