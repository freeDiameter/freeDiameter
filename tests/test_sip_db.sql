-- Load after doc/app_sip.sql in a dedicated disposable database.
-- This table is used by app_sip but is absent from the sample schema.

CREATE TABLE ds_sip_aor_map (
  sip_aor VARCHAR(255) NOT NULL,
  sipserver_uri VARCHAR(255) NOT NULL,
  PRIMARY KEY (sip_aor)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO ds_users
  (id_user, username, password, sip_server_uri, temp_sip_server_uri,
   authentication_pending, registrated)
VALUES
  (1, 'alice.smith', 'correct-horse-battery-staple',
   'sip:scscf1.example.test:5060', '', 0, 0),
  (2, 'bob', 'bob-test-password',
   'sip:scscf2.example.test:5060', '', 0, 1);

INSERT INTO ds_sip_aor (id_sip_aor, id_user, sip_aor, sip_server_uri, registered)
VALUES
  (1, 1, 'sip:alice.smith@example.test', 'sip:scscf1.example.test:5060', 0),
  (2, 2, 'sip:bob@example.test', 'sip:scscf2.example.test:5060', 1);

INSERT INTO ds_sip_aor_map (sip_aor, sipserver_uri)
VALUES ('sip:alice.smith@example.test', 'aaa://hss.example.test');

INSERT INTO ds_networks (id_network, label_network)
VALUES (1, 'access.example.test'), (2, 'roaming.example.test');

INSERT INTO ds_user_networks (id_user_network, id_user, id_network)
VALUES (1, 1, 1), (2, 2, 2);

INSERT INTO ds_user_services (id_user_service, id_user, id_service, compulsory)
VALUES (1, 1, 11, 1);
