import re
import signal

def configure(peers, index, destination, mode="both"):
    config = peers.directory / ("test_app%d.conf" % (index + 1))
    config.write_text('mode = %s;\ndest-host = "%s";\n' %
                      (mode, peers.identities[destination]))
    return config


def exchange(peers, client, server, check_echo):
    destination = re.escape(peers.identities[server])
    client_offset = len(peers.text(client))
    server_offset = len(peers.text(server)) if check_echo else 0
    peers.processes[client].send_signal(signal.SIGUSR1)
    answer = peers.wait(
        lambda: re.search(
            r"RECV ([0-9a-f]+) \(Ok\) Status: 2001 From "
            r"'%s' \('localdomain'\)" % destination,
            peers.text(client)[client_offset:]),
        "test_app answer at " + peers.identities[client],
    )
    sent = re.search(
        r"SEND ([0-9a-f]+) to 'localdomain' \(%s\)" % destination,
        peers.text(client)[client_offset:],
    )
    if not sent or sent.group(1) != answer.group(1):
        raise AssertionError("test_app answer did not echo the sent value")
    if check_echo:
        peers.wait(
            lambda: "ECHO Test-Request received from '%s'" % peers.identities[client]
                    in peers.text(server)[server_offset:],
            "test_app server callback at " + peers.identities[server],
        )
