import sys
from pathlib import Path

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app, dbg_interactive = sys.argv[1:4]


def scenario(peers):
    directory = peers.directory
    script = Path(__file__).resolve().parent / "two_peer_dbg.py"

    def check_logs():
        for index in peers.logs:
            text = peers.text(index)
            if "Traceback (most recent call last):" in text or "Exception ignored in:" in text:
                raise AssertionError("Python exception in peer %d" % (index + 1))
        if "Py ECHO" not in peers.text(0):
            raise AssertionError("interactive dispatch callback did not run")

    peers.watch_error_file(directory / "python-error")
    peers.on_stop((directory / "python-stop").touch)
    peers.start(0, [(dbg_interactive, script)], env={"FD_TWO_PEER_DIR": str(directory)})
    peers.after_stop(check_logs)
    peers.wait(lambda: (directory / "python-ready").exists(),
               "interactive application registration")
    peers.start(1, [(test_app, configure(peers, 1, 0))])
    peers.wait_open(1, 0)
    exchange(peers, 1, 0, False)
    peers.wait_all_open()
    peers.wait_transport()
    (directory / "python-send").touch()
    peers.wait(lambda: (directory / "python-answer").exists(),
               "interactive answer callback")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 2, [(0, 1)])
