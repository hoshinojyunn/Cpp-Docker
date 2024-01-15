#ifndef CONTAINER_H
#define CONTAINER_H
#include <memory>
#include <sys/types.h>
#include <utility>
#define STACK_SIZE 1024*1024
#define CURRENT_DIR "."
#define PARENT_DIR ".."

#include <string_view>

namespace docker {
    using process_pid = int;
    enum proc_status {
        proc_err = -1,
        proc_exit = 0,
        proc_wait = 1
    };
    
    struct ContainerConfig{
        // 主机名
        std::string_view host_name;
        // 容器根目录
        std::string_view root_dir;
        // 容器ip
        std::string_view ip;
        // 网桥
        std::string_view bridge_name;
        // 网桥ip
        std::string_view bridge_ip;
    };
    struct CGroupConfig{
        constexpr static std::string_view cpu_cfs_period_us_filename = "cpu.cfs_period_us";
        constexpr static std::string_view cpu_cfs_quota_us_filename = "cpu.cfs_quota_us";
        constexpr static std::string_view tasks_filename = "tasks";
        constexpr static std::string_view memory_limit_in_bytes_filename = "memory.limit_in_bytes";
        // cgroup cpu控制组路径
        std::string_view cpu_cgroup_path;
        // cgroup memory控制组路径
        std::string_view memory_cgroup_path;
        // cpu周期（单位：微秒）
        int cpu_period_us;
        /* 
            cpu限额（单位：微秒）
            表示一个进程在cpu_period_us内可以使用cpu_quota_us微秒的cpu时间
        */
        int cpu_quota_us;
        int memory_quota;
        bool isEnable(){
            return !cpu_cgroup_path.empty() 
                && !memory_cgroup_path.empty()
                && cpu_period_us!=0
                && cpu_quota_us!=0
                && memory_quota!=0;
        }
    };
    struct Container : std::enable_shared_from_this<Container>{
        using process_pid = int;
        Container()=default;
        Container(
            const ContainerConfig&_container_config, 
            const CGroupConfig&_cgroup_config):container_config{_container_config}, cgroup_config{_cgroup_config}{}
        void setContainerConfig(const ContainerConfig&_config){this->container_config = _config;}
        void setCGroupConfig(const CGroupConfig&_config){this->cgroup_config=_config;}
        void start();
        std::shared_ptr<Container> getThis(){return shared_from_this();}
        ~Container();
    private:
        // 子进程分配1M栈空间
        char child_stack[STACK_SIZE];
        ContainerConfig container_config;
        CGroupConfig cgroup_config;
        // 问就是为了兼容c接口
        char* veth0;
        char* veth1;

        void run_cmd(std::string_view cmd, char* const args[]);
        // network
        void set_hostname();
        void set_root();
        void set_procsys();
        
        std::pair<char*, char*> create_veth_pair();
        void move_veth2container(
            const char*veth_name, 
            pid_t container_pid, const char*new_name="eth0");
        void init_network(const char* if_name);
        // cgroup
        void set_cpu_quota(pid_t pid);
        void set_memory_quota(pid_t pid);
    };
}


#endif