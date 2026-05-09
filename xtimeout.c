#define _POSIX_C_SOURCE 200809L
#ifdef _WIN32
#  error MS Windows not supported.
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <getopt.h>
#include <stdbool.h>

#define NL "\n"
#define ENV(VAR) "\"${" VAR "}\""



char HELP[] =
    "Usage: xtimeout [OPTS...] [-x POPUP_MESSAGE] DURATION COMMAND..."            NL
    ""                                                                            NL
    "Start COMMAND, [show a popup and] kill it if still running after DURATION."  NL
    ""                                                                            NL
    "  -V           : Dump configuration (environment variables)."                NL
    "  -k DURATION  : Also send a KILL if running after the initial signal."      NL
    "  -s NUMBER    : Terminate with this signal, by default SIGTERM."            NL
    "  -n MESSAGE   : Send a notification when timed out."                        NL
    "  -X DURATION  : Popup message timeout, by default one minute."              NL
    ""                                                                            NL
    "Duration format: 1w2d3h30m, 3h50m, ..."                                      NL
    "When the program times out the exit status is 124. If the user declines the" NL
    "kill it will ask again after DURATION."                                      NL
    ;

char const *xtimeout_popup_env =
    "xmessage"
    " -buttons Kill:0,Keep:2"
    " ${__TIMEOUT:+ -timeout \"${__TIMEOUT}\" }"
    " -center \"${__MESSAGE}\" >/dev/null";
/* zenity --question ${__TIMEOUT:+--timeout="${__TIMEOUT}"} --text="${__MESSAGE}" */
char const *notify_send_env  =
    "notify-send -a";

static int  xtimeout_popup(char const _msg[], long _timeout, bool *_res);
static int  notify_send(char const _title[], char const _message[]);

static int  duration_parse(char const _cad[], long *_out);
static int  pipe_timeout(int pipe_rd, long seconds);

int
main(int _argc, char *_argv[])
{
	pid_t   pid = -1;
	int     p[2] = { -1, -1 };

	long    term_duration      = 0;
	int     term_signal        = SIGTERM;
	long    kill_duration      = 0;
	char   *send_notification  = NULL;
	long    popup_duration     = 60;
	char   *popup_message      = NULL;

	int     exit_code = 1;
	int     status;
	int     opt, e;
	char   *env;

	if (_argc == 1 || !strcmp(_argv[1], "-h") || !strcmp(_argv[1], "--help")) {
		puts(HELP);
		return 0;
	}

	if ((env = getenv("XTIMEOUT_POPUP"))) {
		xtimeout_popup_env = env;
	}
	if ((env = getenv("NOTIFY_SEND"))) {
		notify_send_env = env;
	}

	while ((opt = getopt(_argc, _argv, "Vx:k:s:n:X:")) != -1) {
		e = 0;
		switch (opt) {
		case 'V':
			printf(
			    "XTIMEOUT_POPUP : %s" NL
			    "NOTIFY_SEND    : %s" NL,
			    xtimeout_popup_env,
			    notify_send_env
			);
			return 0;
		case 'x': popup_message  = optarg;                         break;
		case 'X': popup_duration = 0; e = duration_parse(optarg, &popup_duration); break;
		case 'k': kill_duration = 0;  e = duration_parse(optarg, &kill_duration);  break;
		case 's': term_signal   = atoi(optarg);                   break;
		case 'n': send_notification = optarg;                     break;
		case '?':
		default:
			return 1;
		}
		if (e<0/*err*/) {
			return 1;
		}
	}
	if (!_argv[optind]) {
		fprintf(stderr, "xtimeout: Missing DURATION.\n");
		return 1;
	}
	if (!_argv[optind+1]) {
		fprintf(stderr, "xtimeout: missing COMMAND\n");
		return 1;
	}
	term_duration = 0;
	e = duration_parse(_argv[optind++], &term_duration);
	if (e==-1/*err*/) { return 1; }

	e = pipe(p);
	if (e==-1/*err*/) { perror("pipe"); goto cleanup; }

	pid = fork();
	if (pid < 0/*err*/) { perror("fork"); goto cleanup; }
	if (pid == 0) {
		close(p[0]);
		execvp(_argv[optind], &_argv[optind]);
		perror(_argv[optind]);
		_exit(127);
	}
	close(p[1]);
	p[1] = -1;

	while (1) {
		e = pipe_timeout(p[0], term_duration);

		/* Pipe closed = child exited. */
		if (e > 0) {
			break;
		}
		exit_code = 124;

		/* Timed out, but user declined kill. */
		if (popup_message) {
			bool resp = true;
			xtimeout_popup(popup_message, popup_duration, &resp);
			/* The response ignored, if asking fails kill the program. */
			if (!resp) {
				continue;
			}
		}

		/* Terminate the program, and go to wait when kill not needed. */
		kill(pid, term_signal);
		if (!kill_duration) {
			break;
		}

		/* Kill the program if needed. */
		e = pipe_timeout(p[0], kill_duration);
		if (e > 0) {
			break;
		}
		kill(pid, SIGKILL);
		break;
	}

	waitpid(pid, &status, 0);
	pid = -1;
	if (exit_code != 124 && WIFEXITED(status)) {
		exit_code = WEXITSTATUS(status);
	}

	if (send_notification && exit_code == 124) {
		notify_send("Program killed", send_notification);
	}
	
	cleanup:
	if (p[0] != -1) close(p[0]);
	if (p[1] != -1) close(p[1]);
	if (pid > 0) {
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
	}
	return exit_code;
}

static int
xtimeout_popup(char const _msg[], long _timeout, bool *_res)
{
	char timeout_s[64] = {0};
	if (_timeout) {
		snprintf(timeout_s, sizeof(timeout_s)-1, "%li", _timeout);
	}
	setenv("__TIMEOUT", timeout_s, 1);
	setenv("__MESSAGE", _msg, 1);
	int ret = system(xtimeout_popup_env);
	unsetenv("__TIMEOUT");
	unsetenv("__MESSAGE");
	ret = WEXITSTATUS(ret);
	if (ret == 0) {
		*_res = true;
		return 0;
	} else if (ret == 2) {
		*_res = false;
		return 0;
	} else {
		fprintf(stderr, "xtimeout: Can't show a popup, exit code: %i\n", ret);
		return -1;
	}
}

static int
notify_send(char const _title[], char const _message[])
{
	size_t l = strlen(notify_send_env)+64;
	char b[l];
	snprintf(b, l-1, "%s xtimeout %s %s >/dev/null", notify_send_env, ENV("__TITLE"), ENV("__MESSAGE"));
	setenv("__TITLE", _title, 1);
	setenv("__MESSAGE", _message, 1);
	int ret = system(b);
	unsetenv("__TITLE");
	unsetenv("__MESSAGE");
	return (ret==0)?0:-1;
}

static int
duration_parse(char const _cad[], long *_ret)
{
	char *ptr = NULL;
	if (*_cad=='\0') {
		return 0;
	} else if (!strchr("0123456789", *_cad)) {
		fprintf(stderr, "xtimeout: Invalid duration: %s\n", _cad);
		return -1;
	}
	
	long num = strtol(_cad, &ptr, 10);
	if (num < 0 || num > 1000000) {
		fprintf(stderr, "xtimeout: Invalid duration: %s\n", _cad);
		return -1;
	}
	if (ptr && *ptr) {
		switch (*ptr) {
		case 's': break;
		case 'm': num *= 60;         break;
		case 'h': num *= 60*60;      break;
		case 'd': num *= 60*60*24;   break;
		case 'w': num *= 60*60*24*7; break;
		default:  fprintf(stderr, "xtimeout: Invalid duration: %s\n", _cad); return -1;
		}
		*_ret += num;
		return duration_parse(ptr+1, _ret);
	} else {
		*_ret += num;
		return 0;
	}
}

static int
pipe_timeout(int pipe_rd, long seconds)
{
	fd_set         rfds;
	struct timeval tv;
	#ifndef __linux__
	#  warning POSIX does not guarantee select to update tv.
	#endif
	tv.tv_sec  = seconds;
	tv.tv_usec = 0;
	retry:
	FD_ZERO(&rfds);
	FD_SET(pipe_rd, &rfds);
	int e = select(pipe_rd + 1, &rfds, NULL, NULL, &tv);
	if (e == -1 && errno == EINTR) {
		goto retry;
	}
	return e;
}




