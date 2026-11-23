import json
import sys

from multi_peer_harness import run_scenario


daemon, dump_extension, expected_avps, *extensions = sys.argv[1:]


def scenario(peers):
    output = peers.directory / "dictionary.json"
    peers.start(0, [(extension, None) for extension in extensions] +
                [(dump_extension, output)])
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "daemon startup")
    data = json.loads(output.read_text())
    for expected in expected_avps.split(","):
        name, code, vendor = expected.split(":")
        if not any(avp["Name"] == name and avp["Code"] == int(code) and
                   avp.get("Vendor", 0) == int(vendor) for avp in data["AVPs"]):
            raise AssertionError("dictionary is missing AVP %s (%s, vendor %s)" %
                                 (name, code, vendor))


if __name__ == "__main__":
    run_scenario(daemon, scenario, 1, [])
