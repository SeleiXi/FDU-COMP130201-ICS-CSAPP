# Shlab实验报告
## 实验目的
本lab的目的在于模拟一个命令行程序，可以实现以下几种功能：
1. jobs: 查询正在运行或已经暂停的前台和后台任务</p>
2. bg < job >: 将已停止的后台作业更改为正在运行的后台作业</p>
3. fg < job >: 将已停止的或正在运行的后台作业改为前台作业</p>
4. kill < job >:终止任务
5. quit: 退出tsh
## 实验内容
每步解释如代码注释所示
### 1. 实现eval函数：
（1） 判断输入命令是不是内置命令，其是前台命令还是后台命令。
（2） 若是内置命令则不进行处理。
（3） 若是后台命令，则将其添加至任务列表，等待当前前台命令结束后执行；若是后台命令，则添加至任务列表，运行之并打印之。

```C
void eval(char *cmdline) 
{
	char* argv[MAXARGS];
	char buf[MAXLINE];
	int bg,state;
	pid_t pid;
	strcpy(buf,cmdline);
	bg=parseline(buf,argv);
	state=bg?BG:FG;
	if(argv[0]==NULL)
		return;
	sigset_t mask_all,prev_one,mask_one;
    //阻塞SIGCHLD信号并保存到mask_one,防止子进程打断父进程的执行
	sigfillset(&mask_all);
	sigemptyset(&mask_one);
	sigaddset(&mask_one,SIGCHLD);
	if(!builtin_cmd(argv)){
        //阻塞所有信号，防止中间出现进程打断，并将信号保存至prev_one
		sigprocmask(SIG_BLOCK,&mask_one,&prev_one);
		if((pid=fork())==0){
            //解除子进程阻塞信号
			sigprocmask(SIG_SETMASK,&prev_one,NULL);
            //将子进程组ID设置为子进程组PID
			setpgid(0,0);
            //执行输入的进程。若执行成功，则子进程被替换；若执行不成功，则退出
			execve(argv[0],argv,environ);
			exit(0);
		}
        //若命令为前台
		if(state==FG){
			sigprocmask(SIG_BLOCK,&mask_all,NULL);
			addjob(jobs,pid,state,cmdline);
			sigprocmask(SIG_SETMASK,&mask_one,NULL);
			waitfg(pid);
		}
        //若命令为后台
		else{
			sigprocmask(SIG_BLOCK,&mask_all,NULL);
			addjob(jobs,pid,state,cmdline);
			sigprocmask(SIG_SETMASK,&mask_one,NULL);
			printf("[%d](%d)%s\n",pid2jid(pid),pid,cmdline);
		}
        //设置信号掩码，重新阻塞信号
		sigprocmask(SIG_SETMASK,&prev_one,NULL);
	}
    	return;
}
```
此处需注意在执行子进程时才解除信号阻塞，否则会出现并发，导致形如"fg %1"指令卡顿
### 2. 实现waitfg函数
如果输入命令为前台命令，则悬挂之，暂不执行。
```C
void waitfg(pid_t pid)
{
	sigset_t mask;
	sigemptyset(&mask);
	while(fgpid(jobs)!=0){
		sigsuspend(&mask);
	}
    	return;
}
```
使用sigsuspend函数这一原子指令，防止被打断

### 3. 实现builtin_cmd函数
该函数为简单的命令解析函数
```C
int builtin_cmd(char **argv) 
{
	bool tag_quit=!strcmp(argv[0],"quit");
	bool tag_fg=!strcmp(argv[0],"fg");
	bool tag_bg=!strcmp(argv[0],"bg");
	bool tag_job=!strcmp(argv[0],"jobs");
	bool tag_or=!strcmp(argv[0],"&");
	if(tag_quit)
		exit(0);
	if(tag_fg||tag_bg){
		do_bgfg(argv);
		return 1;
	}
	if(tag_job){
		listjobs(jobs);
		return 1;
	}
	if(tag_or)
		return 1;
	return 0;     /* not a builtin command */
}
```
### 4. 信号发送函数的实现
#### （1）实现sigint_handler函数
该函数用于发送SIGINT信号
```C
void sigint_handler(int sig) 
{
	//保存原始errno值，防止在信号处理过程中改变errno导致的错误
	int olderrno=errno;
	int pid;
	sigset_t mask_all,prev;
    //填充信号，使信号集里有所有信号
	sigfillset(&mask_all);
    //阻塞所有信号
	sigprocmask(SIG_BLOCK,&mask_all,&prev);
    //如果是子进程
	if((pid=fgpid(jobs))!=0){
        //解除掩码
		sigprocmask(SIG_SETMASK,&prev,NULL);
        //使用kill函数发送SIGINT信号到前台pid对应的进程组
		kill(-pid,SIGINT);
	}
    //恢复之前的errno值
	errno=olderrno;
    return;
}
```
#### (2) 实现sigstp_handler
该函数使用kill函数发送SIGSTOP信号，实现方法参照以上sigint_handler
```C
void sigtstp_handler(int sig) 
{
	int olderrno=errno;
	int pid;
	sigset_t mask_all,prev;
	sigfillset(&mask_all);
	sigprocmask(SIG_BLOCK,&mask_all,&prev);
	if((pid=fgpid(jobs))>0){
		sigprocmask(SIG_SETMASK,&prev,NULL);
		kill(-pid,SIGSTOP);
	}
	errno=olderrno;
    return;
}
```
#### (3) 实现sigchld_handler
该函数用于回收（僵死的）子进程。该函数将子进程分为三种情况：EXITED,STOPPED,TERMINATED,并在父进程中阻塞信号，而后回收子进程。
```C
void sigchld_handler(int sig) 
{
	//储存原来errno的值
	int olderrno=errno;
	int status;
	pid_t pid;
	struct job_t *job;
	sigset_t mask_all,prev_one;
	sigfillset(&mask_all);
	//在父进程中执行相关操作。本循环用于判断该进程为父进程
	while((pid=waitpid(-1,&status,WNOHANG|WUNTRACED))>0){
		//阻塞信号，防止子进程结束的信号发送至父进程，从而父进程无法回收
		sigprocmask(SIG_BLOCK,&mask_all,&prev_one);
		//如果一个子进程已经结束
		if(WIFEXITED(status)){
			deletejob(jobs,pid);
		}
		//如果一个子进程收到了一个打断信号
		else if(WIFSIGNALED(status)){
			printf("Job [%d](%d)terminated by signal %d\n",pid2jid(pid),pid,WTERMSIG(status));
			deletejob(jobs,pid);
		}
		//如果一个子进程被停止了
		else if(WIFSTOPPED(status)){
			printf("Job[%d](%d)terminated by signal %d\n",pid2jid(pid),pid,WSTOPSIG(status));
			job=getjobpid(jobs,pid);
			job->state=ST;
		}
		//接触信号
		sigprocmask(SIG_SETMASK,&prev_one,NULL);
	}
	errno=olderrno;
    return;
}
```
### 5. 处理前后台程序的函数
该函数为buitin_command的一部分，用来处理前后台的相关命令
```C
void do_bgfg(char **argv) 
{
	struct job_t *job=NULL;
	int state;
	int id;
	//先判断命令是前台命令还是后台命令
	if(!strcmp(argv[0],"bg"))
		state=BG;
	else
		state=FG;
	//如果未输入命令编号
	if(argv[1]==NULL){
		printf("%s command requires PID or %%jobid\n",argv[0]);
		return;
	}
	//查询是否有相应进程在运行，查询的id类型为jid
	if(argv[1][0]=='%'){
		if(sscanf(&argv[1][1],"%d",&id)>0){
			job=getjobjid(jobs,id);
			if(job==NULL){
				printf("%%%d:No such job\n",id);
				return;
			}
		}
	}
	//输入命令非法
	else if(!isdigit(argv[1][0])){
		printf("%s:argument must be a pid or %%jobid\n",argv[0]);
		return;
	}
	//输入的是pid类型的命令
	else{
		id=atoi(argv[1]);
		job=getjobpid(jobs,id);
		if(job==NULL){
			printf("(%d):No such process\n",id);
			return;
		}
	}
	//若不是以上命令，则结束相应进程
	kill(-(job->pid),SIGCONT);
	job->state=state;
	//若是后台进程，则打印之
	if(state==BG)
		printf("[%d](%d)%s\n",job->jid,job->pid,job->cmdline);
	else
	//若是前台进程，则使用waitfg函数进行处理
		waitfg(job->pid);
    	return;
}
```
## 实验输出
所有16个trace文件均已通过测试，以下以trace15为例：
```
# trace15.txt - Putting it all together
#
tsh> ./bogus
tsh> ./myspin 10
Job [1](<pid>)terminated by signal 2
tsh> ./myspin 3 &
[1](<pid>)./myspin 3 &

tsh> ./myspin 4 &
[2](<pid>)./myspin 4 &

tsh> jobs
[1] (<pid>) Running ./myspin 3 &
[2] (<pid>) Running ./myspin 4 &
tsh> fg %1
Job[1](<pid>)terminated by signal 19
tsh> jobs
[1] (<pid>) Stopped ./myspin 3 &
[2] (<pid>) Running ./myspin 4 &
tsh> bg %3
%3:No such job
tsh> bg %1
[1](<pid>)./myspin 3 &

tsh> jobs
[1] (<pid>) Running ./myspin 3 &
[2] (<pid>) Running ./myspin 4 &
tsh> fg %1
tsh> quit
```
**(注意: `<pid>` 会在实际运行时替换为真实的进程ID)** 