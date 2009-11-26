#!/usr/bin/make -s
#
# This file is inspired from freeDiameter's contrib/ca_script and
# improved to handle multiple CA in a hierarchical fashion.

SCRIPT_DIR = .
CONFIG = -config $(SCRIPT_DIR)/openssl.cnf
REMAKE = $(MAKE) -f $(SCRIPT_DIR)/Makefile
DATA_DIR = ./test

#Disable "make destroy"
force = 


# Default: print the help
all:	help

# Help message
help:
	@echo "\n\
Available commands:\n\
   make init topca=name\n\
       Creates the initial top-level CA structure\n\
   make new_ca name=caname\n\
       Creates a new sub-CA that can be used for certificates later.\n\
   make newcsr name=foo ca=bar\n\
       Create private key and csr in clients subdir (named foo.*)\n\
   make cert name=foo ca=bar\n\
       Signs the CSR foo.csr and creates the certificate foo.cert (signed by bar).\n\
   make revoke name=foo ca=bar\n\
       Revokes the certificate foo.cert issued by bar and regenerates the CRL.\n\
   make gencrl ca=bar\n\
       Regenerates the CRL for CA bar. Should be run at least once a month.\n\
\n\
";
	
# Destroy the CA hierarchy completly. Use with care.
destroy:
	@if [ -z "$(force)" ]; then echo "Destroy disabled, use: make destroy force=y"; exit 1; fi
	@if [ ! -d $(SCRIPT_DIR) ]; then echo "Error in setup"; exit 1; fi
	@echo "Removing everything (for debug purpose)..."
	@rm -rf $(DATA_DIR)/*

# Initialize the CA structure
structure:
	@if [ -z "$(caname)" ]; then echo "Internal error: caname is missing"; exit 1; fi
	@if [ -d $(DATA_DIR)/$(caname) ]; then echo "CA $(caname) already exists."; exit 1; fi
	@echo "Creating CA structure..."
	@mkdir $(DATA_DIR)/$(caname)/crl
	@mkdir $(DATA_DIR)/$(caname)/certs
	@mkdir $(DATA_DIR)/$(caname)/newcerts
	@mkdir $(DATA_DIR)/$(caname)/public-www
	@mkdir $(DATA_DIR)/$(caname)/private
	@chmod 700 $(DATA_DIR)/$(caname)/private
	@mkdir $(DATA_DIR)/$(caname)/clients
	@mkdir $(DATA_DIR)/$(caname)/clients/privkeys
	@mkdir $(DATA_DIR)/$(caname)/clients/csr
	@mkdir $(DATA_DIR)/$(caname)/clients/certs
	@echo "01" > $(DATA_DIR)/$(caname)/serial
	@touch $(DATA_DIR)/$(caname)/index.txt
	
# Initialize the top-level CA structure and keys.
init:
	@if [ -z "$(topca)" ]; then echo "Please specify the name of the CA in as topca=name.testbed.aaa"; exit 1; fi
	# Create the folder hierarchy
	@$(REMAKE) structure caname=$(topca)
	# Generate the self-signed certificate
	@CA_ROOT_DIR=$(DATA_DIR)/$(topca) openssl req $(CONFIG) -new -batch -x509 -nodes -newkey rsa:2048 -out $(DATA_DIR)/$(topca)/public-www/cacert.pem \
		-keyout $(DATA_DIR)/$(topca)/private/cakey.pem -subj /CN=$(topca)
	# Add the certificate hash
	@ln -s $(DATA_DIR)/$(topca)/public-www/cacert.pem $(DATA_DIR)/$(topca)/certs/`openssl x509 -noout -hash < $(DATA_DIR)/$(topca)/public-www/cacert.pem`.0
	@$(REMAKE) gencrl ca=$(topca)

# Create a secondary CA
newca:



############
# En dessous ce n est pas fini...



# Regenerate the Certificate Revocation List.
# This list should be available publicly
gencrl:
	@openssl ca $(CONFIG) -gencrl -out $(DIR)/public-www/crl.pem
	@ln -sf $(DIR)/public-www/crl.pem $(DIR)/crl/`openssl crl -noout -hash < $(DIR)/public-www/crl.pem`.r0

# Create a new private key and a CSR, in case the client does not provide the CSR by another mean.
# Usage is: make newcsr name=peer.client.fqdn email=admin@client.fqdn
newcsr:
	@if [ -z "$(name)" -o -z "$(email)" ]; then echo "Please provide certificate name and email address: make newcsr name=mn.nautilus.org email=you@mail.com"; exit 1; fi
	@if [ -e $(DIR)/clients/csr/$(name).csr ]; then echo "There is already a pending csr for this name."; exit 1; fi
	@if [ ! -e $(DIR)/clients/privkeys/$(name).key.pem ]; \
		then echo "Generating a private key for $(name) ..."; \
		openssl genrsa -out $(DIR)/clients/privkeys/$(name).key.pem 1024; \
		fi;
	@echo "Creating the CSR in $(DIR)/clients/csr/$(name).csr";
	@openssl req $(CONFIG) -new -batch -out $(DIR)/clients/csr/$(name).csr \
		-key $(DIR)/clients/privkeys/$(name).key.pem \
		-subj /C=$(C)/ST=$(ST)/L=$(L)/O=$(O)/OU=$(OU)/CN=$(name)/emailAddress=$(email)

# Process a CSR to create a x509 certificate. The certificate is valid for 1 year. 
# It should be sent to the client by any mean.
cert:
	@if [ -z "$(name)" ]; then echo "name must be provided: make cert name=mn.n6.org"; exit 1; fi
	@if [ ! -e $(DIR)/clients/csr/$(name).csr ]; then echo "Could not find CSR in $(DIR)/clients/csr/$(name).csr."; exit 1; fi
	@if [ -e $(DIR)/clients/certs/$(name).cert ]; \
		then echo "Revoking old certificate..."; \
		$(MAKE) revoke name=$(name); \
		fi;
	@openssl ca $(CONFIG) -in $(DIR)/clients/csr/$(name).csr \
		-out $(DIR)/clients/certs/$(name).cert \
		-days $(DAYS) \
		-batch
	@ln -s $(DIR)/clients/certs/$(name).cert $(DIR)/certs/`openssl x509 -noout -hash < $(DIR)/clients/certs/$(name).cert`.0

# Revoke a certificate.
revoke:
	@if [ -z "$(name)" ]; then echo "name must be provided: make revoke name=mn.n6.org"; exit 1; fi
	@if [ ! -e $(DIR)/clients/certs/$(name).cert ]; \
		then echo "$(DIR)/clients/certs/$(name).cert not found"; \
		exit 1; \
		fi;
	@openssl ca $(CONFIG) -revoke $(DIR)/clients/certs/$(name).cert;
	@rm -f $(DIR)/certs/`openssl x509 -noout -hash < $(DIR)/clients/certs/$(name).cert`.0
	@$(MAKE) gencrl
	
# End of file...
