import contextlib
import io
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from multi_peer_harness import Peers, create_tls_credentials
from multi_peer_test_app import exchange


class HarnessTest(unittest.TestCase):
    def test_multiple_extensions_and_peer_configuration(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.subprocess.Popen") as popen:
            process = popen.return_value
            process.poll.return_value = None
            process.returncode = 0
            with Peers("/daemon", Path(temporary), 2, [(0, 1)]) as peers:
                peers.start(0, [("/first.fdx", "/first.conf"), ("/second.fdx", None)],
                            extra_config='NoRelay;', env={"SCENARIO": "enabled"})
                config = (Path(temporary) / "peer1.conf").read_text()
                self.assertIn('LoadExtension = "/first.fdx" : "/first.conf";', config)
                self.assertIn('LoadExtension = "/second.fdx";', config)
                self.assertIn("NoRelay;", config)
                self.assertEqual(popen.call_args[1]["env"]["SCENARIO"], "enabled")
            process.send_signal.assert_called_once()
            process.wait.assert_called_once()

    def test_error_file_interrupts_wait(self):
        with tempfile.TemporaryDirectory() as temporary, \
                contextlib.redirect_stderr(io.StringIO()):
            error = Path(temporary) / "scenario-error"
            error.write_text("callback failed")
            with self.assertRaisesRegex(RuntimeError, "callback failed"):
                with Peers("/daemon", Path(temporary), 2, [(0, 1)]) as peers:
                    peers.watch_error_file(error)
                    peers.wait(lambda: False, "scenario completion")

    def test_passive_peer_does_not_configure_neighbors(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.subprocess.Popen") as popen:
            popen.return_value.poll.return_value = None
            popen.return_value.returncode = 0
            directory = Path(temporary)
            with Peers("/daemon", directory, 2, [(0, 1)]) as peers:
                peers.start(0, [], connect_neighbors=False)
                peers.start(1, [])
                self.assertNotIn("ConnectPeer =", (directory / "peer1.conf").read_text())
                self.assertIn('ConnectPeer = "peer1.localdomain"',
                              (directory / "peer2.conf").read_text())

    def test_tls_sctp_configuration(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.free_ports", return_value=[30001, 30002, 30003]), \
                mock.patch("multi_peer_harness.create_tls_credentials") as credentials, \
                mock.patch("multi_peer_harness.subprocess.Popen") as popen:
            directory = Path(temporary)
            credentials.return_value = (directory / "ca.pem", [
                (directory / "peer1.pem", directory / "peer1.key"),
                (directory / "peer2.pem", directory / "peer2.key"),
                (directory / "peer3.pem", directory / "peer3.key"),
            ])
            popen.return_value.poll.return_value = None
            popen.return_value.returncode = 0
            with Peers("/daemon", directory, 3, [(0, 1), (1, 2)],
                       tls=True, sctp=True, openssl="/openssl") as peers:
                peers.start(1, [])
                config = (directory / "peer2.conf").read_text()
                self.assertIn("Port = 0;\nSecPort = 30002;", config)
                self.assertIn("No_TCP;", config)
                self.assertNotIn("No_SCTP;", config)
                self.assertNotIn("No_TLS;", config)
                self.assertIn('TLS_Cred = "%s", "%s";' %
                              (directory / "peer2.pem", directory / "peer2.key"), config)
                self.assertIn('TLS_CA = "%s";' % (directory / "ca.pem"), config)
                self.assertIn('ConnectPeer = "peer1.localdomain"', config)
                self.assertIn('ConnectPeer = "peer3.localdomain"', config)
                self.assertIn("Port = 30001;", config)
                self.assertIn("Port = 30003;", config)
                self.assertEqual(config.count("ConnectPeer ="), 2)
                credentials.assert_called_once_with(directory, "/openssl", peers.identities)

    def test_invalid_topology_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            for count, links in ((0, []), (3, [(0, 0)]), (3, [(0, 3)]),
                                 (3, [(0, 1), (1, 0)])):
                with self.subTest(count=count, links=links):
                    with self.assertRaises(ValueError):
                        Peers("/daemon", Path(temporary), count, links)

    def test_star_and_isolated_peer_configuration(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.subprocess.Popen") as popen:
            popen.return_value.poll.return_value = None
            popen.return_value.returncode = 0
            directory = Path(temporary)
            with Peers("/daemon", directory, 5, [(0, 1), (0, 2), (0, 3)]) as peers:
                peers.start(0, [])
                peers.start(4, [])
                hub = (directory / "peer1.conf").read_text()
                for identity in peers.identities[1:4]:
                    self.assertIn('ConnectPeer = "%s"' % identity, hub)
                self.assertEqual(hub.count("ConnectPeer ="), 3)
                self.assertNotIn("ConnectPeer =", (directory / "peer5.conf").read_text())

    def test_certificate_generation_failure_is_reported(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.subprocess.run") as run:
            run.return_value.returncode = 1
            run.return_value.stderr = b"certificate error"
            with self.assertRaisesRegex(RuntimeError, "certificate error"):
                create_tls_credentials(Path(temporary), "/openssl", ["peer1.localdomain"])

    def test_transport_check_rejects_fallback(self):
        with tempfile.TemporaryDirectory() as temporary:
            peers = Peers("/daemon", Path(temporary), 3, [(0, 1), (1, 2)])
            peers.tls, peers.sctp = True, True
            logs = [
                "Connected to 'peer2.localdomain' (SCTP,TLS,soc#1)",
                "Connected to 'peer1.localdomain' (SCTP,TLS,soc#2)\n"
                "Connected to 'peer3.localdomain' (SCTP,TLS,soc#3)",
                "Connected to 'peer2.localdomain' (SCTP,TLS,soc#4)",
            ]
            peers.text = lambda index: logs[index]

            def wait(condition, description):
                if not condition():
                    raise RuntimeError(description)

            peers.wait = wait
            peers.wait_transport()
            logs[1] = logs[1].replace("Connected to", "CONNECTED TO")
            peers.wait_transport()
            logs[0] = logs[0].replace("SCTP", "TCP")
            with self.assertRaisesRegex(RuntimeError, "SCTP,TLS"):
                peers.wait_transport()
            logs[0] = logs[0].replace("TCP,TLS", "SCTP")
            with self.assertRaisesRegex(RuntimeError, "SCTP,TLS"):
                peers.wait_transport()
            logs[0] = logs[0].replace("SCTP", "SCTP,TLS")
            logs[2] = logs[2].replace("SCTP", "TCP")
            with self.assertRaisesRegex(RuntimeError, "SCTP,TLS"):
                peers.wait_transport()

    def test_all_open_checks_each_side_of_each_link(self):
        with tempfile.TemporaryDirectory() as temporary:
            peers = Peers("/daemon", Path(temporary), 3, [(0, 1), (1, 2)])
            logs = [
                "'STATE_OPEN' 'peer2.localdomain'",
                "'STATE_OPEN' 'peer1.localdomain'\n'STATE_OPEN' 'peer3.localdomain'",
                "'STATE_OPEN' 'peer2.localdomain'",
            ]
            peers.text = lambda index: logs[index]

            def wait(condition, description):
                if not condition():
                    raise RuntimeError(description)

            peers.wait = wait
            peers.wait_all_open()
            logs[2] = "'STATE_OPEN_NEW' 'peer2.localdomain'"
            with self.assertRaisesRegex(RuntimeError, "STATE_OPEN"):
                peers.wait_all_open()

    def test_peers_stop_in_reverse_start_order(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.subprocess.Popen") as popen:
            order = []
            first, second = mock.Mock(), mock.Mock()
            for index, process in ((1, first), (0, second)):
                process.poll.return_value = None
                process.returncode = 0
                process.send_signal.side_effect = lambda signal, index=index: order.append(index)
            popen.side_effect = (first, second)
            with Peers("/daemon", Path(temporary), 2, [(0, 1)]) as peers:
                peers.start(1, [])
                peers.start(0, [])
            self.assertEqual(order, [0, 1])

    def test_post_stop_check_sees_final_log(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.subprocess.Popen") as popen:
            process = popen.return_value
            process.poll.return_value = None
            process.returncode = 0

            def finish(timeout):
                log = popen.call_args[1]["stdout"]
                log.write(b"final output")
                log.flush()

            process.wait.side_effect = finish
            with Peers("/daemon", Path(temporary), 2, [(0, 1)]) as peers:
                peers.start(0, [])
                peers.after_stop(lambda: self.assertIn("final output", peers.text(0)))

    def test_stop_callback_failure_still_stops_peer_and_reports(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch("multi_peer_harness.subprocess.Popen") as popen, \
                contextlib.redirect_stderr(io.StringIO()):
            process = popen.return_value
            process.poll.return_value = None
            process.returncode = 0
            with self.assertRaisesRegex(RuntimeError, "stop callback failed: broken"):
                with Peers("/daemon", Path(temporary), 2, [(0, 1)]) as peers:
                    peers.start(0, [])

                    def fail():
                        raise RuntimeError("broken")

                    peers.on_stop(fail)
            process.send_signal.assert_called_once()
            process.wait.assert_called_once()

    def test_timed_out_peer_captures_diagnostics_before_kill(self):
        for debugger_error in (None, OSError("gdb unavailable"),
                               subprocess.TimeoutExpired("gdb", 15)):
            with self.subTest(debugger_error=debugger_error), \
                    tempfile.TemporaryDirectory() as temporary, \
                    mock.patch.dict("os.environ", {"FD_MULTI_PEER_DIAGNOSTICS":
                                                  str(Path(temporary) / "diagnostics")}), \
                    mock.patch("multi_peer_harness.subprocess.Popen") as popen, \
                    mock.patch("multi_peer_harness.subprocess.run") as run, \
                    contextlib.redirect_stderr(io.StringIO()):
                directory = Path(temporary)
                process = popen.return_value
                process.pid = 1234
                process.poll.return_value = None
                process.returncode = -9
                process.wait.side_effect = [subprocess.TimeoutExpired("daemon", 25), 0]

                def capture(*args, **kwargs):
                    process.kill.assert_not_called()
                    kwargs["stdout"].write("debugger output")
                    if debugger_error:
                        raise debugger_error

                run.side_effect = capture
                peers = Peers("/daemon", directory, 1, [])
                peers.start(0, [])
                peers.logs[0].write(b"peer output")
                failures = peers.stop()
                peers.logs[0].close()

                self.assertIn("peer 1 did not stop gracefully", failures)
                self.assertEqual(any("diagnostic capture failed" in item for item in failures),
                                 debugger_error is not None)
                process.kill.assert_called_once()
                self.assertEqual(process.wait.call_args_list,
                                 [mock.call(timeout=25), mock.call(timeout=5)])
                run.assert_called_once()
                self.assertIn("attach 1234", run.call_args[0][0])
                diagnostic_dirs = list((directory / "diagnostics").iterdir())
                self.assertEqual(len(diagnostic_dirs), 1)
                self.assertEqual((diagnostic_dirs[0] / "peer1-backtrace.txt").read_text(),
                                 "debugger output")
                self.assertEqual((diagnostic_dirs[0] / "peer1.log").read_bytes(),
                                 b"peer output")

    def test_exchange_cannot_reuse_previous_output(self):
        peers = mock.Mock()
        peers.identities = ("peer1.localdomain", "peer2.localdomain")
        peers.text.side_effect = lambda index: (
            "SEND 123 to 'localdomain' (peer1.localdomain)\n"
            "RECV 123 (Ok) Status: 2001 From 'peer1.localdomain' ('localdomain')\n"
            if index == 1 else "ECHO Test-Request received from 'peer2.localdomain'\n"
        )
        peers.processes = {1: mock.Mock()}

        def wait(condition, description):
            if not condition():
                raise RuntimeError("timed out waiting for " + description)
            return condition()

        peers.wait.side_effect = wait
        with self.assertRaisesRegex(RuntimeError, "test_app answer"):
            exchange(peers, 1, 0, True)


if __name__ == "__main__":
    unittest.main()
