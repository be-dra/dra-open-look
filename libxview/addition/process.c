/*
 * This file is a product of Bernhard Drahota and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Bernhard Drahota to assist in its use, correction,
 * modification or enhancement.
 *
 * BERNHARD DRAHOTA SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Bernhard Drahota be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if B. Drahota has been advised of the possibility of such damages.
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <xview/xview.h>
#include <xview/process.h>
#include <xview_private/i18n_impl.h>

char process_c_sccsid[] = "@(#) %M% V%I% %E% %U% $Id: process.c,v 4.3 2024/11/03 13:00:21 dra Exp $";

typedef enum { NOT_RUNNING, IS_ALIVE, MAYBE_DEAD, IS_DEAD, IS_DONE } chstat_t;

typedef void (*io_proc_t) (Process, char *, int, int);
typedef void (*exit_proc_t) (Process, int, int *);
typedef int (*child_proc_t)(Process);

typedef struct {
	Xv_opaque       public_self;
	char            *program, **argv, **envp, *dir;
	char            own_session, out_is_err, auto_destroy, own_wait;
	char            notify_immediate, wait_for_exec;
	int             input_fd, output_fd, error_fd;
	int             *output_fd_ptr, *error_fd_ptr, pid;
	int             nice, outpipe, errpipe;
	child_proc_t	childproc;
	io_proc_t       outproc, errproc;
	exit_proc_t     exitproc;
	Xv_opaque       client_data;
	Process_status  status;
	int             draErrno;
	chstat_t        alive;
	int       exitstatus;
} Process_private;

#define A0 *attrs
#define A1 attrs[1]
#define ADONE ATTR_CONSUME(*attrs);break
#define PROCPRIV(_x_) XV_PRIVATE(Process_private, Xv_process, _x_)
#define PROCPUB(_x_) XV_PUBLIC(_x_)

#define EXEC_FAILED_EXIT 99

static char **process_path = (char **)0;

static void inform_about_exit(Process_private *priv)
{
	if (priv->exitproc) {
		if (priv->status == PROCESS_EXEC_FAILED &&
			WIFEXITED(priv->exitstatus) &&
			WEXITSTATUS(priv->exitstatus) == EXEC_FAILED_EXIT)
		{
			/* do not call any client exit handler */
			return;
		}
		(*(priv->exitproc))(PROCPUB(priv), priv->pid, &priv->exitstatus);
	}
}

static void check_finished(Process_private *priv)
{
	if (priv->outpipe >= 0) return;
	if (priv->errpipe >= 0) return;
	switch (priv->alive) {
		case IS_ALIVE:
			if (kill(priv->pid, 0)) {
				priv->alive = MAYBE_DEAD;
				/* all pipes closed, kill returns error:
				 * now rather set up a timer !
				 */
/* 				INCOMPLETE */
			}
			break;
		case MAYBE_DEAD:
/* 			INCOMPLETE */
			break;
		case IS_DEAD:
			inform_about_exit(priv);
			if (priv->auto_destroy) xv_destroy_safe(PROCPUB(priv));
			break;
		case IS_DONE:
			if (priv->auto_destroy) xv_destroy_safe(PROCPUB(priv));
			break;
		default: break;
	}
}

static Notify_value note_exit(Notify_client cl, int pid, int *status)
{
	Process self = (Process)cl;
	Process_private *priv = PROCPRIV(self);

	if (WIFSTOPPED(*status)) {
		if (!kill(pid, SIGCONT)) return NOTIFY_DONE;
		return NOTIFY_IGNORED;
	}

	priv->exitstatus = *status;
	if (priv->notify_immediate) {
		inform_about_exit(priv);
		priv->alive = IS_DONE;
	}
	else priv->alive = IS_DEAD;
	check_finished(priv);
	return NOTIFY_DONE;
}

static Notify_value note_error(Notify_client cl, int fd)
{
	Process self = (Process)cl;
	Process_private *priv = PROCPRIV(self);
	int myerr = 0, len;
	char buf[BUFSIZ];

	if ((len = read(fd, buf, sizeof(buf) - 1)) <= 0) {
		if (len == -1) {
			myerr = errno;
			if (myerr == EINTR) return NOTIFY_DONE;
		}
		priv->errpipe = -1;
		notify_set_input_func(cl, NOTIFY_IO_FUNC_NULL, fd);
		close(fd);
	}

	if (len >= 0) {
		buf[len] = '\0';
		myerr = 0;
	}
	if (priv->errproc) (*(priv->errproc))(self, buf, len, myerr);

	if (len <= 0) check_finished(priv);

	return NOTIFY_DONE;
}

static Notify_value note_output(Notify_client cl, int fd)
{
	Process self = (Process)cl;
	Process_private *priv = PROCPRIV(self);
	int myerr = 0, len;
	char buf[BUFSIZ];

	if ((len = read(fd, buf, sizeof(buf) - 1)) <= 0) {
		if (len == -1) {
			myerr = errno;
			if (myerr == EINTR) return NOTIFY_DONE;
		}
		priv->outpipe = -1;
		notify_set_input_func(cl, NOTIFY_IO_FUNC_NULL, fd);
		close(fd);
	}

	if (len >= 0) {
		buf[len] = '\0';
		myerr = 0;
	}
	if (priv->outproc) (*(priv->outproc))(self, buf, len, myerr);

	if (len <= 0) check_finished(priv);

	return NOTIFY_DONE;
}

static void build_path(void)
{
	char *p, *sp;
	int cnt;

	if (process_path) return;

	p = (char *)getenv("PATH");
	if (!p || !*p) p = "/bin:/usr/bin";

	/* this will never be freed */
	sp = xv_strsave(p);

	/* the following should be enough */
	process_path = xv_alloc_n(char *, (strlen(sp) / 3) + 2);
	cnt = 0;
	process_path[0] = strtok(sp, ":");
	if (process_path[0]) {
		while ((process_path[++cnt] = strtok((char *)0, ":")));
	}
}

static char *make_full_path(Process_private *priv)
{
	static char pathbuf[1000];
	int i;

	if (! priv->program) return (char *)0;
	if (priv->program[0] == '/') return priv->program; /* full path */
	if (strchr(priv->program, '/')) return priv->program; /* relative path */

	for (i = 0; process_path[i]; i++) {
		sprintf(pathbuf, "%s/%s", process_path[i], priv->program);
		if (!access(pathbuf, F_OK)) { /* file exists */
			return pathbuf;
		}
	}

	priv->draErrno = ENOENT;
	return (char *)0;
}

static Process_status process_run(Process_private *priv)
{
	int j, i, cpip[2], pip[2], epip[2];
	int wpip[2];
	char full_prog_path[MAXPATHLEN];

	if (! priv->childproc) {
		char *p;

		build_path();
		p = make_full_path(priv);
		if (! p) return PROCESS_NO_EXECUTABLE;
		strcpy(full_prog_path, p);
		if (access(full_prog_path, X_OK)) {
			priv->draErrno = errno;
			return PROCESS_NO_EXECUTABLE;
		}
	}

	priv->status = PROCESS_NO_EXECUTABLE;
	priv->draErrno = ENOENT;

	pip[0] = pip[1] = epip[0] = epip[1] = cpip[1] = -1;

	if (priv->wait_for_exec) {
		if (pipe(cpip)) {
			priv->draErrno = errno;
			return PROCESS_PIPE_FAILED;
		}
	}

	if (priv->outproc || priv->output_fd_ptr) {
		if (pipe(pip)) {
			priv->draErrno = errno;
			if (priv->wait_for_exec) {
				close(cpip[0]);
				close(cpip[1]);
			}
			return PROCESS_PIPE_FAILED;
		}
	}

	if (priv->errproc || priv->error_fd_ptr) {
		if (pipe(epip)) {
			priv->draErrno = errno;
			if (priv->wait_for_exec) {
				close(cpip[0]);
				close(cpip[1]);
			}
			if (pip[0] >= 0) close(pip[0]);
			if (pip[1] >= 0) close(pip[1]);
			return PROCESS_PIPE_FAILED;
		}
	}

	if (pipe(wpip)) {
		priv->draErrno = errno;
		if (priv->wait_for_exec) {
			close(cpip[0]);
			close(cpip[1]);
		}
		if (epip[0] >= 0) close(epip[0]);
		if (epip[1] >= 0) close(epip[1]);
		if (pip[0] >= 0) close(pip[0]);
		if (pip[1] >= 0) close(pip[1]);
		return PROCESS_PIPE_FAILED;
	}

	switch (priv->pid = (int)fork()) {
		case -1:
			priv->draErrno = errno;
			close(wpip[0]);
			close(wpip[1]);
			if (priv->wait_for_exec) {
				close(cpip[0]);
				close(cpip[1]);
			}
			if (pip[0] >= 0) close(pip[0]);
			if (pip[1] >= 0) close(pip[1]);
			if (epip[0] >= 0) close(epip[0]);
			if (epip[1] >= 0) close(epip[1]);
			return PROCESS_FORK_FAILED;

		case 0: /* child process */
			close(wpip[1]);
			do {
				j = read(wpip[0], (char *)&i, 1L);
			} while (j == -1 && errno == EINTR);
			if (j == -1) perror("process_run: error in read");
			close(wpip[0]);

			if (priv->own_session) {
				signal(SIGINT, SIG_DFL);
				if (setsid() < 0) {
					perror("setsid");
				}
			}

			nice(priv->nice);
			if (priv->dir) chdir(priv->dir);

			if (priv->input_fd > 0) dup2(priv->input_fd, 0);
			else if (priv->input_fd < 0) dup2(open("/dev/null", O_RDONLY), 0);

			if (pip[1] >= 0) dup2(pip[1], 1);
			else if (priv->output_fd < 0) dup2(open("/dev/null", O_WRONLY), 1);
			else if (priv->output_fd != 1) dup2(priv->output_fd, 1);

			if (epip[1] >= 0) dup2(epip[1], 2);
			else if (priv->out_is_err) dup2(1, 2);
			else if (priv->error_fd < 0) dup2(open("/dev/null", O_WRONLY), 2);
			else if (priv->error_fd != 2) dup2(priv->error_fd, 2);

			for (i = sysconf(_SC_OPEN_MAX) - 1; i > 2; i--) {
				if (i == cpip[1]) fcntl(i, F_SETFD, 1); /* close-on-exec */
				else close(i);
			}

			if (priv->childproc) {
				return (*(priv->childproc))(PROCPUB(priv));
			}
			else {
				/* careful: this will write into the pipe (if any) */
				if (priv->envp) execve(full_prog_path, priv->argv, priv->envp);
				else execv(full_prog_path, priv->argv);

				if (priv->wait_for_exec) {
					/* if exec succeeded, cpip[1] is now closed, otherwise open */
					i = errno;
					write(cpip[1], (char *)&i, sizeof(i));
				}
				else {
					write(2, "Cannot execv ", 13L);
					perror(full_prog_path);
				}
				exit(EXEC_FAILED_EXIT);
			}

		default:
			break;
	}

	if (!priv->own_wait)
		notify_set_wait3_func(PROCPUB(priv), (Notify_func)note_exit, priv->pid);

	priv->alive = IS_ALIVE;

	if (pip[0] >= 0) {
		close(pip[1]);
		if (priv->outproc) {
			notify_set_input_func(PROCPUB(priv), note_output, pip[0]);
			priv->outpipe = pip[0];
		}
		if (priv->output_fd_ptr) *(priv->output_fd_ptr) = pip[0];
	}

	if (epip[0] >= 0) {
		close(epip[1]);
		if (priv->errproc) {
			notify_set_input_func(PROCPUB(priv), note_error, epip[0]);
			priv->errpipe = epip[0];
		}
		if (priv->error_fd_ptr) *(priv->error_fd_ptr) = epip[0];
	}

	close(wpip[0]);
	close(wpip[1]);

	if (priv->wait_for_exec) {
		int len;

		close(cpip[1]);

		do {
			len = read(cpip[0], (char *)&i, sizeof(i));
		} while (len == -1 && errno == EINTR);

		if (len == sizeof(i)) {
			priv->draErrno = i;
			close(cpip[0]);
			return PROCESS_EXEC_FAILED;
		}
		else if (len == 0) {
			close(cpip[0]);
			return PROCESS_OK;
		}

		perror("read exec-control-pipe");
		return PROCESS_FAILED;
	}
	return PROCESS_OK;
}

static void free_string_list(char **list)
{
	int i;

	if (! list) return;
	for (i = 0; list[i]; i++) xv_free(list[i]);
	xv_free(list);
}

static void new_args(Process_private *priv, char **argv)
{
	unsigned int i, num;

	free_string_list(priv->argv);

	for (num = 0; argv[num]; num++);

	priv->argv = xv_alloc_n(char *, (unsigned long)num+1);
	for (i = 0; i < num; i++) {
		priv->argv[i] = xv_strsave(argv[i]);
	}
}

static void new_env(Process_private *priv, char **envp)
{
	unsigned int i, num;

	free_string_list(priv->envp);

	for (num = 0; envp[num]; num++);

	priv->envp = xv_alloc_n(char *, (unsigned long)num+1);
	for (i = 0; i < num; i++) {
		priv->envp[i] = xv_strsave(envp[i]);
	}
}

static void suicide(Process self)
{
	xv_destroy_safe(self);
}

static int process_init(Xv_opaque owner, Xv_opaque slf, Attr_avlist avlist,
					int *unused)
{
	Xv_process *self = (Xv_process *)slf;
	Process_private *priv = (Process_private *)xv_alloc(Process_private);

	if (!priv) return XV_ERROR;

	priv->public_self = (Xv_opaque)self;
	self->private_data = (Xv_opaque)priv;

	priv->alive = NOT_RUNNING;
	priv->input_fd = 0;
	priv->output_fd = 1;
	priv->error_fd = 2;
	priv->outpipe = priv->errpipe = -1;
	priv->wait_for_exec = TRUE;

	return XV_OK;
}

static Xv_opaque process_set(Process self, Attr_avlist avlist)
{
	Process_private *priv = PROCPRIV(self);
	char *shargv[6];
	Attr_attribute *attrs;

	for (attrs=avlist; *attrs; attrs=attr_next(attrs)) switch ((int)*attrs) {
		case PROCESS_PROGRAM:
			if (priv->program) xv_free(priv->program);
			priv->program = (char *)0;
			if (A1) priv->program = xv_strsave((char *)A1);
			ADONE;
		case PROCESS_ARGV:
			new_args(priv, (char **)A1);
			ADONE;
		case PROCESS_ARGS:
			new_args(priv, (char **)&A1);
			ADONE;
		case PROCESS_ENVIRONMENT:
			new_env(priv, (char **)A1);
			ADONE;
		case PROCESS_OWN_SESSION:
			priv->own_session = (char)A1;
			ADONE;
		case PROCESS_OWN_WAIT:
			priv->own_wait = (char)A1;
			ADONE;
		case PROCESS_INPUT_FD:
			priv->input_fd = (int)A1;
			ADONE;
		case PROCESS_OUTPUT_FD:
			priv->output_fd = (int)A1;
			ADONE;
		case PROCESS_ERROR_FD:
			priv->error_fd = (int)A1;
			ADONE;
		case PROCESS_OUTPUT_FD_PTR:
			priv->output_fd_ptr = (int *)A1;
			ADONE;
		case PROCESS_ERROR_FD_PTR:
			priv->error_fd_ptr = (int *)A1;
			ADONE;
		case PROCESS_DIE:
			xv_perform_soon(self, suicide);
			ADONE;
		case PROCESS_ABORT_CHILD_IO:
			if (priv->outpipe >= 0) {
				notify_set_input_func(self, NOTIFY_IO_FUNC_NULL, priv->outpipe);
				close(priv->outpipe);
				priv->outpipe = -1;
			}
			if (priv->errpipe >= 0) {
				notify_set_input_func(self, NOTIFY_IO_FUNC_NULL, priv->errpipe);
				close(priv->errpipe);
				priv->errpipe = -1;
			}
			ADONE;
		case PROCESS_CHILD_FUNCTION:
			priv->childproc = (child_proc_t)A1;
			priv->wait_for_exec = FALSE;
			ADONE;
		case PROCESS_OUTPUT_PROC:
			priv->outproc = (io_proc_t)A1;
			ADONE;
		case PROCESS_ERROR_PROC:
			priv->errproc = (io_proc_t)A1;
			ADONE;
		case PROCESS_EXIT_PROC:
			priv->exitproc = (exit_proc_t)A1;
			ADONE;
		case PROCESS_OUTPUT_IS_ERROR:
			priv->out_is_err = (char)A1;
			ADONE;
		case PROCESS_RUN:
			priv->status = process_run(priv);
			ADONE;
		case PROCESS_KILL:
			if (priv->pid > 0) kill(priv->pid, (int)A1);
			ADONE;
		case PROCESS_AUTO_DESTROY:
			priv->auto_destroy = (char)A1;
			ADONE;
		case PROCESS_SHELL_COMMAND:
			shargv[0] = "sh";
			shargv[1] = "-c";
			shargv[2] = (char *)A1;
			shargv[3] = (char *)0;
			new_args(priv, shargv);
			if (priv->program) xv_free(priv->program);
			priv->program = xv_strsave("/bin/sh");
			ADONE;
		case PROCESS_DIRECTORY:
			if (priv->dir) xv_free(priv->dir);
			priv->dir = 0;
			if (A1) priv->dir = xv_strsave((char *)A1);
			ADONE;
		case PROCESS_NICE:
			priv->nice = (int)A1;
			ADONE;
		case PROCESS_NOTIFY_IMMEDIATE: 
			priv->notify_immediate = (char)A1;
			ADONE;
		case PROCESS_WAIT_FOR_EXEC: 
			priv->wait_for_exec = (char)A1;
			ADONE;
		case PROCESS_CLIENT_DATA:
			priv->client_data = (Xv_opaque)A1;
			ADONE;
		default: xv_check_bad_attr(PROCESS, A0);
			break;
	}

	return XV_OK;
}

/*ARGSUSED*/
static Xv_opaque process_get(Process self, int *status, Attr_attribute attr, va_list vali)
{
	Process_private *priv = PROCPRIV(self);

	*status = XV_OK;
	switch ((int)attr) {
		case PROCESS_PROGRAM: return (Xv_opaque)priv->program;
		case PROCESS_ARGV: return (Xv_opaque)priv->argv;
		case PROCESS_ENVIRONMENT: return (Xv_opaque)priv->envp;
		case PROCESS_OWN_SESSION: return (Xv_opaque)priv->own_session;
		case PROCESS_OWN_WAIT: return (Xv_opaque)priv->own_wait;
		case PROCESS_INPUT_FD: return (Xv_opaque)priv->input_fd;
		case PROCESS_OUTPUT_FD: return (Xv_opaque)priv->output_fd;
		case PROCESS_ERROR_FD: return (Xv_opaque)priv->error_fd;
		case PROCESS_OUTPUT_FD_PTR: return (Xv_opaque)priv->output_fd_ptr;
		case PROCESS_ERROR_FD_PTR: return (Xv_opaque)priv->error_fd_ptr;
		case PROCESS_CHILD_FUNCTION: return (Xv_opaque)priv->childproc;
		case PROCESS_OUTPUT_PROC: return (Xv_opaque)priv->outproc;
		case PROCESS_ERROR_PROC: return (Xv_opaque)priv->errproc;
		case PROCESS_EXIT_PROC: return (Xv_opaque)priv->exitproc;
		case PROCESS_OUTPUT_IS_ERROR: return (Xv_opaque)priv->out_is_err;
		case PROCESS_PID: return (Xv_opaque)priv->pid;
		case PROCESS_AUTO_DESTROY: return (Xv_opaque)priv->auto_destroy;
		case PROCESS_STATUS: return (Xv_opaque)priv->status;
		case PROCESS_ERRNO: return (Xv_opaque)priv->draErrno;
		case PROCESS_ERROR_DESCRIPTION:
			{
				static char buf[500];

				switch (priv->status) {
					case PROCESS_OK: return XV_NULL;
					case PROCESS_PIPE_FAILED:
						sprintf(buf, XV_MSG("Cannot open pipe: %s"),
													strerror(priv->draErrno));
						break;
					case PROCESS_FORK_FAILED:
						sprintf(buf, XV_MSG("Cannot fork: %s"),
													strerror(priv->draErrno));
						break;
					case PROCESS_NO_EXECUTABLE:
					case PROCESS_EXEC_FAILED:
						sprintf(buf, XV_MSG("Cannot execve '%s':\n%s"),
									priv->program ? priv->program : "(null)",
									strerror(priv->draErrno));
						break;
					case PROCESS_FAILED:
						strcpy(buf, XV_MSG("Failed from unknown reasons"));
						break;
					
				}
				return (Xv_opaque)buf;
			}
		case PROCESS_NICE: return (Xv_opaque)priv->nice;
		case PROCESS_WAIT_FOR_EXEC: return (Xv_opaque)priv->wait_for_exec;
		case PROCESS_NOTIFY_IMMEDIATE: return (Xv_opaque)priv->notify_immediate;
		case PROCESS_CLIENT_DATA: return priv->client_data;
		default:
			*status = xv_check_bad_attr(PROCESS, attr);
			return (Xv_opaque)XV_OK;
	}
}

static int process_destroy(Process self, Destroy_status status)
{
	if (status == DESTROY_CLEANUP) {
		Process_private *priv = PROCPRIV(self);

		if (priv->program) xv_free(priv->program);
		if (priv->dir) xv_free(priv->dir);
		free_string_list(priv->argv);
		free_string_list(priv->envp);
		xv_free(priv);
	}
	return XV_OK;
}

Xv_pkg xv_process_pkg = {
	"Process",
	ATTR_PKG_PROCESS,
	sizeof(Xv_process),
	XV_GENERIC_OBJECT,
	process_init,
	process_set,
	process_get,
	process_destroy,
	0
};
