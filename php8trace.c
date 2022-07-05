#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

static struct termios orig_termios;
static int ttyfd = STDIN_FILENO;

void tty_reset() {
	tcsetattr(ttyfd,TCSAFLUSH,&orig_termios);
}

void tty_raw() {
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(tty_reset);
	struct termios raw = orig_termios;
	raw.c_lflag &= ~( ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int g_child_pid;

void child_exit(int signal) {
	int wstatus;
	pid_t pid = waitpid(-1, &wstatus, WNOHANG);

	// printf("child %d exit WIFSTOPPED=%d WIFCONTINUED=%d WIFEXITED=%d WIFSIGNALED=%d WTERMSIG=%d\n",
	// 		pid,
	// 		WIFSTOPPED(wstatus),
	// 		WIFCONTINUED(wstatus),
	// 		WIFEXITED(wstatus),
	// 		WIFSIGNALED(wstatus),
	// 		WTERMSIG(wstatus));

	if (pid == 0)
		return ;

	if (pid == g_child_pid && WIFSIGNALED(wstatus)) {
		if (WTERMSIG(wstatus) < SIGSYS) {
			exit(1);
		}
	}

	if (pid == g_child_pid && WIFEXITED(wstatus))
		exit(WEXITSTATUS(wstatus));
}
void on_sigstop(int signal) {
	puts("forwarding SIGTSTP");
	kill(g_child_pid, SIGTSTP);
}
void on_sigint(int signal) {
	puts("forwarding SIGINT");
	kill(g_child_pid, SIGINT);
}

int main(int argc, char **argv) {

	int pipe_stdin[2];

	pipe(pipe_stdin);

	if (!(g_child_pid = fork())) {
		printf("starting php process with pid [ %d ]\n",getpid());

		close(pipe_stdin[1]);
		close(0);
		dup2(pipe_stdin[0], 0);

		char **cloned_argv = calloc((argc+5) * sizeof(char *), 1);
		int i;
		cloned_argv[0] = "php";
		cloned_argv[1] = "-d";
		cloned_argv[2] = "extension=observer.so";
		cloned_argv[3] = "-d";
		cloned_argv[4] = "observer.instrument=1";

		for(i=1;i<argc;i++) {
			cloned_argv[i+4] = strdup(argv[i]);
		}
		execvp("php", cloned_argv);
		perror("woot");
	}

	signal(SIGCHLD, child_exit);
	signal(SIGTSTP, on_sigstop);
	signal(SIGINT, on_sigint);
	close(pipe_stdin[0]);
	tty_raw();

	uint8_t c;
	while (read(STDIN_FILENO, &c, 1) == 1) {
		if (iscntrl(c)) {
			if (c == '') {
				kill(g_child_pid, SIGUSR1);
			}
			if (c == '') {
				kill(g_child_pid, SIGUSR2);
			}
			if (c == '') {
				close(pipe_stdin[1]);
			}
			if (c == '') {
				kill(g_child_pid, SIGCONT);
			}
		} 
		write(pipe_stdin[1], &c, 1);
	}

	kill(g_child_pid, SIGKILL);

	return 0;
}
