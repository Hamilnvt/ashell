#include<stdio.h>
#include<unistd.h>
#include<signal.h>

void handler(int signo)
{
    printf("\n%zu received signal %d\n", getpid(), signo);
}

void DoStuff()
{
    sleep(1);
    printf("ciao\n");
    sleep(1);
    printf("come\n");
    sleep(1);
    printf("stai?\n");
}

int main(int argc, char * argv[]){
	struct sigaction action = { .sa_handler = handler };
	sigaction(SIGINT, &action, NULL);
    printf("My pid is %zu\n", getpid());
    for (;;)
        DoStuff();
}
