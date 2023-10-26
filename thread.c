#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define TIMER_SIGNAL SIGALRM

void handle_signal(int signum)
{
    printf("Signal received by thread: %ld\n", pthread_self());
}

void *signal_thread(void *arg)
{

    // 设置信号处理函数
    // signal(TIMER_SIGNAL, handle_signal);
    sigset_t set;
    int sig;

    // 将目标线程设置为接收 TIMER_SIGNAL 信号
    sigemptyset(&set);
    sigaddset(&set, TIMER_SIGNAL);
    while (1)
    {
        // 等待信号
        siginfo_t info;
        int sig = sigwaitinfo(&set, &info); // 等待触发的信号
        if (sig == TIMER_SIGNAL)
        {
            printf("Timer triggered in thread: %ld\n", pthread_self());
            printf("data is %d\n", *(int *)info.si_value.sival_ptr);
            // 在这里处理定时器触发事件
        }
    }

    return NULL;
}

int main()
{

    sigset_t set;
    int sig;

    // 将目标线程设置为接收 TIMER_SIGNAL 信号
    sigemptyset(&set);
    sigaddset(&set, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    pthread_t thread;

    printf("%p\n", timerid);
    int data = 100;

    // 创建定时器
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    sev.sigev_value.sival_ptr = &data;
    timer_create(CLOCK_REALTIME, &sev, &timerid);
    timer_delete(timerid);
    // 设置定时器参数
    its.it_value.tv_sec = 1; // 初始定时器触发时间为 5 秒
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 1; // 不重复定时
    its.it_interval.tv_nsec = 0;
    //timer_settime(timerid, 0, &its, NULL);

    // 创建信号处理线程
    pthread_create(&thread, NULL, signal_thread, NULL);

    sleep(3);

    its.it_value.tv_sec = 0; // 初始定时器触发时间为 5 秒
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0; // 不重复定时
    its.it_interval.tv_nsec = 0;
    write(STDOUT_FILENO, "stop timer\n", 12);
    timer_settime(timerid,0,&its,NULL);
    sleep(3);
    its.it_value.tv_sec = 1; // 初始定时器触发时间为 5 秒
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 1; // 不重复定时
    its.it_interval.tv_nsec = 0;
    write(STDOUT_FILENO, "start timer\n", 13);
    timer_settime(timerid, 0, &its, NULL);
    // 等待线程结束
    pthread_join(thread, NULL);

    return 0;
}
