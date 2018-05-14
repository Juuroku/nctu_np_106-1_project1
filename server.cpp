#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <regex>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#define MAXLINE 10001


int sockfd;

void init()
{
	char* home = getenv("HOME");
	chdir(home);
	chdir("ras");
	setenv("PATH", "bin:.", 1);
	return;
}

void welcome_msg(int sockfd)
{
	int n;
	char wm[1001] = {"****************************************\n** Welcome to the information server. **\n****************************************\n% "};
	n = send(sockfd, wm, strlen(wm), 0);
	if(n < 0) printf("server: send error\n");
	return;
}

void line_promt(int sockfd)
{
	int n;
	char line[5] = {"% "};
	n = send(sockfd, line, strlen(line), 0);
	if(n < 0) printf("server: send error\n");
	return;
}

void str_echo(int sockfd)
{
	int n, i, k, line_i, read_i, j;
	char cmd_readbuffer[MAXLINE];
	char tmpbuffer[MAXLINE];
	char line[MAXLINE];
	char err_msg1[1001] = {"The command which includes '/' is illegal.\n"};
	// char test_msg1[1001] = {"Input recieved.\n"};
	char program[257];
	char tmp[257][257];
	char* arg[257];
	char pe[] = "printenv";
	char se[] = "setenv";
	char ex[] = "exit";
	int redirection; // 0 if don't need , 1 if need
	char re_dir[257];
	int pipefd_1[2], pipefd_2[2];
	int pipe_mode; // '|' -> pipe_mode = 0 , '|n' (n = 1-1000) pipe_mode = 1 , close -> -1
	int pipe_number; // put output data to which buffer
	int pipe_index; // index of which buffer is use now
	std::string pipebuffer[1001];
	std::string pipe0buffer;
	int pipe_flg[1001]; // 0 if pipe is empty  1 for something inside in mode0, 2 for something inside in mode1 , 3 for both mode0 and mode1
	int cmdc;
	int error_in_line;
	int flg;

	error_in_line = 0;

	redirection = 0;

	memset(cmd_readbuffer, 0, sizeof(cmd_readbuffer));

	for(i = 0;i < 1001; i++) pipe_flg[i] = 0;

	pipe_mode = -1;
	pipe_number = 0;
	pipe_index = 0;

	n = 0;
		
	for(;;)
	{
		flg = 0;	
		cmdc = 0;
		n = 0;
		memset(line, 0, sizeof(line));
		j = strlen(cmd_readbuffer);
		//printf("j : %d\n",j);
		if(!j) j = recv(sockfd, cmd_readbuffer, sizeof(cmd_readbuffer), 0);
		if(j < 0 && errno != EINTR)
		{
			printf("server: receive error\n");
			return;
		}
		else if(j == 0) return;
		for(i = 0; cmd_readbuffer[i-1]!='\n' && cmd_readbuffer[i]!='\0';i++) line[i] = cmd_readbuffer[i];
		line[i] = '\0';
		n = i;
		//printf("n : %d\n",n);
		if (cmd_readbuffer[i]!='\0')
		{
			j = i;
			for(i = 0;cmd_readbuffer[i+j]!='\0';i++)
			{
				cmd_readbuffer[i] = cmd_readbuffer[i+j];
			}
		}
		else memset(cmd_readbuffer, 0, sizeof(cmd_readbuffer));
		while(line[n-1]!='\n')
		{
			j = recv(sockfd, cmd_readbuffer, sizeof(cmd_readbuffer), 0);
			while(j <= 0)
			{
				if(errno == EINTR) j = recv(sockfd, cmd_readbuffer, sizeof(cmd_readbuffer), 0);
				else break;
			}
			if(j <= 0)
			{
				printf("server: receive error\n");
			}
			for(i = 0; cmd_readbuffer[i-1]!='\n' && cmd_readbuffer[i]!='\0';i++) line[i+n] = cmd_readbuffer[i];
			line[i + n] = '\0';
			n = i + n;
			//printf("n : %d\n",n);
			if (cmd_readbuffer[i]!='\0')
			{
				j = i;
				for(i = 0;cmd_readbuffer[i+j]!='\0';i++)
				{
					cmd_readbuffer[i] = cmd_readbuffer[i+j];
				}
			}
			else memset(cmd_readbuffer, 0, sizeof(cmd_readbuffer));
		}
		line[n - 1] = '\0';
		//printf("strlen : %d\n",strlen(line));
		n--;
		//n = read(sockfd, line, sizeof(line));
		if(n < 0) printf("server: receive error\n");
		else if(n == 0) return;
		else
		{
			//printf("n : %d\n",n);
			//printf("%s\n",line);
			error_in_line = 0;
			for(i = 0;i <= n;i++) // test if there is any '/' in input
			{
				if(line[i] == '/')
				{
					k = send(sockfd, err_msg1, strlen(err_msg1), 0);
					if(k < 0) printf("server: send error\n");
					break;
				}
			}
			if(i == n+1)
			{
				//k = send(sockfd, test_msg1, strlen(test_msg1), 0);
				//if(k < 0) printf("server: send error\n");

				// split readline by space
				std::regex re("\\s+");
				std::string s(line);
				std::vector<std::string> splited_command
				{
					std::sregex_token_iterator(s.begin(), s.end(), re, -1), {}
				};

				k = splited_command.size();
				printf("k : %d\n",k);
				int cnt, l;
				cnt = 0;
				pipe_number = 0;
				pipe_mode = -1;
				for(i = 0;i < k;i++)
				{
					//printf("%s\n",splited_command[i].c_str());
					if(splited_command[i][0] == '|' || splited_command[i][0] == '>' || splited_command[i][0] == '&')
					{
						cmdc++;
						//for(l = 0;l <= 1000;l++) if(pipe_flg[l] != 0) printf("%d:%d\n",l,pipe_flg[l]);
						pipe(pipefd_1);
						pipe(pipefd_2);
						if(splited_command[i][0] == '|')
						{
							if(splited_command[i].size() == 1)
							{
								pipe_mode = 0;
								pipe_number = 1;
								if(pipe_flg[(pipe_index+1)%1001] == 2 || pipe_flg[(pipe_index+1)%1001] == 3) pipe_flg[(pipe_index+1)%1001] = 3;
								else pipe_flg[(pipe_index+1)%1001] = 1;
							}
							else
							{
								pipe_number = 0;
								for(l = 1;l < splited_command[i].size();l++) pipe_number = pipe_number * 10 + ( splited_command[i][l] - '0' );
								//printf("pipe_number: %d\n",pipe_number);
								pipe_mode = 1;
								if(pipe_flg[(pipe_index+pipe_number)%1001] == 1 || pipe_flg[(pipe_index+pipe_number)%1001] == 1) pipe_flg[(pipe_index+pipe_number)%1001] = 3;
								else pipe_flg[(pipe_index+pipe_number)%1001] = 2;
							}
						}
						else if(splited_command[i][0] == '&') flg = 1;
						else
						{
							redirection = 1;
							strcpy(re_dir, splited_command[i+1].c_str()); 
							i++;
						}
						if(strlen(program) != 0)
						{
							arg[cnt] = NULL;
							pid_t pid;
							if(!strcmp(program,pe))
							{
							close(pipefd_1[0]);
							close(pipefd_1[1]);
							close(pipefd_2[0]);
							close(pipefd_2[1]);
								char* pPath;
								char msg[] = "";
								pPath = getenv(arg[1]);
								if(pPath == NULL) pPath = msg;
								std::string s1(arg[1]);
								std::string s2(pPath);
								std::string s3 = s1 + "=" + s2 + "\n";
								l = send(sockfd, s3.c_str(), strlen(s3.c_str()), 0);
							}
							else if(error_in_line && flg);
							else if(!strcmp(program,se))
							{
							close(pipefd_1[0]);
							close(pipefd_1[1]);
							close(pipefd_2[0]);
							close(pipefd_2[1]);
								//printf("se\n");
								if(setenv(arg[1], arg[2], 1))
								{
									char msg[] = "error setting enviroment variable\n";
									l = send(sockfd, msg, strlen(msg), 0);
									if(l < 0) printf("server: error send\n");
								}
							}
							else if(!strcmp(program,ex)) 
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								return;
							}
							else if((pid = fork()) < 0)
							{
							close(pipefd_1[0]);
							close(pipefd_1[1]);
							close(pipefd_2[0]);
							close(pipefd_2[1]);
								printf("server: fork error\n");
								exit(-1);
							}
							else if(pid == 0)
							{
								dup2(sockfd, STDERR_FILENO);
								//printf("Child: %s\n",program);
								//printf("pipe_mode: %d\n",pipe_mode);
								//printf("redirection: %d\n",redirection);
								printf("pipe_index: %d\n",pipe_index);
								if(pipe_flg[pipe_index])
								{
									// child read from pipefd_1
									//printf("Child: read pipe %d\n",pipefd_1[0]);
									close(pipefd_1[1]);
									dup2(pipefd_1[0],0);
									close(pipefd_1[0]);
								}
								else
								{
									close(pipefd_1[0]);
									close(pipefd_1[1]);
								}
								if(pipe_mode == -1 && redirection == 0)
								{
									//printf("Child: dup2 sockfd\n");
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									dup2(sockfd, STDOUT_FILENO);
								}
								else if(redirection)
								{
									//printf("Child: redirection\n");
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									int fd;
									fd = creat(re_dir, 0644);
									dup2(fd, STDOUT_FILENO);
								}
								else
								{
									// child write to pipefd_2
									//printf("Child: write pipe %d\n",pipefd_2[1]);
									close(pipefd_2[0]);
									dup2(pipefd_2[1],1);
									close(pipefd_2[1]);
								}
								if(execvp(program, arg) == -1)
								{
									if(redirection == 0 && pipe_mode == -1) close(STDOUT_FILENO);
									if(errno == ENOENT)
									{
										char msg1[] = {"Unknown command: ["};
										char msg2[] = {"].\n"};
										std::string msg = std::string(msg1) + std::string(program) + std::string(msg2);
										l = send(sockfd, msg.c_str(), strlen(msg.c_str()), 0);
										if(l < 0) printf("sever: send error\n");
									}
									else
									{
										char* msg = strerror(errno);
										l = send(sockfd, msg, strlen(msg), 0);
										if(l < 0) printf("sever: send error\n");
									}	
									exit(1);
								}
							}
							else
							{
								//printf("server: parent\n");
								printf("pipe_flg: %d\n",pipe_flg[pipe_index]);
								if(pipe_flg[pipe_index])
								{
									// parent write to pipefd_1
									//printf("server: write pipe %d\n",pipefd_1[1]);
									close(pipefd_1[0]);
									if(pipe_flg[pipe_index] == 2) 
									{
									//	printf("%s",pipebuffer[pipe_index].c_str());
										write(pipefd_1[1], pipebuffer[pipe_index].c_str(), strlen(pipebuffer[pipe_index].c_str()));
									}
									else if(pipe_flg[pipe_index] == 1)
									{
									//	printf("%s",pipe0buffer.c_str());
										write(pipefd_1[1], pipe0buffer.c_str(), strlen(pipe0buffer.c_str()));
										pipe0buffer.clear();
									}
									else
									{
										std::string buffer = pipebuffer[pipe_index] + pipe0buffer;
									//	printf("%s",buffer.c_str());
										write(pipefd_1[1], buffer.c_str(), strlen(buffer.c_str()));
										pipe0buffer.clear();
									}
									close(pipefd_1[1]);
								}
								else
								{
									close(pipefd_1[1]);
									close(pipefd_1[0]);
								}
								printf("pipe_mode: %d\n",pipe_mode);
								if(pipe_mode == -1)
								{
									close(pipefd_2[0]);
									close(pipefd_2[1]);
								}
								else
								{
									char readbuffer[65537];
									// parent read from pipifd_2
									//printf("server: read pipe %d\n",pipefd_2[0]);
									close(pipefd_2[1]);
									l = read(pipefd_2[0], readbuffer, sizeof(readbuffer));
									readbuffer[l] = '\0';
									//printf("%s",readbuffer);
									if(pipe_mode == 0) pipe0buffer = std::string(readbuffer);
									else if(pipe_mode == 1) pipebuffer[(pipe_index+pipe_number)%1001] = pipebuffer[(pipe_index+pipe_number)%1001] + std::string(readbuffer);
									close(pipefd_2[0]);
								}

								int status;								
								wait(&status);
							printf("server: receive waiting status\n");
								pipe_mode = -1;
								if(!status) // check if child has any error
								{
									//printf("server: child exit without error\n");
									pipe_flg[pipe_index] = 0;
										pipebuffer[pipe_index].clear();
									pipe_index++;
									if(pipe_index > 1000) pipe_index = 0;
									redirection = 0;
								}
								else
								{
									printf("server: child exit with error\ni = %d\n",i);
									if(cmdc == 0)
									{
										pipe_flg[pipe_index] = 0;
										pipebuffer[pipe_index].clear();
										pipe_index++;
										if(pipe_index > 1000) pipe_index = 0;
									}
									redirection = 0;
									error_in_line = 1;
									break;
								}
							}
						}
						cnt = 0;
						program[0] = '\0';
					}
					else
					{
						for(l = 0;l < splited_command[i].size();l++)
						{
							//printf("%d ", l);
							if(cnt == 0) program[l] = splited_command[i][l];
							tmp[cnt][l] = splited_command[i][l];
						}
						//printf("\n");
						if(cnt == 0)program[l] = '\0';
						tmp[cnt][l] = '\0';
						arg[cnt] = tmp[cnt];
						cnt++;
					}
				} // end of for(k)
				if(error_in_line);
				else if(strlen(program) != 0)
				{
					
						//for(l = 0;l <= 1000;l++) if(pipe_flg[l] != 0) printf("%d:%d\n",l,pipe_flg[l]);
						arg[cnt] = NULL;
						pipe(pipefd_1);
						pipe(pipefd_2);
						pid_t pid;
						if(error_in_line && flg);
						else if(!strcmp(program,pe))
						{
							close(pipefd_1[0]);
							close(pipefd_1[1]);
							close(pipefd_2[0]);
							close(pipefd_2[1]);
							char* pPath;
							char msg[] = "";
							pPath = getenv(arg[1]);
							if(pPath == NULL) pPath = msg;
							std::string s1(arg[1]);
							std::string s2(pPath);
							std::string s3 = s1 + "=" + s2 + "\n";
							l = send(sockfd, s3.c_str(), strlen(s3.c_str()), 0);
						}
						else if(!strcmp(program,se))
						{
							//printf("se\n");
							char* pPath;
							close(pipefd_1[0]);
							close(pipefd_1[1]);
							close(pipefd_2[0]);
							close(pipefd_2[1]);
							if(setenv(arg[1], arg[2], 1))
							{
								char msg[] = "error setting enviroment variable\n";
								l = send(sockfd, msg, strlen(msg), 0);
								if(l < 0) printf("server: error send\n");
							}
						}
						else if(!strcmp(program,ex))
						{
							close(pipefd_1[0]);
							close(pipefd_1[1]);
							close(pipefd_2[0]);
							close(pipefd_2[1]);
							return;
						}
						else if((pid = fork()) < 0)
						{
							close(pipefd_1[0]);
							close(pipefd_1[1]);
							close(pipefd_2[0]);
							close(pipefd_2[1]);
							char* pPath;
							printf("server: fork error\n");
							exit(-1);
						}
						else if(pid == 0)
						{
								dup2(sockfd, STDERR_FILENO);
							//printf("Child: %s\n",program);
							//printf("pipe_mode: %d\n",pipe_mode);
							//printf("redirection: %d\n",redirection);
							//	printf("pipe_index: %d\n",pipe_index);
							if(pipe_flg[pipe_index])
							{
								// child read from pipefd_1
								//printf("Child: read pipe %d\n",pipefd_1[0]);
								close(pipefd_1[1]);
								dup2(pipefd_1[0],0);
								close(pipefd_1[0]);
							}
							else
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
							}
							if(pipe_mode == -1 && redirection == 0)
							{
							//	printf("Child: dup2 sockfd\n");
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								dup2(sockfd, STDOUT_FILENO);
							}
							else if(redirection)
							{
							//	printf("Child: redirection\n");
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								int fd;
								fd = creat(re_dir, 0644);
								dup2(fd, STDOUT_FILENO);
							}
							else
							{
								// child write to pipefd_2
								//printf("Child: write to pipe %d\n",pipefd_2[1]);
								close(pipefd_2[0]);
								dup2(pipefd_2[1],1);
								close(pipefd_2[1]);
							}
							if(execvp(program, arg) == -1)
							{
									if(redirection == 0 && pipe_mode == -1) close(STDOUT_FILENO);
								if(errno == ENOENT)
								{
									char msg1[] = {"Unknown command: ["};
									char msg2[] = {"].\n"};
									std::string msg = std::string(msg1) + std::string(program) + std::string(msg2);
									l = send(sockfd, msg.c_str(), strlen(msg.c_str()), 0);
									if(l < 0) printf("sever: send error\n");
								}
								else
								{
									char* msg = strerror(errno);
									l = send(sockfd, msg, strlen(msg), 0);
									if(l < 0) printf("sever: send error\n");
								}	
								exit(1);
							}
						}
						else
						{
							//printf("server: parent\n");
							//printf("pipe_flg: %d\n",pipe_flg[pipe_index]);
							if(pipe_flg[pipe_index])
							{
								// parent write to pipefd_1
								//printf("server: write pipe %d\n",pipefd_1[1]);
								close(pipefd_1[0]);
								if(pipe_flg[pipe_index] == 2)
								{
								//		printf("%s",pipebuffer[pipe_index].c_str());
									write(pipefd_1[1], pipebuffer[pipe_index].c_str(), strlen(pipebuffer[pipe_index].c_str()));
								}
								else if(pipe_flg[pipe_index] == 1)
								{
								//		printf("%s",pipe0buffer.c_str());
									write(pipefd_1[1], pipe0buffer.c_str(), strlen(pipe0buffer.c_str()));
									pipe0buffer.clear();
								}
								else
								{
									std::string buffer = pipebuffer[pipe_index] + pipe0buffer;
								//		printf("%s",buffer.c_str());
									write(pipefd_1[1], buffer.c_str(), strlen(buffer.c_str()));
										pipe0buffer.clear();
								}
								close(pipefd_1[1]);
							}
							else
							{
								close(pipefd_1[1]);
								close(pipefd_1[0]);
							}
							if(pipe_mode == -1)
							{
								close(pipefd_2[0]);
								close(pipefd_2[1]);
							}
							else
							{
								char readbuffer[65537];
								// parent read from pipefd_2
								//printf("server: read pipe %d\n",pipefd_2[0]);
								close(pipefd_2[1]);
								l = read(pipefd_2[0], readbuffer, sizeof(readbuffer));
								readbuffer[l] = '\0';
								//		printf("%s",readbuffer);
								if(pipe_mode == 0) pipe0buffer = std::string(readbuffer);
								else if(pipe_mode == 1) pipebuffer[(pipe_index+pipe_number)%1001] = pipebuffer[(pipe_index+pipe_number)%1001] + std::string(readbuffer);
								close(pipefd_2[0]);
							}

							int status;								
							wait(&status);
							printf("server: receive waiting status\n");
							pipe_mode = -1;
							if(!status) // check if child has any error
							{
							//	printf("server: chile exit without error\n");
								pipe_flg[pipe_index] = 0;
										pipebuffer[pipe_index].clear();
								pipe_index++;
								if(pipe_index > 1000) pipe_index = 0;
									redirection = 0;
							}
							else
							{
								printf("server: chile exit with error\ni = %d\n",i);
								if(cmdc == 0)
								{
									pipe_flg[pipe_index] = 0;
										pipebuffer[pipe_index].clear();
									pipe_index++;
									if(pipe_index > 1000) pipe_index = 0;
								}
									redirection = 0;
									error_in_line = 1;
							}
						}// end of program execute
				}// end of if error_in_line and program check
			}// end of if / check
		}// end of if recv check
		line_promt(sockfd);
		printf("server: promt\n");
	}

}

static void s_exit(int dunno)
{
	int status;
	wait(&status);
	exit(0);
}

int main()
{
	signal(SIGINT, s_exit);
	int  newsockfd, childpid;
	unsigned int clilen;
	struct sockaddr_in cli_addr, serv_addr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) printf("server: can't open stream socket\n");
	
	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(7016);

	if(bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0) printf("server: can't bind local address\n");

	init();

	listen(sockfd, 5);

	for(;;)
	{
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd,(struct sockaddr*) &cli_addr,&clilen);
		if(newsockfd < 0) printf("server: accept error\n");
		if((childpid = fork()) < 0) printf("server: fork error\n");
		else if(childpid == 0) // child
		{
			close(sockfd);
			welcome_msg(newsockfd); // write welcome messege
			str_echo(newsockfd); // execute function
			exit(0);
		}
		close (newsockfd);
	}

		int status;
		wait(&status);
		if(status) printf("server: forc exec error\n");
	return 0;
}
