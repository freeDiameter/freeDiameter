import json
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app, extension, loglevel, name = sys.argv[1:6]


def scenario(peers):
    log_config = peers.directory / "loglevel.conf"
    log_config.write_text("LogLevel=1;\n")
    config = None
    if name in ("dbg_dict_dump", "dbg_dict_dump_json"):
        config = peers.directory / "dictionary.out"
    elif name == "dbg_loglevel":
        config = log_config
    extensions = [(loglevel, log_config)] if name != "dbg_loglevel" else []
    extensions.append((extension, config))

    peers.start(2, [(test_app, configure(peers, 2, 0, mode="server"))],
                extra_config="NoRelay;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2), "server startup")
    peers.start(1, extensions)
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, configure(peers, 0, 2, mode="client"))],
                extra_config="NoRelay;")
    peers.wait_open(0, 1)

    if name == "dbg_dict_dump":
        text = config.read_text()
        if "Origin-Host" not in text or "Destination-Realm" not in text:
            raise AssertionError("dictionary dump is missing base AVPs")
    elif name == "dbg_dict_dump_json":
        data = json.loads(config.read_text())
        if "Origin-Host" not in json.dumps(data) or "Destination-Realm" not in json.dumps(data):
            raise AssertionError("JSON dictionary dump is missing base AVPs")
    elif name == "dbg_loglevel":
        log_config.write_text("LogLevel=2;\n")
        peers.processes[1].send_signal(signal.SIGUSR1)
        peers.wait(lambda: "dbg_loglevel: reloaded configuration, log level now 2"
                   in peers.text(1), "log level reload")
    elif name == "dbg_monitor":
        offset = len(peers.text(1))
        peers.processes[1].send_signal(signal.SIGUSR2)
        peers.wait(lambda: all(item in peers.text(1)[offset:] for item in (
            "[dbg_monitor] Dumping config information",
            "[dbg_monitor] Dumping extensions information",
            "[dbg_monitor] Dumping dictionary information",
            peers.identities[1], "Origin-Host")), "monitor signal dumps")

    offset = len(peers.text(1))
    exchange(peers, 0, 2, True)
    if name == "dbg_msg_dumps":
        peers.wait(lambda: all(item in peers.text(1)[offset:] for item in (
            "RCV from '%s':" % peers.identities[0],
            "SND to '%s':" % peers.identities[2])), "message dumps on relay")
    elif name == "dbg_msg_timings":
        peers.wait(lambda: "[TIMING] FWD " in peers.text(1)[offset:] and
                   "[TIMING] RCV ANS " in peers.text(1)[offset:], "relay timing records")
    elif name == "dbg_rt":
        peers.wait(lambda: all(item in peers.text(1)[offset:] for item in (
            "[dbg_rt] FWD routing message:", "[dbg_rt] OUT routing message:",
            peers.identities[2])), "routing debug records")
    peers.wait_all_open()
    peers.wait_transport()


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])
