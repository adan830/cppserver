#include "network.h"

extern PFdProcess gFdProcess[MAX_FD];

//extern float sum;

void process_one_event(int epollfd, struct epoll_event *m_events,
                       timer_link *timerlink) {

    int fd = m_events->data.fd;

    if ((!gFdProcess[fd]->m_activeclose) && (m_events->events & EPOLLIN)) {
        if (gFdProcess[fd]->m_readfun) {
            if (0 > gFdProcess[fd]->m_readfun(epollfd, fd, timerlink)) {
                gFdProcess[fd]->m_activeclose = true;
            }
        }
    }

   else if ((!gFdProcess[fd]->m_activeclose) && (m_events->events & EPOLLOUT)) {
        if (gFdProcess[fd]->m_writefun) {
            if (0 > gFdProcess[fd]->m_writefun(epollfd, fd, timerlink)) {
                gFdProcess[fd]->m_activeclose = true;
            }
        }
    }

    if ((!gFdProcess[fd]->m_activeclose) && (m_events->events & EPOLLRDHUP)) {
        printf("events EPOLLRDHUP \n");
        gFdProcess[fd]->m_activeclose = true;
    }

    if (gFdProcess[fd]->m_activeclose) {
        timerlink->remote_timer(&(gFdProcess[fd]));
        gFdProcess[fd]->m_closefun(epollfd, fd, timerlink);
    }

}

void process_event(int epollfd, struct epoll_event *m_events,
                   time_value timeout, timer_link *timers) {

    int m_eventsize = epoll_wait(epollfd, m_events, MAXEPOLLEVENT, timeout);
    if (m_eventsize > 0) {
        for (int i = 0; i < m_eventsize; ++i) {
            process_one_event(epollfd, m_events + i, timers);
        }
    } else {  // if(sum>0)printf("sum = %f ms\n",sum);
    }

    time_value tnow = get_now();
    // timer
    while (true) {

        void *point = timers->get_timer(tnow);
        if (!point) {
            break;
        }
        FdProcess *p = (FdProcess *)point;
        p->m_timeoutfun(epollfd, p->fd, timers, tnow);
        //		timers->add
    }
}
