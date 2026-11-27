#!/usr/bin/env python3
"""Check relative inline Markdown links in the source documentation."""

import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"(?<!!)\[[^\]]+\]\((<[^>]+>|[^)\s]+)(?:\s+\"[^\"]*\")?\)")


def check_file(path):
    errors = 0
    fence = None
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.lstrip()
        if stripped.startswith(("```", "~~~")):
            marker = stripped[:3]
            if fence is None:
                fence = marker
            elif fence == marker:
                fence = None
            continue
        if fence:
            continue
        for match in LINK.finditer(line):
            target = match.group(1).strip("<>")
            url = urlsplit(target)
            if url.scheme or url.netloc or not url.path:
                continue
            if not (path.parent / unquote(url.path)).exists():
                print("{}:{}: missing link {}".format(path, number, target))
                errors += 1
    return errors


def main():
    paths = sorted((ROOT / "doc").rglob("*.md"))
    paths.append(ROOT / ".github/instructions/fd.instructions.md")
    paths.extend([
        ROOT / "contrib/README",
        ROOT / "extensions/app_diameap/README",
        ROOT / "extensions/app_sip/README",
    ])
    errors = sum(check_file(path) for path in paths)
    if errors:
        return 1
    print("Checked relative Markdown links in {} files".format(len(paths)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
