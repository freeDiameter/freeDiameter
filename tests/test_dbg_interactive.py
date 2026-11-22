import os
import pty
import select
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


DAEMON, EXTENSION = sys.argv[1:3]
sys.argv[1:3] = []


class InteractiveTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(
            dir=Path(DAEMON).resolve().parent.parent / "tests"
        )
        self.directory = Path(self.tmp.name)
        self.process = None

    def tearDown(self):
        if self.process and self.process.poll() is None:
            self.process.kill()
            self.process.communicate(timeout=5)
        self.tmp.cleanup()

    def config(self, script=None, extra=""):
        config = self.directory / "freeDiameter.conf"
        argument = f' : "{script}"' if script is not None else ""
        config.write_text(
            'Identity = "debug.localdomain";\nRealm = "localdomain";\n'
            'Port = 0;\nSecPort = 0;\nNo_SCTP;\n'
            f'LoadExtension = "{EXTENSION}"{argument};\n{extra}'
        )
        return str(config)

    def start(self, config, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE):
        self.process = subprocess.Popen(
            [DAEMON, "-c", config], stdin=stdin, stdout=stdout,
            stderr=subprocess.STDOUT,
        )

    def wait_for_file(self, path):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if path.exists():
                return
            if self.process.poll() is not None:
                self.fail(f"daemon exited early: {self.process.communicate()[0]!r}")
            time.sleep(0.05)
        self.process.terminate()
        output, _ = self.process.communicate(timeout=10)
        self.fail(f"script did not complete: {output!r}")

    def stop(self):
        self.process.send_signal(signal.SIGTERM)
        output, _ = self.process.communicate(timeout=10)
        self.assertEqual(self.process.returncode, 0, output)
        return output

    def test_script_and_callback(self):
        ready = self.directory / "ready"
        script = self.directory / "script.py"
        (self.directory / "helper.py").write_text("value = 42\n")
        script.write_text(
            "import sys, threading, time, helper\n"
            "from pathlib import Path\n"
            "assert sys.argv == [__file__]\n"
            "assert helper.value == 42\n"
            "assert cvar.fd_g_config.cnf_diamid == 'debug.localdomain'\n"
            "assert dictionary() is not None\n"
            "value = avp_value()\n"
            "value.os = b'a\\x00\\xff'\n"
            "assert value.os.len == 3\n"
            "assert value.os.as_str() == b'a\\x00\\xff'\n"
            "assert fd_os_hash('hello') == fd_os_hash(b'hello')\n"
            "try:\n"
            "    fd_hook_hdl(0, None)\n"
            "    raise AssertionError('missing callback accepted')\n"
            "except ValueError:\n"
            "    pass\n"
            "queue = fifo()\n"
            "def produce():\n"
            "    time.sleep(0.05)\n"
            "    try:\n"
            "        fd_hook_hdl(0, None)\n"
            "    except ValueError:\n"
            "        queue.post('queued')\n"
            "producer = threading.Thread(target=produce)\n"
            "producer.start()\n"
            "assert queue.get() == 'queued'\n"
            "producer.join()\n"
            "cleaned = []\n"
            "session_cb = session_handler(lambda state, sid: cleaned.append(state))\n"
            "sess = session()\n"
            "sess.store(session_cb, {'value': 42})\n"
            "del session_cb\n"
            "assert cleaned == [{'value': 42}]\n"
            "del sess\n"
            "try:\n"
            "    fd_hook_data_hdl()\n"
            "    raise AssertionError('unsafe per-message data accepted')\n"
            "except NotImplementedError:\n"
            "    pass\n"
            "seen = threading.Event()\n"
            "def callback(kind, message, peer, other, previous):\n"
            "    assert previous is None\n"
            "    seen.set()\n"
            "h = fd_hook_hdl(1 << HOOK_MESSAGE_DROPPED, callback)\n"
            "model = cvar.fd_g_config.cnf_dict.search(DICT_COMMAND, CMD_BY_NAME, 'Device-Watchdog-Request')\n"
            "wire = msg(model).bufferize()\n"
            "assert isinstance(wire, bytes)\n"
            "assert msg(wire).bufferize() == wire\n"
            "msg(model).send()\n"
            "assert seen.wait(5), 'hook was not called'\n"
            f"Path({str(ready)!r}).touch()\n"
        )
        self.start(self.config(script))
        self.wait_for_file(ready)
        output = self.stop()
        self.assertNotIn(b"Python script", output)

    def test_bounded_fifo_post_allows_consumer_to_run(self):
        ready = self.directory / "fifo-ready"
        script = self.directory / "fifo.py"
        script.write_text(
            "import threading, time\n"
            "from pathlib import Path\n"
            "queue = fifo(1)\n"
            "queue.post('first')\n"
            "started = threading.Event()\n"
            "def produce():\n"
            "    started.set()\n"
            "    queue.post('second')\n"
            "producer = threading.Thread(target=produce)\n"
            "producer.start()\n"
            "assert started.wait(5)\n"
            "time.sleep(0.1)\n"
            "assert queue.get() == 'first'\n"
            "producer.join(5)\n"
            "assert not producer.is_alive()\n"
            "assert queue.get() == 'second'\n"
            "queue = fifo(1)\n"
            "kind = 'struct fd_event *'\n"
            "queue.post(fd_event(FDEV_TRIGGER, 0), kind)\n"
            "started.clear()\n"
            "def produce_event():\n"
            "    started.set()\n"
            "    queue.post(fd_event(FDEV_TRIGGER, 0), kind)\n"
            "producer = threading.Thread(target=produce_event)\n"
            "producer.start()\n"
            "assert started.wait(5)\n"
            "time.sleep(0.1)\n"
            "cvar.fd_g_config.cnf_main_ev.post(queue.get(kind), kind)\n"
            "producer.join(5)\n"
            "assert not producer.is_alive()\n"
            "cvar.fd_g_config.cnf_main_ev.post(queue.get(kind), kind)\n"
            f"Path({str(ready)!r}).touch()\n"
        )
        self.start(self.config(script))
        self.wait_for_file(ready)
        output = self.stop()
        self.assertNotIn(b"Python script", output)

    def test_sample(self):
        sample = Path(__file__).resolve().parents[1] / "doc" / "dbg_interactive.py.sample"
        ready = self.directory / "sample-ready"
        script = self.directory / "sample-runner.py"
        log = self.directory / "sample.log"
        script.write_text(
            "from pathlib import Path\n"
            f"sample = Path({str(sample)!r})\n"
            "exec(compile(sample.read_bytes(), str(sample), 'exec'), globals())\n"
            "assert isinstance(buf, bytes) and err is None\n"
            "assert ep.thisown, 'adding an endpoint must not transfer its ownership'\n"
            "request = msg(d_req)\n"
            "origin = avp(d_oh, AVPFL_SET_BLANK_VALUE)\n"
            "origin.header().avp_value.os = 'debug.localdomain'\n"
            "request.add_child(origin)\n"
            "test_value = avp(d_avp, AVPFL_SET_BLANK_VALUE)\n"
            "test_value.header().avp_value.u32 = 123\n"
            "request.add_child(test_value)\n"
            "status, answer, action = test_app_cb(request, None, None)\n"
            "assert status == 0 and action == DISP_ACT_SEND\n"
            "assert answer.search(d_avp).header().avp_value.u32 == 123\n"
            "assert answer.search(d_rc).header().avp_value.u32 == 2001\n"
            "receive_answer(answer, 123)\n"
            "started, acquired = threading.Event(), threading.Event()\n"
            "def take_read_lock():\n"
            "    started.set()\n"
            "    r.rdlock()\n"
            "    acquired.set()\n"
            "    r.unlock()\n"
            "r.wrlock()\n"
            "reader = threading.Thread(target=take_read_lock)\n"
            "reader.start()\n"
            "assert started.wait(5), 'reader did not start'\n"
            "import time\n"
            "time.sleep(0.05)\n"
            "r.unlock()\n"
            "assert acquired.wait(5), 'reader could not acquire the lock'\n"
            "reader.join()\n"
            f"Path({str(ready)!r}).touch()\n"
        )
        with log.open("w+b") as output:
            self.start(self.config(script), stdout=output)
            try:
                self.wait_for_file(ready)
            except AssertionError as error:
                output.flush()
                self.fail(f"{error}\n{log.read_text(errors='replace')}")
            self.stop()
        text = log.read_text(errors="replace")
        self.assertIn("Py ECHO", text)
        self.assertIn("Py RECV 7b (expected: 7b) Status: 2001", text)
        self.assertNotIn("Traceback", text)
        self.assertNotIn("Exception ignored", text)
        self.assertNotIn(" ERROR ", text)

    def test_script_exception_is_reported_without_exiting_daemon(self):
        script = self.directory / "exit.py"
        script.write_text("raise SystemExit(7)\n")
        self.start(self.config(script))
        time.sleep(0.5)
        self.assertIsNone(self.process.poll())
        output = self.stop()
        self.assertIn(b"Python script", output)

    def test_startup_failure_does_not_hang(self):
        script = self.directory / "never.py"
        script.write_text("raise AssertionError('must not run')\n")
        self.start(self.config(script, 'LoadExtension = "/missing/extension.fdx";\n'))
        output, _ = self.process.communicate(timeout=10)
        self.assertNotEqual(self.process.returncode, 0, output)
        self.assertNotIn(b"must not run", output)

    def test_console_and_external_shutdown(self):
        master, slave = pty.openpty()
        try:
            self.start(self.config(), stdin=slave, stdout=slave)
            os.close(slave)
            slave = -1
            output = b""
            deadline = time.monotonic() + 10
            while b">>> " not in output and time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    output += os.read(master, 8192)
            self.assertIn(b">>> ", output)
            os.write(master, b"print('DBG_SHELL', cvar.fd_g_config.cnf_diamid)\n")
            while b"DBG_SHELL debug.localdomain" not in output and time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    output += os.read(master, 8192)
            self.assertIn(b"DBG_SHELL debug.localdomain", output)
            self.process.send_signal(signal.SIGTERM)
            self.assertEqual(self.process.wait(timeout=10), 0)
        finally:
            os.close(master)
            if slave != -1:
                os.close(slave)

    def test_console_multiline_and_eof(self):
        master, slave = pty.openpty()
        try:
            self.start(self.config(), stdin=slave, stdout=slave)
            os.close(slave)
            slave = -1
            output = b""
            deadline = time.monotonic() + 10
            while b">>> " not in output and time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    output += os.read(master, 8192)
            self.assertIn(b">>> ", output)
            os.write(master, b"for i in range(1):\n    print('MULTILINE_OK')\n\n")
            while b"MULTILINE_OK\r\n>>>" not in output and time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    output += os.read(master, 8192)
            self.assertIn(b"MULTILINE_OK\r\n>>>", output)
            os.write(master, b"\x04")
            self.assertEqual(self.process.wait(timeout=10), 0)
        finally:
            os.close(master)
            if slave != -1:
                os.close(slave)

    def test_non_terminal_console_requests_shutdown(self):
        self.start(self.config())
        output, _ = self.process.communicate(timeout=10)
        self.assertEqual(self.process.returncode, 0, output)
        self.assertIn(b"requires a terminal", output)


if __name__ == "__main__":
    unittest.main()
