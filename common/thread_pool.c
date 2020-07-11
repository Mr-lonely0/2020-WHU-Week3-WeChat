/*************************************************************************
	> File Name: thread_pool.c
	> Author: 
	> Mail: 
	> Created Time: Thu 09 Jul 2020 02:50:47 PM CST
 ************************************************************************/

#include "head.h"

extern int repollfd, bepollfd;
extern struct User *bteam, *rteam;
extern pthread_mutex_t bmutex,rmutex;

void send_all(struct ChatMsg *msg){
    for(int i = 0; i < MAX; i++){
        if(bteam[i].online) 
            send(bteam[i].fd, (void *)msg, sizeof(struct ChatMsg), 0);
        if(rteam[i].online) 
            send(rteam[i].fd, (void *)msg, sizeof(struct ChatMsg), 0);
    }
}
void send_to(char *to,struct ChatMsg *msg,int fd){
    int flag = 0;
    for(int i = 0;i< MAX; i++){
        if(rteam[i].online && (!strcmp(to, rteam[i].name) || !strcmp(to,rteam[i].name))){
        send(rteam[i].fd,msg, sizeof(struct ChatMsg), 0);
        flag = 1;
        break;
    }
        if(bteam[i].online && (!strcmp(to,bteam[i].name) || !strcmp(to,bteam[i].name))){
             send(bteam[i].fd,msg, sizeof(struct ChatMsg), 0);
             flag = 1;
             break; 
         } 
    }
    if(!flag){
        memset(msg->msg, 0 ,sizeof(msg->msg));
        sprintf(msg->msg,"用户 %s 不在线，或者用户名错误啦！", to);
        msg->type = CHAT_SYS;
        send(fd, msg, sizeof(struct ChatMsg), 0);
    }
}

void do_work(struct User *user){
    struct ChatMsg r_msg;
    struct ChatMsg msg;
    bzero(&msg,sizeof(msg));
    bzero(&r_msg,sizeof(r_msg));
    recv(user->fd, (void*)&msg, sizeof(msg), 0);
    if (msg.type & CHAT_WALL) {
        printf("<%s> : %s \n", user->name, msg.msg);
        //r_msg.type = CHAT_SYS;
        //bzero(r_msg.msg, sizeof(r_msg.msg));
        //sprintf(r_msg.msg,"您的好友"NONE"%s"NONE"上线了，快打个招呼吧！\n",user->name);
        //send_all(&r_msg);
        strcpy(msg.name,user->name);
        send_all(&msg);
    } else if (msg.type & CHAT_MSG) {
        char to[20] = {0};
        int i = 1;
        for(;i <=21; i++){
            if(msg.msg[i] == ' ') break;
        }
        if(msg.msg[i] != ' ' || msg.msg[0] != '@'){
            memset(&r_msg, 0, sizeof(r_msg));
            r_msg.type = CHAT_SYS;
            sprintf(r_msg.msg,"私聊格式错误！");
            send(user->fd,(void *)&r_msg, sizeof(r_msg),0);
        }else{
            msg.type = CHAT_MSG;
            strcpy(msg.name, user->name);
            strncpy(to,msg.msg + 1,i - 1);
            send_to(to,&msg,user->fd);
        }
    } else if(msg.type & CHAT_FIN){
        bzero(msg.msg, sizeof(msg.msg));
        msg.type = CHAT_SYS;
        sprintf(msg.msg,"注意：我们的好朋友 %s 要下线了！\n", user->name);
        strcpy(msg.name,user->name);
        send_all(&msg);
        if(user->team)
            pthread_mutex_lock(&bmutex);
        else
            pthread_mutex_lock(&rmutex);
        user->online = 0;
        int epollfd = user->team ? bepollfd : repollfd;
        del_event(epollfd,user->fd);
        if(user->team)
            pthread_mutex_unlock(&bmutex);
        else 
            pthread_mutex_unlock(&rmutex);
        printf(GREEN"Server Info"NONE": %s logout!\n", user->name);
        close(user->fd);
    } else if (msg.type & CHAT_FUNC) {
        if (msg.msg[0] != '#') {
            memset(&r_msg, 0, sizeof(r_msg));
            r_msg.type = CHAT_SYS;
            sprintf(r_msg.msg, "特殊功能格式错误（#+数字），请检查后重试\n");
            send(user->fd, &r_msg, sizeof(r_msg), 0);
        } else if (msg.msg[1] == '1') {
            r_msg.type = CHAT_SYS;
            bzero(r_msg.msg, sizeof(r_msg.msg));
            strcat(r_msg.msg, "\n当前在线用户：\n");
            int flag = 0;
            int count = 0;
            for (int i = 0; i < MAX; i++) {
                if (rteam[i].online == 1) {
                    if (count < 3) {
                        strcat(r_msg.msg, rteam[i].name);
                        strcat(r_msg.msg, "\n");
                        flag = 1;
                    }
                    count++;
                }
                if (bteam[i].online == 1) {
                    if (count < 3) {
                        strcat(r_msg.msg, bteam[i].name);
                        strcat(r_msg.msg, "\n");
                        flag = 1;
                    }
                    count++;
                }
            }
            if (flag == 0) {
                strcat(r_msg.msg, "当前0人在线\n");
            } else {
                char tail[50] = {0};
                sprintf(tail, "共计%d人在线", count);
                strcat(r_msg.msg, tail);
            }
            send_to(user->name, &r_msg, user->fd);
            return;
        } else if (msg.msg[1] == '2') {
        memset(&r_msg, 0, sizeof(r_msg));
        r_msg.type = CHAT_SYS;
        sprintf(r_msg.msg, "网安勇士为了救出花椰妹，重拾c语言宝剑，跨过debug陷阱，抓住熊孩子蒜头君，靠踢足球为生（结果因为太菜被迫聊天，沦为网络乞丐），手撕红黑树，最后投奔蒜头大魔王，过上幸福快乐的生活。");
        send_all(&r_msg);
        }
    }
}


void task_queue_init(struct task_queue *taskQueue, int sum, int epollfd) {
    taskQueue->sum = sum;
    taskQueue->epollfd = epollfd;
    taskQueue->team = calloc(sum, sizeof(void *));
    taskQueue->head = taskQueue->tail = 0;
    pthread_mutex_init(&taskQueue->mutex, NULL);
    pthread_cond_init(&taskQueue->cond, NULL);
}

void task_queue_push(struct task_queue *taskQueue, struct User *user) {
    pthread_mutex_lock(&taskQueue->mutex);
    taskQueue->team[taskQueue->tail] = user;
    DBG(L_GREEN"Thread Pool"NONE" : Task push %s\n", user->name);
    if (++taskQueue->tail == taskQueue->sum) {
        DBG(L_GREEN"Thread Pool"NONE" : Task Queue End\n");
        taskQueue->tail = 0;
    }
    pthread_cond_signal(&taskQueue->cond);
    pthread_mutex_unlock(&taskQueue->mutex);
}


struct User *task_queue_pop(struct task_queue *taskQueue) {
    pthread_mutex_lock(&taskQueue->mutex);
    while (taskQueue->tail == taskQueue->head) {
        DBG(L_GREEN"Thread Pool"NONE" : Task Queue Empty, Waiting For Task\n");
        pthread_cond_wait(&taskQueue->cond, &taskQueue->mutex);
    }
    struct User *user = taskQueue->team[taskQueue->head];
    DBG(L_GREEN"Thread Pool"NONE" : Task Pop %s\n", user->name);
    if (++taskQueue->head == taskQueue->sum) {
        DBG(L_GREEN"Thread Pool"NONE" : Task Queue End\n");
        taskQueue->head = 0;
    }
    pthread_mutex_unlock(&taskQueue->mutex);
    return user;
}

void *thread_run(void *arg) {
    pthread_detach(pthread_self());
    struct task_queue *taskQueue = (struct task_queue *)arg;
    while (1) {
        struct User *user = task_queue_pop(taskQueue);
        do_work(user);
    }
}
