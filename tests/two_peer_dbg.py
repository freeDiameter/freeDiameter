import os
import threading
import time
import traceback
from pathlib import Path


directory = Path(os.environ["FD_TWO_PEER_DIR"])
sample = Path(__file__).resolve().parents[1] / "doc" / "dbg_interactive.py.sample"
exec(compile(sample.read_bytes(), str(sample), "exec"), globals())

original_receive_answer = receive_answer
received = threading.Event()


def checked_answer(answer, expected):
    try:
        if answer.search(d_avp).header().avp_value.u32 != expected:
            raise AssertionError("answer did not echo the requested value")
        if answer.search(d_rc).header().avp_value.u32 != 2001:
            raise AssertionError("answer did not report DIAMETER_SUCCESS")
        if answer.search(d_oh).header().avp_value.os.as_str() != b"peer2.localdomain":
            raise AssertionError("answer came from the wrong peer")
        if answer.search(d_or).header().avp_value.os.as_str() != b"localdomain":
            raise AssertionError("answer came from the wrong realm")
        original_receive_answer(answer, expected)
        (directory / "python-answer").touch()
    except Exception:
        (directory / "python-error").write_text(traceback.format_exc())
        raise
    finally:
        received.set()
    return None


receive_answer = checked_answer
(directory / "python-ready").touch()
deadline = time.monotonic() + 30
while not (directory / "python-send").exists() and not (directory / "python-stop").exists():
    if time.monotonic() >= deadline:
        raise RuntimeError("timed out waiting to send the interactive request")
    time.sleep(0.05)
if not (directory / "python-stop").exists():
    send_query()
    deadline = time.monotonic() + 10
    while not received.wait(0.05) and not (directory / "python-stop").exists():
        if time.monotonic() >= deadline:
            raise RuntimeError("interactive answer callback was not invoked")
