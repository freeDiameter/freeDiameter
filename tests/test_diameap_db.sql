-- Load after extensions/app_diameap/diameap.sql into a disposable database.
INSERT INTO users (username, password, eapmethod, vendor, active)
VALUES ('test''user', 'test-password', 4, 0, 'Y'),
       ('disabled', 'test-password', 4, 0, 'N');
