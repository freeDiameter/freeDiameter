#!/bin/sh
set -eu
umask 077

# Test credentials only. Never reuse this CA or its keys in production.
# Refuse to overwrite an existing directory, including a partial earlier run.
mkdir single-host-certs
cd single-host-certs

cat > ca.cnf <<'EOF'
[req]
distinguished_name = dn
x509_extensions = ca
[dn]
[ca]
basicConstraints = critical,CA:TRUE
keyUsage = critical,keyCertSign,cRLSign
EOF

openssl req -new -x509 -nodes -newkey rsa:2048 -sha256 -days 2 \
	-config ca.cnf -subj /CN=ca.localdomain \
	-keyout cakey.pem -out cacert.pem

for n in 1 2; do
	cat > "peer$n.cnf" <<EOF
[peer]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth,clientAuth
subjectAltName = DNS:peer$n.localdomain
EOF
	openssl req -new -nodes -newkey rsa:2048 -sha256 \
		-subj "/CN=peer$n.localdomain" \
		-keyout "peer$n.key.pem" -out "peer$n.csr.pem"
	openssl x509 -req -sha256 -days 2 -in "peer$n.csr.pem" \
		-CA cacert.pem -CAkey cakey.pem -set_serial "$n" \
		-extfile "peer$n.cnf" -extensions peer -out "peer$n.cert.pem"
done

echo "Test-only credentials created in single-host-certs (valid for two days)."
