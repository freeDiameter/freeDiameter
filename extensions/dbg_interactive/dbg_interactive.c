/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2013, WIDE Project and NICT								 *
* All rights reserved.											 *
* 													 *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						 *
* 													 *
* * Redistributions of source code must retain the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer.										 *
*    													 *
* * Redistributions in binary form must reproduce the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer in the documentation and/or other						 *
*   materials provided with the distribution.								 *
* 													 *
* * Neither the name of the WIDE Project or NICT nor the 						 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of WIDE Project and 						 *
*   NICT.												 *
* 													 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	 *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	 *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								 *
*********************************************************************************************************/

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#undef HAVE_CLOCK_GETTIME
#include <freeDiameter/extension.h>
#include <poll.h>
#include <unistd.h>

extern PyObject *PyInit__fDpy(void);
extern unsigned char fDpy_py[];
extern unsigned int fDpy_py_len;

static pthread_t pyinterp;
static int thread_started;
static int stop_pipe[2] = { -1, -1 };
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t init_cond = PTHREAD_COND_INITIALIZER;
static int init_result = -1;
static int waiting_for_start;

/* Wait without holding the GIL so callbacks can run while the console is idle. */
static int wait_input(int interactive)
{
	struct pollfd fds[2];
	int ret, saved_errno;

	fds[0].fd = stop_pipe[0];
	fds[0].events = POLLIN;
	fds[1].fd = STDIN_FILENO;
	fds[1].events = POLLIN;
	do {
		Py_BEGIN_ALLOW_THREADS
		ret = poll(fds, interactive ? 2 : 1, -1);
		saved_errno = errno;
		Py_END_ALLOW_THREADS
	} while (ret < 0 && saved_errno == EINTR);
	if (ret < 0) {
		LOG_E("[dbg_interactive] poll failed: %s", strerror(saved_errno));
		return -1;
	}
	return fds[0].revents ? 0 : 1;
}

static void run_console(void)
{
	PyObject *console, *line, *result;
	char *buf = NULL;
	size_t len = 0, capacity = 0;
	int more = 0, ready;

	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
		LOG_E("[dbg_interactive] an interactive console requires a terminal");
		return;
	}
	console = PyRun_String("__import__('code').InteractiveConsole(globals())",
			       Py_eval_input, PyModule_GetDict(PyImport_AddModule("__main__")),
			       PyModule_GetDict(PyImport_AddModule("__main__")));
	if (!console) {
		PyErr_Print();
		return;
	}
	printf("Starting interactive python interpreter [experimental].\n"
	       "Please use Ctrl-D to exit.\n");
	fflush(stdout);
	while (1) {
		char ch;
		ssize_t n;
		if (!len) {
			printf("%s", more ? "... " : ">>> ");
			fflush(stdout);
		}
		ready = wait_input(1);
		if (ready <= 0)
			break;
		n = read(STDIN_FILENO, &ch, 1);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0) {
			if (n < 0)
				LOG_E("[dbg_interactive] console read failed: %s", strerror(errno));
			break;
		}
		if (ch != '\n') {
			if (len == capacity) {
				char *newbuf;
				size_t newcapacity = capacity ? capacity * 2 : 128;
				newbuf = realloc(buf, newcapacity);
				if (!newbuf) {
					LOG_E("[dbg_interactive] cannot allocate console input");
					break;
				}
				buf = newbuf;
				capacity = newcapacity;
			}
			buf[len++] = ch;
			continue;
		}
		line = PyUnicode_DecodeUTF8(buf ? buf : "", len, "strict");
		len = 0;
		if (!line) {
			PyErr_Print();
			continue;
		}
		result = PyObject_CallMethod(console, "push", "O", line);
		Py_DECREF(line);
		if (!result) {
			if (PyErr_ExceptionMatches(PyExc_SystemExit))
				PyErr_Clear();
			else
				PyErr_Print();
			break;
		}
		more = PyObject_IsTrue(result);
		Py_DECREF(result);
		if (more < 0) {
			PyErr_Print();
			break;
		}
	}
	free(buf);
	Py_DECREF(console);
}

static void cancel_start_wait(void *arg)
{
	PyEval_RestoreThread(arg);
	if (Py_FinalizeEx() < 0)
		LOG_E("[dbg_interactive] Python finalization failed");
}

static int set_script_path(PyObject *filename)
{
	PyObject *path = PySys_GetObject("path");
	PyObject *ospath = PyImport_ImportModule("os.path");
	PyObject *absolute = NULL, *directory = NULL;
	int ret = -1;

	if (!path || !PyList_Check(path)) {
		PyErr_SetString(PyExc_RuntimeError, "sys.path is not a list");
		goto out;
	}
	if (!ospath)
		goto out;
	absolute = PyObject_CallMethod(ospath, "abspath", "O", filename);
	if (!absolute)
		goto out;
	directory = PyObject_CallMethod(ospath, "dirname", "O", absolute);
	if (directory)
		ret = PyList_Insert(path, 0, directory);
out:
	Py_XDECREF(directory);
	Py_XDECREF(absolute);
	Py_XDECREF(ospath);
	return ret;
}

static void *myinterp(void *arg)
{
	char *shadow;
	int ret = 0;
	PyObject *module;
	PyThreadState *state;

	fd_log_threadname("fDpy");
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	Py_InitializeEx(0);
	module = PyInit__fDpy();
	if (!module || PyDict_SetItemString(PyImport_GetModuleDict(), "_fDpy", module) != 0) {
		PyErr_Print();
		LOG_E("[dbg_interactive] cannot initialize Python bindings");
		ret = EINVAL;
	}
	Py_XDECREF(module);
	shadow = ret ? NULL : malloc((size_t)fDpy_py_len + 1);
	if (!ret && !shadow) {
		LOG_E("[dbg_interactive] cannot allocate Python shadow definitions");
		ret = ENOMEM;
	} else if (shadow) {
		memcpy(shadow, fDpy_py, fDpy_py_len);
		shadow[fDpy_py_len] = '\0';
		if (PyRun_SimpleString("__file__ = ''") != 0 || PyRun_SimpleString(shadow) != 0) {
			LOG_E("[dbg_interactive] failed to initialize Python bindings");
			ret = EINVAL;
		}
		free(shadow);
	}
	pthread_mutex_lock(&init_mutex);
	init_result = ret;
	waiting_for_start = !ret;
	pthread_cond_signal(&init_cond);
	pthread_mutex_unlock(&init_mutex);
	if (ret)
		goto end;

	state = PyEval_SaveThread();
	pthread_cleanup_push(cancel_start_wait, state);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	ret = fd_core_waitstartcomplete();
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_mutex_lock(&init_mutex);
	waiting_for_start = 0;
	pthread_mutex_unlock(&init_mutex);
	pthread_cleanup_pop(0);
	PyEval_RestoreThread(state);
	if (ret) {
		LOG_E("[dbg_interactive] failed waiting for core startup: %s", strerror(ret));
		goto end;
	}
	/* A shutdown during startup also wakes fd_core_waitstartcomplete. */
	{
		struct pollfd fd = { stop_pipe[0], POLLIN, 0 };
		ret = poll(&fd, 1, 0);
		if (ret < 0) {
			LOG_E("[dbg_interactive] poll failed: %s", strerror(errno));
			goto end;
		}
		if (ret > 0)
			goto end;
	}
	if (arg) {
		FILE *script = fopen(arg, "r");
		if (!script) {
			LOG_E("[dbg_interactive] cannot open Python script '%s': %s", (char *)arg, strerror(errno));
		} else {
			PyObject *main = PyImport_AddModule("__main__");
			PyObject *globals = PyModule_GetDict(main);
			PyObject *filename = PyUnicode_DecodeFSDefault(arg);
			PyObject *argv = filename ? Py_BuildValue("[O]", filename) : NULL;
			PyObject *result = NULL;
			if (argv && PySys_SetObject("argv", argv) == 0 &&
			    PyDict_SetItemString(globals, "__file__", filename) == 0 &&
			    set_script_path(filename) == 0)
				result = PyRun_FileExFlags(script, arg, Py_file_input, globals, globals, 1, NULL);
			else
				fclose(script);
			Py_XDECREF(argv);
			Py_XDECREF(filename);
			if (!result) {
				if (PyErr_ExceptionMatches(PyExc_SystemExit))
					PyErr_Clear();
				else
					PyErr_Print();
				LOG_E("[dbg_interactive] Python script '%s' failed", (char *)arg);
			}
			Py_XDECREF(result);
		}
	} else {
		run_console();
		CHECK_FCT_DO(fd_core_shutdown(), LOG_E("[dbg_interactive] failed to request shutdown"));
	}
	(void)wait_input(0);
end:
	if (Py_FinalizeEx() < 0)
		LOG_E("[dbg_interactive] Python finalization failed");
	return NULL;
}

static int di_main(char *conffile)
{
	int ret;
	TRACE_ENTRY("%p", conffile);
	if (Py_IsInitialized()) {
		LOG_E("[dbg_interactive] Python is already initialized");
		return EALREADY;
	}
	CHECK_SYS(pipe(stop_pipe));
	ret = pthread_create(&pyinterp, NULL, myinterp, conffile);
	if (ret) {
		close(stop_pipe[0]);
		close(stop_pipe[1]);
		stop_pipe[0] = stop_pipe[1] = -1;
		return ret;
	}
	thread_started = 1;
	CHECK_POSIX(pthread_mutex_lock(&init_mutex));
	while (init_result == -1)
		CHECK_POSIX(pthread_cond_wait(&init_cond, &init_mutex));
	ret = init_result;
	CHECK_POSIX(pthread_mutex_unlock(&init_mutex));
	if (ret)
		return ret;
	return 0;
}

void fd_ext_fini(void)
{
	if (thread_started) {
		CHECK_POSIX_DO(pthread_mutex_lock(&init_mutex), );
		if (waiting_for_start)
			CHECK_POSIX_DO(pthread_cancel(pyinterp), LOG_E("[dbg_interactive] cannot cancel startup wait"));
		CHECK_POSIX_DO(pthread_mutex_unlock(&init_mutex), );
		if (write(stop_pipe[1], "x", 1) != 1)
			LOG_E("[dbg_interactive] cannot wake interpreter: %s", strerror(errno));
		CHECK_POSIX_DO(pthread_join(pyinterp, NULL), LOG_E("[dbg_interactive] cannot join interpreter"));
		thread_started = 0;
	}
	if (stop_pipe[0] != -1) {
		close(stop_pipe[0]);
		close(stop_pipe[1]);
		stop_pipe[0] = stop_pipe[1] = -1;
	}
}

/* Define the entry point function */
EXTENSION_ENTRY("dbg_interactive", di_main);
