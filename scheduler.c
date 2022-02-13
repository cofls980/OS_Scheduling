#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "src/host.h"
#include "src/cpu.h"
#include "src/parser.h"


/* g_cpu : cpu 전역 변수, g_host : host 전역 변수
//실제 사용할 때는 직접 접근하지 마세요 (혹시나 간단하게 디버깅 할 때 관련 변수 쓸까봐 놔둠) 
//제출할 때는 사용한 부분 다 지우고 제출하도록!
*/
extern Cpu* g_cpu; 
extern Host* g_host;

/*****************구현 시작*****************/

/* 구현에 필요한 추가적인 자료구조 구현할 경우에 여기에 추가 */ 
typedef struct Node {
    Proc *process;
    unsigned int last_time;
    struct Node *next;
} Node;

/* 
// 각 scheduler 구현을 위해 필요한 구조체
// (각 구조체에 사용할 정보들을 각각 저장해서 쓰세요.)
*/
typedef struct FIFO {
    Node *front;
    Node *rear;
    int count;
} FIFO;

typedef struct SJF {
    Node *root;
    int count;
} SJF;

typedef struct RR {
    Node *root;
    int count;
} RR;

typedef struct MLFQ {
    struct Node *urgent;
    struct Node *high;
    struct Node *mid;
    struct Node *low;
    int count;
} MLFQ;

typedef struct GUARANTEE {
    Node *root;
    int count;
} GUARANTEE;

/* 구현에 필요한 추가적인 함수 필요한 경우 여기에 추가 */
Node *newNode(Proc *process, unsigned int last_time)
{
    Node *now = (Node *)malloc(sizeof(Node));
    now->process = process;
    now->last_time = last_time;
    now->next = NULL;
}

void Enqueue(FIFO** sch_fifo, Proc *process)
{
    Node *now = newNode(process, -1);    
    if ((*sch_fifo)->count == 0)
        (*sch_fifo)->front = now;
    else
        (*sch_fifo)->rear->next = now;
    (*sch_fifo)->rear = now;
    (*sch_fifo)->count++;
}

void Dequeue(FIFO** sch_fifo)
{
    Node *now;
    if ((*sch_fifo)->count == 0)
        return ;
    now = (*sch_fifo)->front;
    (*sch_fifo)->front = now->next;
    free(now);
    (*sch_fifo)->count--;
}

void Push_SJF(SJF** sch_sjf, Proc *process)
{
    Node *now = newNode(process, -1);
    Node *tmp = (*sch_sjf)->root;
    Node *prev = (*sch_sjf)->root;
    while (tmp)
    {
        if (tmp->process->required_time > process->required_time)
            break;
        prev = tmp;
        tmp = tmp->next;
    }
    if (tmp == prev)
    {
        now->next = tmp;
        (*sch_sjf)->root = now;
    }
    else
    {
        now->next = tmp;
        prev->next = now;
    }
    (*sch_sjf)->count++;
}

void Pop_SJF(SJF** sch_sjf)
{
    Node *tmp;
    if ((*sch_sjf)->count == 0)
        return ;
    tmp = (*sch_sjf)->root;
    (*sch_sjf)->root = tmp->next;
    free(tmp);
    (*sch_sjf)->count--;
}

void Push_RR(RR** sch_rr, Proc *process)
{
    Node *now = newNode(process, -1);
    if ((*sch_rr)->count == 0)
        (*sch_rr)->root = now;
    else
    {
        Node *tmp = (*sch_rr)->root;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = now;
    }
    (*sch_rr)->count++;
}

void Pop_RR(RR** sch_RR)
{
    Node *tmp;
    if ((*sch_RR)->count == 0)
        return ;
    tmp = (*sch_RR)->root;
    (*sch_RR)->root = tmp->next;
    free(tmp);
    (*sch_RR)->count--;
}

void Push_GUARANTEE(GUARANTEE** sch_guarantee, Proc *process)
{
    Node *now = newNode(process, -1);
    if ((*sch_guarantee)->count == 0)
        (*sch_guarantee)->root = now;
    else
    {
        Node *tmp = (*sch_guarantee)->root;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = now;
    }
    (*sch_guarantee)->count++;
}

void Pop_GUARANTEE(GUARANTEE** sch_guarantee, Proc *process)
{
    Node *prev = (*sch_guarantee)->root;
    Node *tmp = (*sch_guarantee)->root;
    while (tmp)
    {
        if (tmp->process->create_time == process->create_time)
            break;
        prev = tmp;
        tmp = tmp->next;
    }
    if (tmp == prev)
        (*sch_guarantee)->root = tmp->next;
    else
        prev->next = tmp->next;
    free(tmp);
    (*sch_guarantee)->count--;
}

Proc *chooseLowestRatio(GUARANTEE** sch_guarantee)
{
    Proc *proc;
    Node *tmp = (*sch_guarantee)->root;

    proc = tmp->process;
    tmp = tmp->next;
    while (tmp)
    {
        if (((float)tmp->process->process_time / (float)(get_current_time() - tmp->process->create_time)) < ((float)proc->process_time / (float)(get_current_time() - proc->create_time)))
            proc = tmp->process;
        tmp = tmp->next;
    }    
    return proc;
}

Node *check_if_null(Node **root, Node*now, MLFQ **sch_mlfq)
{
    if (*root == NULL)
    {
        *root = now;
        (*sch_mlfq)->count++;
        return NULL;
    }
    return *root;
}

void Push_MLFQ(MLFQ **sch_mlfq, Proc *process, unsigned int last_time)
{
    Node *now = newNode(process, last_time);
    Node *tmp;
    if (process->priority == 0)
    {
        if ((tmp = check_if_null(&((*sch_mlfq)->urgent), now, sch_mlfq)) == NULL)
            return;
        tmp = (*sch_mlfq)->urgent;
    }
    else if (process->priority == 1)
    {
        if ((tmp = check_if_null(&((*sch_mlfq)->high), now, sch_mlfq)) == NULL)
            return;
        tmp = (*sch_mlfq)->high;
    }
    else if (process->priority == 2)
    {
        if ((tmp = check_if_null(&((*sch_mlfq)->mid), now, sch_mlfq)) == NULL)
            return;
        tmp = (*sch_mlfq)->mid;
    }
    else if (process->priority == 3)
    {
        if ((tmp = check_if_null(&((*sch_mlfq)->low), now, sch_mlfq)) == NULL)
            return;
        tmp = (*sch_mlfq)->low;
    }
    while (tmp->next)
        tmp = tmp->next;
    tmp->next = now;
    (*sch_mlfq)->count++;
}

void Pop_MLFQ(MLFQ **sch_mlfq, int priority)
{
    Node *tmp;
    if ((*sch_mlfq)->count == 0)
        return ;
    if (priority == 0)
    {
        tmp = (*sch_mlfq)->urgent;
        (*sch_mlfq)->urgent = tmp->next;
    }
    else if (priority == 1)
    {
        tmp = (*sch_mlfq)->high;
        (*sch_mlfq)->high = tmp->next;
    }
    else if (priority == 2)
    {
        tmp = (*sch_mlfq)->mid;
        (*sch_mlfq)->mid = tmp->next;
    }
    else if (priority == 3)
    {
        tmp = (*sch_mlfq)->low;
        (*sch_mlfq)->low = tmp->next;
    }
    free(tmp);
    (*sch_mlfq)->count--;
}

Proc *choosePriority(MLFQ **sch_mlfq)
{
    Proc *proc;
    if ((*sch_mlfq)->urgent != NULL)
        return ((*sch_mlfq)->urgent->process);
    else if ((*sch_mlfq)->high != NULL)
        return ((*sch_mlfq)->high->process);
    else if ((*sch_mlfq)->mid != NULL)
        return ((*sch_mlfq)->mid->process);
    else if ((*sch_mlfq)->low != NULL)
        return ((*sch_mlfq)->low->process);
    else
        return NULL;
}

int findProc(Node *tmp)
{
    int cnt = 0, first = -1;
    unsigned int time = 0;
    
    while (tmp)
    {
        if ((get_current_time() - tmp->last_time + 1) >= 10)
        {
            if (time == 0 || time > (tmp->process->required_time - tmp->process->process_time))
            {
                time = tmp->process->required_time - tmp->process->process_time;
                first = cnt;
            }
        }
        tmp = tmp->next;
        cnt++;
    }
    return (first);
}

void realupdate(Node *tmp, int first, MLFQ **sch_mlfq)
{
    Node *prev, *node;
    int cnt = 0;
    prev = tmp;
    while (tmp)
    {
        if (cnt == first)
            break;
        prev = tmp;
        tmp = tmp->next;
        cnt++;
    }
    tmp->process->priority -= 1;
    Push_MLFQ(sch_mlfq, tmp->process, get_current_time() + 1);
    node = tmp;
    if (prev == tmp)
    {
        int pri = tmp->process->priority + 1;
        if (pri == 1)
            (*sch_mlfq)->high = tmp->next;
        else if (pri == 2)
            (*sch_mlfq)->mid = tmp->next;
        else if (pri == 3)
            (*sch_mlfq)->low = tmp->next;    
    }
    else
        prev->next = tmp->next;
    free(node);
    (*sch_mlfq)->count--;
}

void updatePriority(MLFQ **sch_mlfq)
{
    int first;
    while ((first = findProc((*sch_mlfq)->high)) != -1)
        realupdate((*sch_mlfq)->high, first, sch_mlfq);
    while ((first = findProc((*sch_mlfq)->mid)) != -1)
        realupdate((*sch_mlfq)->mid, first, sch_mlfq);
    while ((first = findProc((*sch_mlfq)->low)) != -1)
        realupdate((*sch_mlfq)->low, first, sch_mlfq);
}

/* 
// 각 scheduler의 시뮬레이션 전 initial 작업을 처리하는 함수
// (initial 작업으로 필요한 행동을 하면 됨.)
*/
void init_FIFO(FIFO** sch_fifo, Parser* parser) {
    if (!parser)
        return;
    (*sch_fifo)->front = (*sch_fifo)->rear = NULL;
    (*sch_fifo)->count = 0;
}
void init_SJF(SJF** sch_sjf, Parser* parser) {
    if (!parser)
        return;
    (*sch_sjf)->root = NULL;
    (*sch_sjf)->count = 0;
}
void init_RR(RR** sch_rr, Parser* parser) {
    if (!parser)
        return;
    (*sch_rr)->root = NULL;
    (*sch_rr)->count = 0;
}
void init_MLFQ(MLFQ** sch_mlfq, Parser* parser) {
    if (!parser)
        return;
    (*sch_mlfq)->urgent = NULL;
    (*sch_mlfq)->high = NULL;
    (*sch_mlfq)->mid = NULL;
    (*sch_mlfq)->low = NULL;
    (*sch_mlfq)->count = 0;
}
void init_GUARANTEE(GUARANTEE** sch_guarantee, Parser* parser) {
    if (!parser)
        return;
    (*sch_guarantee)->root = NULL;
    (*sch_guarantee)->count = 0;
}

/* 
// 각 scheduler의 시뮬레이션 중 한 cycle마다 해야할 행동을 처리하는 함수
// (한 cycle마다 해야할 행동을 작성하면 됨.)
*/
void simulate_FIFO(Proc* next_process, FIFO* sch_fifo) {
    if (next_process != NULL)
        Enqueue(&sch_fifo, next_process);
    if (!is_cpu_busy())
    {
        if (!(sch_fifo->count == 0))
            schedule_cpu(sch_fifo->front->process);
    }
    if (run_cpu())
    {
        terminate_process(unschedule_cpu());
        Dequeue(&sch_fifo);
    }
}

void simulate_SJF(Proc* next_process, SJF* sch_sjf) {
    if (next_process != NULL)
        Push_SJF(&sch_sjf, next_process);
    if (!is_cpu_busy())
    {
        if (!(sch_sjf->count == 0))
        {
            schedule_cpu(sch_sjf->root->process);
            Pop_SJF(&sch_sjf);
        }
    }
    if (run_cpu())
        terminate_process(unschedule_cpu());
}

void simulate_RR(Proc* next_process, RR* sch_rr) {
    if (next_process != NULL)
        Push_RR(&sch_rr, next_process);
    if (!is_cpu_busy())
    {
        if (!(sch_rr->count == 0))
        {
            schedule_cpu(sch_rr->root->process);
            Pop_RR(&sch_rr);
        }
    }
    if (run_cpu())
    {
        Proc *proc = unschedule_cpu();
        if (proc->process_time != proc->required_time)
            Push_RR(&sch_rr, proc);
        else
            terminate_process(proc);
    }
}

void simulate_MLFQ(Proc* next_process, MLFQ* sch_mlfq) {
    if (next_process != NULL)
        Push_MLFQ(&sch_mlfq, next_process, next_process->create_time);
    if (!is_cpu_busy())
    {
        Proc *proc = choosePriority(&sch_mlfq);
        if (proc != NULL)
        {
            schedule_cpu(proc);
            Pop_MLFQ(&sch_mlfq, proc->priority);
        }
    }
    if (run_cpu())
    {
        Proc *proc = unschedule_cpu();
        if (proc->process_time != proc->required_time)
            Push_MLFQ(&sch_mlfq, proc, get_current_time() + 1);
        else
            terminate_process(proc);  
    }
    updatePriority(&sch_mlfq);
}

void simulate_GUARANTEE(Proc* next_process, GUARANTEE* sch_guarantee) {
    if (next_process != NULL)
        Push_GUARANTEE(&sch_guarantee, next_process);
    if (!is_cpu_busy())
    {
        if (!(sch_guarantee->count == 0))
        {
            Proc *proc = chooseLowestRatio(&sch_guarantee);
            schedule_cpu(proc);
            Pop_GUARANTEE(&sch_guarantee, proc);
        }  
    }
    if (run_cpu())
    {
        Proc *proc = unschedule_cpu();
        if (proc->process_time != proc->required_time)
            Push_GUARANTEE(&sch_guarantee, proc);
        else
            terminate_process(proc);
    }
}

/* 
// 각 scheduler의 시뮬레이션이 끝나고 해야할 행동을 처리하는 함수
// (시뮬레이션이 끝나고 해야할 행동을 작성하면 됨.)
*/
void terminate_FIFO(FIFO* sch_fifo) {

}
void terminate_SJF(SJF* sch_sjf) {

}
void terminate_RR(RR* sch_rr) {

}
void terminate_MLFQ(MLFQ* sch_mlfq) {

}
void terminate_GUARANTEE(GUARANTEE* sch_guaranteeo) {

}

/******************구현 끝******************/

void print_result();
void* init_scheduler(Parser* parser);
void terminate_scheduler(void* scheduler, SCHEDULING_POLICY policy);

int main(int argc, char* argv[]) {
    Parser parser;
    void* scheduler = NULL;
    /*parse and err check parameters*/
    printf("\n=============== Environment Setting ================\n");
    if (parse_parameters(&parser, argc, argv) == -1) 
        return 0;
    printf("=================================================\n");

    /* init host & cpu & scheduler*/
    printf("\n=============== Init Simulation ================\n");
    
    if (init_host(&parser) == -1)
        return 0;
    if (init_cpu(&parser) == -1)
        goto term_host;
    if ((scheduler = init_scheduler(&parser)) == NULL)
        goto term_cpu;
    printf("=================================================\n");
    
    /* start simulation */
    printf("\n=============== Start Simulation ================\n\n");
    while (is_simulation_on_going()) {

        /* simulate one cycle start*/
        Proc* next_process = create_if_next_process_available();
        switch (parser.policy) {
        case POLICY_FIFO:
            simulate_FIFO(next_process, (FIFO*)scheduler);
            break;
        case POLICY_SJF:
            simulate_SJF(next_process, (SJF*)scheduler);
            break;
        case POLICY_RR:
            simulate_RR(next_process, (RR*)scheduler);
            break;
        case POLICY_MLFQ:
            simulate_MLFQ(next_process, (MLFQ*)scheduler);
            break;
        case POLICY_GUARANTEE:
            simulate_GUARANTEE(next_process, (GUARANTEE*)scheduler);
            break;
        }
        
        /* simulate one cycle finish*/
        increase_current_time();
    }
    printf("=============== Finish Simulation ===============\n\n");
    
    print_result(); //print result

    /*terminate scheduler & cpu & host*/
    printf("\n============= Terminate Simulation ==============\n");
    terminate_scheduler(scheduler, parser.policy);
term_cpu:
    terminate_cpu();
term_host:
    terminate_host();
    printf("=================================================\n");

    return 0;
}

void* init_scheduler(Parser* parser) {
    void* sch;
    switch(parser->policy) {
        case POLICY_FIFO:
            if((sch = (void*)malloc(sizeof(FIFO))))
                init_FIFO((FIFO**)(&sch), parser);
            break;
        case POLICY_SJF:
            if((sch = (void*)malloc(sizeof(SJF))))
                init_SJF((SJF**)(&sch), parser);
            break;
        case POLICY_RR:
            if((sch = (void*)malloc(sizeof(RR))))
                init_RR((RR**)(&sch), parser);
            break;
        case POLICY_MLFQ:
            if((sch = (void*)malloc(sizeof(MLFQ))))
                init_MLFQ((MLFQ**)(&sch), parser);
            break;
        case POLICY_GUARANTEE:
            if((sch = (void*)malloc(sizeof(GUARANTEE))))
                init_GUARANTEE((GUARANTEE**)(&sch), parser);
            break;
    }
    
    if (sch)
        PRINT_MSG("INIT SCHEDULER: OK");
    else
        PRINT_MSG("INIT SCHEDULER: FAIL");
    return sch;
}

void terminate_scheduler(void* scheduler, SCHEDULING_POLICY policy) {
    switch(policy) {
        case POLICY_FIFO:
            terminate_FIFO((FIFO*)scheduler);
        break;
        case POLICY_SJF:
            terminate_SJF((SJF*)scheduler);
        break;
        case POLICY_RR:
            terminate_RR((RR*)scheduler);
        break;
        case POLICY_MLFQ:
            terminate_MLFQ((MLFQ*)scheduler);
        break;
        case POLICY_GUARANTEE:
            terminate_GUARANTEE((GUARANTEE*)scheduler);
        break;
    }
    free(scheduler);
    PRINT_MSG("TERMINATE SCHEDULER: OK");
}

void print_result() {
    char policies[5][10] = {"FIFO", "SJF", "RR", "MLFQ", "GUARANTEE"};
    printf("\n=============== Simulation Result ===============\n");
    printf("scheduling policy: %s\n", policies[g_cpu->policy]);
    printf("request process: %lld\t", g_host->request_cnt);
    printf("service process: %lld\n", g_host->service_cnt);
    printf("schedule count: %d\n", g_cpu->schedule_cnt);
    printf("total process time(cycle): %lld\n", get_current_time());
    printf("total cpu time(cycle): %lld\t", g_cpu->process_time);
    printf("total turnaround time(cycle): %lld\n", g_host->turnaround_time);
    printf("=================================================\n");
}