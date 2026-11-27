#!/usr/bin/env python3
"""Check the single-host fixture's test certificate generator."""

import hashlib
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "doc/single_host/make_certs.sh"
OPENSSL = shutil.which("openssl")


@unittest.skipUnless(OPENSSL, "openssl is required for the single-host certificate fixture")
class CertificateTests(unittest.TestCase):
    def test_certificates_and_overwrite_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                ["sh", str(SCRIPT)], cwd=directory, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, universal_newlines=True, timeout=60,
            )
            self.assertEqual(0, result.returncode, result.stdout)
            cert_dir = Path(directory) / "single-host-certs"
            for peer in (1, 2):
                cert = cert_dir / "peer{}.cert.pem".format(peer)
                verify = subprocess.run(
                    [OPENSSL, "verify", "-CAfile", str(cert_dir / "cacert.pem"),
                     "-verify_hostname", "peer{}.localdomain".format(peer), str(cert)],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    universal_newlines=True, timeout=10,
                )
                self.assertEqual(0, verify.returncode, verify.stdout)
            cert = cert_dir / "peer1.cert.pem"
            original = hashlib.sha256(cert.read_bytes()).digest()
            retry = subprocess.run(
                ["sh", str(SCRIPT)], cwd=directory, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, universal_newlines=True, timeout=10,
            )
            self.assertNotEqual(0, retry.returncode, retry.stdout)
            self.assertEqual(original, hashlib.sha256(cert.read_bytes()).digest())


if __name__ == "__main__":
    unittest.main()
