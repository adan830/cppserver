#include "network.h"
#include "socket_util.h"
#include "timers.h"

using namespace std;

pthread_t t_all[MAX_CPU] = {0};
cpu_set_t mask[MAX_CPU];

// 命令管道
int sv[MAX_CPU][2] = {};
int cpu_cmd_map[MAX_CPU][2];
int cpu_num = 1;
bool close_fun(int fd) { return true; }

struct sendfdbuff order_list[MAX_CPU];

volatile int jobs = 0;

struct epoll_event *m_events;

int epollfd;

struct accepted_fd *g_fd;

pthread_mutex_t dispatch_mutex;
pthread_cond_t dispatch_cond;

extern PFdProcess gFdProcess[MAX_FD];

int besim_fds[MAX_CPU] = {0};
int besim_port = 9999;
char besim_addr[32] = "10.61.0.109";
map<pthread_t, int> cpu_map;

char files[50][BUF_LEN];
map<string, char *> fileMap;

void open_file()
{
    DIR* dp;
    struct dirent* ep;
    string name, path;
    char *dirname = NULL, *fileBuf = NULL;
    int count = 0;
    for (int i = 0; i < 2; i++)
    {
        if (i == 0)
            dirname = PADDING_DIR;
        else
            dirname = TPL_DIR;
        dp = opendir(dirname);
        if (dp != NULL)
        {
            while (ep = readdir(dp))
            {
                if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, ".."))
                {
                    name = ep->d_name;
                    path = dirname;
                    path += name;
                    fstream infile(path.c_str(), ios::in);
                    if (!infile.is_open())
                    {
                        perror("open tpl/padding file error");
                        exit(-1);
                    }
                    fileBuf = &(files[count][0]);
                    count++;
                    infile.read(fileBuf, BUF_LEN);
                    fileBuf[infile.gcount()] = 0;
                    fileMap[name] = fileBuf;
                    infile.close();
                }
            }
            closedir(dp);
        }
        else
        {
            perror("open PADDING_DIR error");
            exit(-1);
        }
    }
}


int connect_besim(char *host, int port) {
    struct sockaddr_in addr;
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd <= 0) {
        return -1;
    }
   // printf(" socket new fd %d \n", clientfd);
    if (clientfd >= MAX_FD) {  // 太多链接 系统无法接受
        close(clientfd);
        return -2;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = htons(port);

//    set_noblock(clientfd);

    int i = connect(clientfd, (struct sockaddr *)&addr, sizeof(addr));
    if (i < 0 && errno != EINPROGRESS) {
        Log << "errno " << errno << " " << strerror(errno) << std::endl;
        close(clientfd);
        return -1;
    } else {
        //char *send_message="abcd";
        //int len=send(clientfd,send_message,strlen(send_message),0);
        //printf("%d, %d\n",len, i);
        return clientfd;
    }
}

int main() {

 //   besimportnumber = 89;
 //   besim_addr[256] = "10.61.0.109";
 //   besim_file[1024] = "/fcgi-bin/besim_fcgi.fcgi";

    g_fd = NULL;

    pthread_mutex_init(&dispatch_mutex, NULL);
    pthread_cond_init(&dispatch_cond, NULL);

    signal(SIGPIPE, SIG_IGN);  // sigpipe 信号屏蔽

    // bind
    m_events = (struct epoll_event *)malloc(MAXEPOLLEVENT *
                                            sizeof(struct epoll_event));
    int rt = 0;
    epollfd = epoll_create(MAXEPOLLEVENT);
    struct sockaddr_in servaddr;
    int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listenfd <= 0) {
        perror("socket");
        return 0;
    }

    rt = set_reused(listenfd);
    if (rt < 0) {
        perror("setsockopt");
        exit(1);
    }
    for (int i = 0; i < MAX_FD; ++i) {
        PFdProcess p = new FdProcess();
        assert(p);
        p->fd = i;
        gFdProcess[i] = p;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(listenport);

    rt = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (rt < 0) {
        perror("setsockopt");
        exit(1);
    }

    rt = listen(listenfd, 50000);
    if (rt < 0) {
        perror("listen");
        exit(1);
    }
    //
    struct epoll_event ev = {0};

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenfd;
    set_noblock(listenfd);

    gFdProcess[listenfd]->m_readfun = accept_readfun;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(-1);
    }

    // 到Cpu 核心数目
    cpu_num = sysconf(_SC_NPROCESSORS_CONF) * 1;

 // 生成管理通道
    for (int i = 1; i < cpu_num; ++i) {
        socketpair(AF_LOCAL, SOCK_STREAM, 0, sv[i]);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = sv[i][0];
        set_noblock(sv[i][0]);
        gFdProcess[sv[i][0]]->m_writefun = accept_readfun;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sv[i][0], &ev) == -1) {
            perror("epoll_ctl: listen_sock");
            exit(-1);
        }
    }

    for (int i = 1; i < cpu_num; ++i) {
        Log << " cpu " << i << "  " << sv[i][0] << "  " << sv[i][1]
            << std::endl;
    }

    open_file();

    // 启动 对应的socketpair
    for (int i = 1; i < cpu_num; ++i) {
        CPU_ZERO(&mask[i]);
        CPU_SET(i, &mask[i]);
        struct worker_thread_arg *pagr = new worker_thread_arg();

        pagr->cpuid = i % (sysconf(_SC_NPROCESSORS_CONF) - 1) + 1;
        pagr->orderfd = sv[i][1];

        // 连接besim
        besim_fds[i] = connect_besim(besim_addr, besim_port);
        if (besim_fds[i] == -1)
            exit(-1);

        // 启动线程
        pthread_create(&t_all[i], NULL, worker_thread, (void *)pagr);
        //绑定亲元性
        pthread_setaffinity_np(t_all[i], sizeof(cpu_set_t),
                               (const cpu_set_t *)&(mask[i]));
    }
/*    int i;
    map<pthread_t, int>::iterator it;
    for(i=1,it=cpu_map.begin();it!=cpu_map.end();++it, i++)
    	printf("%d\ttid: %lu\tcpu_id: %d\tsocketfd: %d\n", i, it->first, it->second, besim_fds[it->second]);
*/
    pthread_t dispatch_t;

    pthread_create(&dispatch_t, NULL, dispatch_conn, NULL);

    // 启动主线程
    pthread_t t_main;
    cpu_set_t mask_main;
    // pthread_attr_t attr_main;

    CPU_ZERO(&mask_main);
    CPU_SET(0, &mask_main);

    // 启动线程
    pthread_create(&t_main, NULL, main_thread, NULL);
    //绑定亲元性
    pthread_setaffinity_np(t_main, sizeof(cpu_set_t),
                           (const cpu_set_t *)&mask_main);

    pthread_join(t_main, NULL);

    for (int i = 1; i < cpu_num; i++)
        close(besim_fds[i]);
    cpu_map.clear();

    return 0;
}

void *main_thread(void *arg) {

    timer_link global_timer;
    int outime = 1000;
    while (true) {

        process_event(epollfd, m_events, outime, &global_timer);
        if (global_timer.get_arg_time_size() > 0) {
            outime = global_timer.get_mintimer();
        } else {
            outime = 1000;
        }
        if (jobs < 0) {
            free(m_events);
            return 0;
        }
    }
}
