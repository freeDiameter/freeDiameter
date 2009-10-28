/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2009, WIDE Project and NICT								 *
* All rights reserved.											 *
* 													 *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						 *
* 													 *
* * Redistributions of source code must retain the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer.										 *
*    													 *
* * Redistributions in binary form must reproduce the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer in the documentation and/or other						 *
*   materials provided with the distribution.								 *
* 													 *
* * Neither the name of the WIDE Project or NICT nor the 						 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of WIDE Project and 						 *
*   NICT.												 *
* 													 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	 *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	 *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								 *
*********************************************************************************************************/

#include "tests.h"

#ifndef TEST_PORT
#define TEST_PORT	3868
#endif /* TEST_PORT */

#ifndef GNUTLS_DEFAULT_PRIORITY
# define GNUTLS_DEFAULT_PRIORITY "NORMAL"
#endif /* GNUTLS_DEFAULT_PRIORITY */

#ifndef GNUTLS_DEFAULT_DHBITS
# define GNUTLS_DEFAULT_DHBITS 1024
#endif /* GNUTLS_DEFAULT_DHBITS */


/* The cryptographic data */
static char ca_data[] =		"-----BEGIN CERTIFICATE-----\n"
				"MIIEqjCCA5KgAwIBAgIJANKgDwdlDYQDMA0GCSqGSIb3DQEBBQUAMIGUMQswCQYD\n"
				"VQQGEwJKUDEOMAwGA1UECAwFVG9reW8xEDAOBgNVBAcMB0tvZ2FuZWkxDTALBgNV\n"
				"BAoMBFdJREUxDzANBgNVBAsMBkFBQSBXRzEfMB0GA1UEAwwWY2hhdnJvdXguY293\n"
				"YWRkaWN0Lm9yZzEiMCAGCSqGSIb3DQEJARYTc2RlY3VnaXNAbmljdC5nby5qcDAe\n"
				"Fw0wOTEwMDUwODUxNDRaFw0xOTEwMDMwODUxNDRaMIGUMQswCQYDVQQGEwJKUDEO\n"
				"MAwGA1UECAwFVG9reW8xEDAOBgNVBAcMB0tvZ2FuZWkxDTALBgNVBAoMBFdJREUx\n"
				"DzANBgNVBAsMBkFBQSBXRzEfMB0GA1UEAwwWY2hhdnJvdXguY293YWRkaWN0Lm9y\n"
				"ZzEiMCAGCSqGSIb3DQEJARYTc2RlY3VnaXNAbmljdC5nby5qcDCCASIwDQYJKoZI\n"
				"hvcNAQEBBQADggEPADCCAQoCggEBAM5c6w4NnngTvGNWcJzbo0Kklp+kvUNQNgGu\n"
				"myvz826qPp07HTSyJrIcgFnuYDR0Nd130Ot9u5osqpQhHTvolxDE87Tii8i3hJSj\n"
				"TTY9K0ZwGb4AZ6QkuyMXS1jtOY657HqjpGZqT/2Syh0i7dM/hqSXFw0SPbyq+W1H\n"
				"SVFWa1CTkPywFWAzwdr5WKah77uZ1dxWqgPgUdcZOiIQtLRp5n3fg40Nwso5YdwS\n"
				"64+ebBX1pkhrCQ8AGc8O61Ep1JTXcO7jqQmPgzjiN+FeostI1Dp73S3MqleTAHjR\n"
				"hqZ77VF7nkroMM9btMHJBaxnfwc2ewULUJwnuOiGWrvMq/9Z4J8CAwEAAaOB/DCB\n"
				"+TAdBgNVHQ4EFgQUkqpVn7N3gmiJ7X5zQ2bki+7qv4UwgckGA1UdIwSBwTCBvoAU\n"
				"kqpVn7N3gmiJ7X5zQ2bki+7qv4WhgZqkgZcwgZQxCzAJBgNVBAYTAkpQMQ4wDAYD\n"
				"VQQIDAVUb2t5bzEQMA4GA1UEBwwHS29nYW5laTENMAsGA1UECgwEV0lERTEPMA0G\n"
				"A1UECwwGQUFBIFdHMR8wHQYDVQQDDBZjaGF2cm91eC5jb3dhZGRpY3Qub3JnMSIw\n"
				"IAYJKoZIhvcNAQkBFhNzZGVjdWdpc0BuaWN0LmdvLmpwggkA0qAPB2UNhAMwDAYD\n"
				"VR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOCAQEAJy0XLk8j8YLSTt2/VMy9TAUx\n"
				"esXUiZj0Ung+gkr7A1K0NnwYxDzG2adMhf13upHoydu2ErLMmD6F77x+QuY/q7nc\n"
				"ZvO0tvcoAP6ToSDwiypU5dnTmnfkgwVwzFkNCi1sGRosEm8c/c/8MfK0I0nVdj1/\n"
				"BIkIG7tTDVi9JvkWYl0UlSKWTZKrntVwCmscfC02DGb+GoLbO9+QmiNM5Y3yOYZ4\n"
				"Pc7SSoKLL0rwJBmpPNs7boYsweeSuCAVu0shRfgC90odXcej2EN5ETfCuU1evXNW\n"
				"5cA+zZsDK/nWJwxBaW0CxAHX579FElFWlK4+BnzhZRdDhmJDnN5dh4ekJGM6Lg==\n"
				"-----END CERTIFICATE-----\n";
				
/* Client:
				Certificate:
				    Data:
        				Version: 3 (0x2)
        				Serial Number: 5 (0x5)
        				Signature Algorithm: sha1WithRSAEncryption
        				Issuer: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=chavroux.cowaddict.org/emailAddress=sdecugis@nict.go.jp
        				Validity
        				    Not Before: Oct 27 04:04:05 2009 GMT
        				    Not After : Oct 25 04:04:05 2019 GMT
        				Subject: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=client.test/emailAddress=client@test
        				Subject Public Key Info:
        				    Public Key Algorithm: rsaEncryption
        				    RSA Public Key: (1024 bit)
                				Modulus (1024 bit):
                				    00:bd:eb:50:1e:9d:7a:cd:9d:bb:e7:bc:4e:38:4a:
                				    b2:cc:9e:b4:89:77:01:ef:d1:c6:19:29:00:fe:ce:
                				    3c:62:05:13:b1:8c:ff:31:7a:0f:c1:2e:4b:3c:0c:
                				    40:1e:36:4e:76:da:0a:64:43:fc:1e:ea:0c:97:b2:
                				    57:9c:9c:8c:90:bd:eb:23:7b:b8:b7:5c:03:ed:6f:
                				    48:55:8a:88:08:38:c5:cd:33:b7:ab:a8:3a:6f:7f:
                				    13:10:65:a5:50:b9:f4:8b:cc:2e:e9:79:58:a6:11:
                				    f0:58:45:41:ef:36:b3:35:cb:14:ec:82:0c:ad:11:
                				    6a:ea:64:ef:28:a2:6e:47:45
                				Exponent: 65537 (0x10001)
        				X509v3 extensions:
        				    X509v3 Basic Constraints: 
                				CA:FALSE
        				    Netscape Comment: 
                				OpenSSL Generated Certificate
        				    X509v3 Subject Key Identifier: 
                				BE:B3:89:4F:9D:8F:6C:20:C4:D0:3E:6A:05:11:82:50:54:49:70:A2
        				    X509v3 Authority Key Identifier: 
                				keyid:92:AA:55:9F:B3:77:82:68:89:ED:7E:73:43:66:E4:8B:EE:EA:BF:85

				    Signature Algorithm: sha1WithRSAEncryption
        				a3:88:f5:15:b5:ad:20:60:a1:85:19:3f:b9:5e:1e:be:31:7f:
        				84:7a:c2:18:3a:63:6a:67:1f:46:86:4d:10:d6:1d:ad:a2:c8:
        				0b:95:33:fa:e4:05:f4:b8:70:34:77:f7:85:6e:70:46:ac:39:
        				54:a9:5f:ea:5e:d1:33:bb:c9:a3:42:81:41:90:25:b5:92:8b:
        				e8:6e:3e:97:06:dd:9a:cc:29:61:34:5a:d3:1c:5d:ad:d1:a3:
        				eb:6a:47:b4:d0:c2:17:89:e1:e2:2d:36:18:50:1a:e7:d4:fc:
        				38:2e:47:0b:39:50:87:2f:aa:07:64:f8:9a:4d:47:01:da:10:
        				d8:97:c7:a6:13:bc:0e:ca:63:c1:f2:09:fb:f8:6a:a4:5f:08:
        				b5:ad:ed:4f:71:b9:89:7f:43:27:85:72:e7:8d:a8:4a:cc:f6:
        				36:ca:8a:ae:82:b5:a8:42:41:99:87:84:7c:f0:90:fd:ca:96:
        				37:a2:e0:d9:fa:dd:a4:c9:f1:50:b7:e5:e6:8f:af:83:8c:23:
        				b6:20:cc:66:e3:08:60:13:02:8f:42:3a:07:91:a7:38:b2:72:
        				16:fd:bd:a9:60:f0:e2:9f:23:f3:c0:99:e3:17:bc:00:7c:b3:
        				89:9c:ea:fa:3e:f6:69:a1:98:c2:ec:46:da:70:b6:f9:c3:93:
        				a7:fc:36:dd
*/
static char client_cert_data[] ="-----BEGIN CERTIFICATE-----\n"
				"MIIDiTCCAnGgAwIBAgIBBTANBgkqhkiG9w0BAQUFADCBlDELMAkGA1UEBhMCSlAx\n"
				"DjAMBgNVBAgMBVRva3lvMRAwDgYDVQQHDAdLb2dhbmVpMQ0wCwYDVQQKDARXSURF\n"
				"MQ8wDQYDVQQLDAZBQUEgV0cxHzAdBgNVBAMMFmNoYXZyb3V4LmNvd2FkZGljdC5v\n"
				"cmcxIjAgBgkqhkiG9w0BCQEWE3NkZWN1Z2lzQG5pY3QuZ28uanAwHhcNMDkxMDI3\n"
				"MDQwNDA1WhcNMTkxMDI1MDQwNDA1WjCBgTELMAkGA1UEBhMCSlAxDjAMBgNVBAgM\n"
				"BVRva3lvMRAwDgYDVQQHDAdLb2dhbmVpMQ0wCwYDVQQKDARXSURFMQ8wDQYDVQQL\n"
				"DAZBQUEgV0cxFDASBgNVBAMMC2NsaWVudC50ZXN0MRowGAYJKoZIhvcNAQkBFgtj\n"
				"bGllbnRAdGVzdDCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAvetQHp16zZ27\n"
				"57xOOEqyzJ60iXcB79HGGSkA/s48YgUTsYz/MXoPwS5LPAxAHjZOdtoKZEP8HuoM\n"
				"l7JXnJyMkL3rI3u4t1wD7W9IVYqICDjFzTO3q6g6b38TEGWlULn0i8wu6XlYphHw\n"
				"WEVB7zazNcsU7IIMrRFq6mTvKKJuR0UCAwEAAaN7MHkwCQYDVR0TBAIwADAsBglg\n"
				"hkgBhvhCAQ0EHxYdT3BlblNTTCBHZW5lcmF0ZWQgQ2VydGlmaWNhdGUwHQYDVR0O\n"
				"BBYEFL6ziU+dj2wgxNA+agURglBUSXCiMB8GA1UdIwQYMBaAFJKqVZ+zd4Joie1+\n"
				"c0Nm5Ivu6r+FMA0GCSqGSIb3DQEBBQUAA4IBAQCjiPUVta0gYKGFGT+5Xh6+MX+E\n"
				"esIYOmNqZx9Ghk0Q1h2tosgLlTP65AX0uHA0d/eFbnBGrDlUqV/qXtEzu8mjQoFB\n"
				"kCW1kovobj6XBt2azClhNFrTHF2t0aPrake00MIXieHiLTYYUBrn1Pw4LkcLOVCH\n"
				"L6oHZPiaTUcB2hDYl8emE7wOymPB8gn7+GqkXwi1re1PcbmJf0MnhXLnjahKzPY2\n"
				"yoqugrWoQkGZh4R88JD9ypY3ouDZ+t2kyfFQt+Xmj6+DjCO2IMxm4whgEwKPQjoH\n"
				"kac4snIW/b2pYPDinyPzwJnjF7wAfLOJnOr6PvZpoZjC7EbacLb5w5On/Dbd\n"
				"-----END CERTIFICATE-----\n";
static char client_priv_data[] ="-----BEGIN RSA PRIVATE KEY-----\n"
				"MIICXgIBAAKBgQC961AenXrNnbvnvE44SrLMnrSJdwHv0cYZKQD+zjxiBROxjP8x\n"
				"eg/BLks8DEAeNk522gpkQ/we6gyXslecnIyQvesje7i3XAPtb0hViogIOMXNM7er\n"
				"qDpvfxMQZaVQufSLzC7peVimEfBYRUHvNrM1yxTsggytEWrqZO8oom5HRQIDAQAB\n"
				"AoGBAIYnsOLPby3LnC5n8AEHkyHDgdgQvsd/MSYYtuFHIZRD7dNfu+xhQru9TdvO\n"
				"84Pj7K07/FczRuc3gUmu6wBv/UIP9To15RHZh+/n537nybGus5S4IYKVvap477To\n"
				"0rQDf9ec27iw77gxb7moQ9Otuxwbv0h0Z+1EVLI8d8jHOq0BAkEA9YNr0R+7KXBS\n"
				"48yT43g5HpOFkTZzNXWVdpSvYGneb56wslk5Eatp235I4uz/a7Rej5v99W0M3nSe\n"
				"/AgHfYn75QJBAMYH/pBx/WkrLj+pPaARlNwInCIC5zUhr6B0IKCt2tvy5eyuc5sd\n"
				"AoTFaU+cSI+ZqsRzY8jMKkonktxBg48oJ+ECQQCt4AtlqcFVkbVCm8pJGQXq/7Ni\n"
				"qlthiwr1Vkv2TkQ4bPza8pGWT/3Cc2ePPyWN08n8jw+G11p72cAW4mDbqfN5AkEA\n"
				"mNYKrkiLn+NnqlJf8W4gSUGL3uQGtYbuGRQHKnuDckWhFm39YzWcgAQsJvkjN1EN\n"
				"7thvpsWLzfeE7ODTPGVtgQJATObxYJOt6rms3fAStwuXW3ET77TA1ja4XsUEe5Yu\n"
				"JpcQOruJb9XwndqzNbL0dSUePb9gFiBCGKYOyreNTTRTmw==\n"
				"-----END RSA PRIVATE KEY-----\n";
				
/* Server:
				Certificate:
				    Data:
        				Version: 3 (0x2)
        				Serial Number: 4 (0x4)
        				Signature Algorithm: sha1WithRSAEncryption
        				Issuer: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=chavroux.cowaddict.org/emailAddress=sdecugis@nict.go.jp
        				Validity
        				    Not Before: Oct 27 04:03:39 2009 GMT
        				    Not After : Oct 25 04:03:39 2019 GMT
        				Subject: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=serv.test/emailAddress=serv@test
        				Subject Public Key Info:
        				    Public Key Algorithm: rsaEncryption
        				    RSA Public Key: (1024 bit)
                				Modulus (1024 bit):
                				    00:a6:f7:1c:a9:90:5b:fa:c8:f6:a3:04:0c:d0:8b:
                				    45:c3:90:f7:2d:c2:c9:d7:bd:66:8a:7c:1c:51:89:
                				    40:9e:cd:70:57:cb:00:47:a3:e8:76:8b:00:b3:c9:
                				    c3:0d:b1:b9:2a:08:9f:52:92:82:d3:18:c1:d8:d1:
                				    b8:1e:fd:71:fe:23:ec:19:e9:6d:9d:fd:ae:88:bc:
                				    39:44:7a:37:ad:c6:88:d1:64:7c:b1:d4:3c:a9:30:
                				    c4:de:51:02:c4:48:4f:25:3e:2f:93:ae:25:32:66:
                				    9a:dc:f4:44:45:ff:7f:12:49:97:0d:01:8d:13:9a:
                				    d3:8f:9e:2d:62:95:02:0a:c7
                				Exponent: 65537 (0x10001)
        				X509v3 extensions:
        				    X509v3 Basic Constraints: 
                				CA:FALSE
        				    Netscape Comment: 
                				OpenSSL Generated Certificate
        				    X509v3 Subject Key Identifier: 
                				0C:33:C4:7F:39:D0:34:FF:F8:61:A1:46:8B:49:1D:A3:57:B3:4D:58
        				    X509v3 Authority Key Identifier: 
                				keyid:92:AA:55:9F:B3:77:82:68:89:ED:7E:73:43:66:E4:8B:EE:EA:BF:85

				    Signature Algorithm: sha1WithRSAEncryption
        				87:f5:49:a6:04:f9:98:9a:f1:1a:68:ce:06:ae:4c:0c:08:eb:
        				ba:98:e7:3f:df:22:7f:35:88:1d:b7:8a:f3:89:a3:68:0d:53:
        				45:eb:23:a1:dd:6b:dc:b0:80:58:0c:10:0b:49:74:ea:a8:b6:
        				8c:2e:c6:73:dc:7a:74:c7:59:3e:79:5a:d2:5c:15:0b:f1:d8:
        				19:37:2a:c0:22:75:10:3f:4c:e9:a1:e0:eb:b2:9e:09:70:3d:
        				2a:4c:fe:9c:99:36:4b:aa:6c:e1:8b:9c:aa:e1:29:1f:49:6b:
        				14:db:12:ae:cf:68:4a:dd:03:e1:3b:ad:79:b4:54:84:1d:bb:
        				ac:45:c4:85:f1:03:65:65:96:23:ae:e7:97:3c:5c:db:ce:55:
        				34:5d:c3:73:ec:cd:f6:0f:a5:81:5f:c2:ab:a3:42:fa:36:7f:
        				83:ef:db:0f:cd:62:0b:ea:d9:4f:73:35:68:5f:23:d5:0a:be:
        				ff:7f:23:9a:af:0d:a5:f8:3e:3a:f0:63:1c:e1:d2:96:81:cf:
        				7b:5a:6b:d0:9b:67:56:9e:aa:a9:e8:f1:6c:fb:54:2b:1a:f4:
        				ef:16:5a:be:1d:a9:c8:d6:cc:f7:42:8c:fe:83:2c:84:8c:80:
        				fb:1c:88:f6:35:1c:ae:43:72:fa:68:30:9c:25:8b:db:2c:84:
        				87:76:9d:b9
*/
static char server_cert_data[] ="-----BEGIN CERTIFICATE-----\n"
				"MIIDhDCCAmygAwIBAgIBBDANBgkqhkiG9w0BAQUFADCBlDELMAkGA1UEBhMCSlAx\n"
				"DjAMBgNVBAgMBVRva3lvMRAwDgYDVQQHDAdLb2dhbmVpMQ0wCwYDVQQKDARXSURF\n"
				"MQ8wDQYDVQQLDAZBQUEgV0cxHzAdBgNVBAMMFmNoYXZyb3V4LmNvd2FkZGljdC5v\n"
				"cmcxIjAgBgkqhkiG9w0BCQEWE3NkZWN1Z2lzQG5pY3QuZ28uanAwHhcNMDkxMDI3\n"
				"MDQwMzM5WhcNMTkxMDI1MDQwMzM5WjB9MQswCQYDVQQGEwJKUDEOMAwGA1UECAwF\n"
				"VG9reW8xEDAOBgNVBAcMB0tvZ2FuZWkxDTALBgNVBAoMBFdJREUxDzANBgNVBAsM\n"
				"BkFBQSBXRzESMBAGA1UEAwwJc2Vydi50ZXN0MRgwFgYJKoZIhvcNAQkBFglzZXJ2\n"
				"QHRlc3QwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAKb3HKmQW/rI9qMEDNCL\n"
				"RcOQ9y3Cyde9Zop8HFGJQJ7NcFfLAEej6HaLALPJww2xuSoIn1KSgtMYwdjRuB79\n"
				"cf4j7BnpbZ39roi8OUR6N63GiNFkfLHUPKkwxN5RAsRITyU+L5OuJTJmmtz0REX/\n"
				"fxJJlw0BjROa04+eLWKVAgrHAgMBAAGjezB5MAkGA1UdEwQCMAAwLAYJYIZIAYb4\n"
				"QgENBB8WHU9wZW5TU0wgR2VuZXJhdGVkIENlcnRpZmljYXRlMB0GA1UdDgQWBBQM\n"
				"M8R/OdA0//hhoUaLSR2jV7NNWDAfBgNVHSMEGDAWgBSSqlWfs3eCaIntfnNDZuSL\n"
				"7uq/hTANBgkqhkiG9w0BAQUFAAOCAQEAh/VJpgT5mJrxGmjOBq5MDAjrupjnP98i\n"
				"fzWIHbeK84mjaA1TResjod1r3LCAWAwQC0l06qi2jC7Gc9x6dMdZPnla0lwVC/HY\n"
				"GTcqwCJ1ED9M6aHg67KeCXA9Kkz+nJk2S6ps4YucquEpH0lrFNsSrs9oSt0D4Tut\n"
				"ebRUhB27rEXEhfEDZWWWI67nlzxc285VNF3Dc+zN9g+lgV/Cq6NC+jZ/g+/bD81i\n"
				"C+rZT3M1aF8j1Qq+/38jmq8Npfg+OvBjHOHSloHPe1pr0JtnVp6qqejxbPtUKxr0\n"
				"7xZavh2pyNbM90KM/oMshIyA+xyI9jUcrkNy+mgwnCWL2yyEh3aduQ==\n"
				"-----END CERTIFICATE-----\n";
static char server_priv_data[] ="-----BEGIN RSA PRIVATE KEY-----\n"
				"MIICXQIBAAKBgQCm9xypkFv6yPajBAzQi0XDkPctwsnXvWaKfBxRiUCezXBXywBH\n"
				"o+h2iwCzycMNsbkqCJ9SkoLTGMHY0bge/XH+I+wZ6W2d/a6IvDlEejetxojRZHyx\n"
				"1DypMMTeUQLESE8lPi+TriUyZprc9ERF/38SSZcNAY0TmtOPni1ilQIKxwIDAQAB\n"
				"AoGAZv3Ddm0P79CLIt9asEFY1VvUvSuMqkGwwPfx1/HcJJkBFYapM4fN22G/Gyf3\n"
				"47ifSWhsLtklTeXVnVMwSh14dJaJQuSEnaFnUUWfjiRbEAXZnMFwAIiaszEZbPap\n"
				"NUNpcGl06FZrphYAMkjOVUfjCjfOZDAvL4JGpo271Zx4l0ECQQDYoFFQpBCPx0PK\n"
				"TWUmvatXI/Amo94XkGfofbdeeI8PiAJBO5UI6rmjjIVwsJwO9dQb/IlP1/OnBeJv\n"
				"p9YW5uixAkEAxVAOKu7mpGu0Q/K2iEUUYDX9YHf253kgkdIDF4iZk4Tcecjoxuru\n"
				"fIWu9dMtyDVV+HT2X4cNEnO1/oS3kJII9wJBAJkdwDwiqz4lV6o/yFZ4zAoc8dsu\n"
				"CoZXYMq5SYox5tTQit928OHLn4mVgqBjhPsiEVnyx0+zUZpmE2ZemHm5nxECQHfE\n"
				"FBVzVYRP6+eil7E3XRrZKqc3qiLunxpkA4RxYebtKnaxwLmdOI1VB9InEQ8JcNmT\n"
				"BUkOzJx6p+mJ3XJfchkCQQDWmbMYYJajsjlS4YpdUUj7cBSotA6vtkNVHFr0/ak/\n"
				"S+tLkMNuruaInWizK+BKYTIJLlQDf5u5NTrw41vye5Hv\n"
				"-----END RSA PRIVATE KEY-----\n";

/* Expired:
				Certificate:
				    Data:
        				Version: 3 (0x2)
        				Serial Number: 6 (0x6)
        				Signature Algorithm: sha1WithRSAEncryption
        				Issuer: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=chavroux.cowaddict.org/emailAddress=sdecugis@nict.go.jp
        				Validity
        				    Not Before: Oct 27 04:06:35 2009 GMT
        				    Not After : Oct 28 04:06:35 2009 GMT
        				Subject: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=expired.test/emailAddress=expired@test
        				Subject Public Key Info:
        				    Public Key Algorithm: rsaEncryption
        				    RSA Public Key: (1024 bit)
                				Modulus (1024 bit):
                				    00:e3:17:15:54:85:dc:cf:c7:a0:32:4a:49:7d:55:
                				    75:9b:29:15:db:7e:87:17:d9:0e:65:44:53:d7:19:
                				    37:27:c7:c6:fe:c6:dc:72:2b:dc:86:1a:ff:24:6c:
                				    63:3f:75:9c:0a:14:e1:70:06:79:d4:b9:26:d4:68:
                				    4c:28:38:ba:34:60:56:02:3d:94:55:4a:1f:4e:5a:
                				    f0:a5:71:4c:3e:71:69:39:ad:bc:aa:55:35:fb:73:
                				    5b:5f:6c:30:71:8e:8a:b6:a5:06:cc:ee:dd:29:c7:
                				    52:0d:a7:9c:0f:a1:ba:52:11:e2:1b:b9:74:6b:08:
                				    87:11:d2:ec:a9:ac:63:63:4f
                				Exponent: 65537 (0x10001)
        				X509v3 extensions:
        				    X509v3 Basic Constraints: 
                				CA:FALSE
        				    Netscape Comment: 
                				OpenSSL Generated Certificate
        				    X509v3 Subject Key Identifier: 
                				1C:AF:66:42:5B:AD:AA:A5:9B:D9:AE:3A:C1:5A:AC:2F:CC:CE:22:6C
        				    X509v3 Authority Key Identifier: 
                				keyid:92:AA:55:9F:B3:77:82:68:89:ED:7E:73:43:66:E4:8B:EE:EA:BF:85

				    Signature Algorithm: sha1WithRSAEncryption
        				60:8f:55:55:59:82:0f:64:cb:b8:11:c8:44:ce:bf:69:07:0d:
        				be:c2:34:be:42:6a:78:15:39:9f:be:8a:17:d6:43:42:c9:7c:
        				f1:6d:5d:aa:c3:1b:4d:b0:f0:b6:73:46:2a:87:cd:55:56:a3:
        				6d:cc:de:a8:28:6a:53:85:9e:e5:68:b7:3c:f5:72:13:7b:d0:
        				21:f2:91:49:35:e0:37:1e:28:19:d5:1b:cc:e1:32:1e:7f:b0:
        				86:df:43:a4:47:0f:29:0b:eb:51:60:9a:f5:ca:50:f4:2d:59:
        				cd:fc:50:9d:29:ed:45:98:de:a2:5c:d1:b5:7a:34:ad:7a:73:
        				48:8b:a2:9b:89:8e:4a:2e:2a:04:19:d6:62:6a:0d:f0:96:f2:
        				f0:d0:22:77:3b:7f:b1:2a:f4:3b:17:47:5e:38:07:09:65:ad:
        				1d:ea:46:69:6a:96:b6:6b:3b:5c:cc:6e:30:d7:cb:53:69:59:
        				c2:63:78:2b:03:d4:d4:f7:17:29:99:9a:43:ff:78:0a:af:42:
        				c5:b3:8d:09:38:5b:30:70:28:c1:97:ab:fd:7f:87:9a:ec:f2:
        				97:44:ff:f5:b9:41:30:d1:c6:32:98:69:34:c4:39:30:6f:e2:
        				d3:b2:70:97:66:ee:41:f5:ae:0f:09:f0:ed:60:96:67:a9:8a:
        				cd:d6:95:f2
*/
static char expired_cert_data[]="-----BEGIN CERTIFICATE-----\n"
				"MIIDizCCAnOgAwIBAgIBBjANBgkqhkiG9w0BAQUFADCBlDELMAkGA1UEBhMCSlAx\n"
				"DjAMBgNVBAgMBVRva3lvMRAwDgYDVQQHDAdLb2dhbmVpMQ0wCwYDVQQKDARXSURF\n"
				"MQ8wDQYDVQQLDAZBQUEgV0cxHzAdBgNVBAMMFmNoYXZyb3V4LmNvd2FkZGljdC5v\n"
				"cmcxIjAgBgkqhkiG9w0BCQEWE3NkZWN1Z2lzQG5pY3QuZ28uanAwHhcNMDkxMDI3\n"
				"MDQwNjM1WhcNMDkxMDI4MDQwNjM1WjCBgzELMAkGA1UEBhMCSlAxDjAMBgNVBAgM\n"
				"BVRva3lvMRAwDgYDVQQHDAdLb2dhbmVpMQ0wCwYDVQQKDARXSURFMQ8wDQYDVQQL\n"
				"DAZBQUEgV0cxFTATBgNVBAMMDGV4cGlyZWQudGVzdDEbMBkGCSqGSIb3DQEJARYM\n"
				"ZXhwaXJlZEB0ZXN0MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDjFxVUhdzP\n"
				"x6AySkl9VXWbKRXbfocX2Q5lRFPXGTcnx8b+xtxyK9yGGv8kbGM/dZwKFOFwBnnU\n"
				"uSbUaEwoOLo0YFYCPZRVSh9OWvClcUw+cWk5rbyqVTX7c1tfbDBxjoq2pQbM7t0p\n"
				"x1INp5wPobpSEeIbuXRrCIcR0uyprGNjTwIDAQABo3sweTAJBgNVHRMEAjAAMCwG\n"
				"CWCGSAGG+EIBDQQfFh1PcGVuU1NMIEdlbmVyYXRlZCBDZXJ0aWZpY2F0ZTAdBgNV\n"
				"HQ4EFgQUHK9mQlutqqWb2a46wVqsL8zOImwwHwYDVR0jBBgwFoAUkqpVn7N3gmiJ\n"
				"7X5zQ2bki+7qv4UwDQYJKoZIhvcNAQEFBQADggEBAGCPVVVZgg9ky7gRyETOv2kH\n"
				"Db7CNL5CangVOZ++ihfWQ0LJfPFtXarDG02w8LZzRiqHzVVWo23M3qgoalOFnuVo\n"
				"tzz1chN70CHykUk14DceKBnVG8zhMh5/sIbfQ6RHDykL61FgmvXKUPQtWc38UJ0p\n"
				"7UWY3qJc0bV6NK16c0iLopuJjkouKgQZ1mJqDfCW8vDQInc7f7Eq9DsXR144Bwll\n"
				"rR3qRmlqlrZrO1zMbjDXy1NpWcJjeCsD1NT3FymZmkP/eAqvQsWzjQk4WzBwKMGX\n"
				"q/1/h5rs8pdE//W5QTDRxjKYaTTEOTBv4tOycJdm7kH1rg8J8O1glmepis3WlfI=\n"
				"-----END CERTIFICATE-----\n";
static char expired_priv_data[]="-----BEGIN RSA PRIVATE KEY-----\n"
				"MIICXgIBAAKBgQDjFxVUhdzPx6AySkl9VXWbKRXbfocX2Q5lRFPXGTcnx8b+xtxy\n"
				"K9yGGv8kbGM/dZwKFOFwBnnUuSbUaEwoOLo0YFYCPZRVSh9OWvClcUw+cWk5rbyq\n"
				"VTX7c1tfbDBxjoq2pQbM7t0px1INp5wPobpSEeIbuXRrCIcR0uyprGNjTwIDAQAB\n"
				"AoGASwPoDui9XYHTIGm7xwRA+kVjLAOq+qy//aHJlEeHGcP7r1PfpHNqwH4QhGat\n"
				"jlv6dLYbFld9TVDwS8A8UBkVIPLWnCysd5tF2A4C5akx6ouW6HliW/JheYrgl8AV\n"
				"PVeR3bm91UbnpC0ABVlw87jp1Ovyr60Suo4jsoJz+CyTa2ECQQD0LJWpnwn1jIlR\n"
				"DGkLi7F3E70JJcdhTWzBjGFD+Na+/2ZO0MKLhK+O1WUkKa0oi+e5P1JOnGIpTI8c\n"
				"BJOO415RAkEA7hauapYuqGI/auSPH8/nFB5z1G94RTxo2a5THKcG5MqS/8N3ubFj\n"
				"i2PPS0lEYVjqoHEsZUsMnDmXp6KDKMAfnwJBAIp+T1UqM8fmsmwaEerOjRXxSCNM\n"
				"Hk5+T9Vn/jNDjOpAipLhrbbcx4bIWtmsGd8Jm6Fi3RhhcvvhxLorjlZZeEECQQCf\n"
				"IaPD88sNmlUewdLzhUbCiLQMadCuHflKfRxpyy1tYAQuVFxCTdDlynkzra25ju+K\n"
				"+vmcXjP4evnk/lbBtt+rAkEAgOr4Apgs3nMppngPV5yFx0NDqH2n8PlEAM1Il4Qs\n"
				"IuuK18v0KwlUGAfEEmCiNh1e1qkLmD0CnI2QjYAjcLQUhw==\n"
				"-----END RSA PRIVATE KEY-----\n";

struct fd_list eps = FD_LIST_INITIALIZER(eps);

struct connect_flags {
	int	proto;
};

void * connect_thr(void * arg)
{
	struct connect_flags * cf = arg;
	struct cnxctx * cnx = NULL;
	
	fd_log_threadname ( "testcnx:connect" );
	
	/* Connect to the server */
	switch (cf->proto) {
		case IPPROTO_TCP:
			{
				struct fd_endpoint * ep = (struct fd_endpoint *)(eps.next);
				cnx = fd_cnx_cli_connect_tcp( &ep->sa, sSSlen(&ep->ss) );
				CHECK( 1, cnx ? 1 : 0 );
			}
			break;
#ifndef DISABLE_SCTP
		case IPPROTO_SCTP:
			{
				cnx = fd_cnx_cli_connect_sctp(0, TEST_PORT, &eps);
				CHECK( 1, cnx ? 1 : 0 );
			}
			break;
#endif /* DISABLE_SCTP */
		default:
			CHECK( 0, 1 );
	}
	
	/* exit */
	return cnx;
}

struct handshake_flags {
	struct cnxctx * cnx;
	gnutls_certificate_credentials_t	creds;
	int ret;
};

void * handshake_thr(void * arg)
{
	struct handshake_flags * hf = arg;
	fd_log_threadname ( "testcnx:handshake" );
	hf->ret = fd_cnx_handshake(hf->cnx, GNUTLS_CLIENT, NULL, hf->creds);
	return NULL;
}
	
void * destroy_thr(void * arg)
{
	struct cnxctx * cnx = arg;
	fd_log_threadname ( "testcnx:destroy" );
	fd_cnx_destroy(cnx);
	return NULL;
}
	
/* Main test routine */
int main(int argc, char *argv[])
{
	gnutls_datum_t ca 		= { ca_data, 		sizeof(ca_data) 	  };
	gnutls_datum_t server_cert 	= { server_cert_data, 	sizeof(server_cert_data)  };
	gnutls_datum_t server_priv 	= { server_priv_data, 	sizeof(server_priv_data)  };
	gnutls_datum_t client_cert	= { client_cert_data, 	sizeof(client_cert_data)  };
	gnutls_datum_t client_priv 	= { client_priv_data, 	sizeof(client_priv_data)  };
	gnutls_datum_t expired_cert 	= { expired_cert_data, 	sizeof(expired_cert_data) };
	gnutls_datum_t expired_priv 	= { expired_priv_data, 	sizeof(expired_priv_data) };
	
	struct cnxctx * listener;
#ifndef DISABLE_SCTP
	struct cnxctx * listener_sctp;
#endif /* DISABLE_SCTP */
	struct cnxctx * server_side;
	struct cnxctx * client_side;
	pthread_t thr;
	int ret, i;
	uint8_t * cer_buf;
	size_t 	  cer_sz;
	uint8_t * rcv_buf;
	size_t 	  rcv_sz;
	
	/* First, initialize the daemon modules */
	INIT_FD();
	
	/* Set the CA parameter in the config */
	CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( fd_g_config->cnf_sec_data.credentials,
									 &ca,
									 GNUTLS_X509_FMT_PEM), );
	CHECK( 1, ret );
	
	/* Set the server credentials (in config) */
	CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( fd_g_config->cnf_sec_data.credentials,
									&server_cert,
									&server_priv,
									GNUTLS_X509_FMT_PEM), );
	CHECK( GNUTLS_E_SUCCESS, ret );
	
	/* Set the default priority */
	CHECK_GNUTLS_DO( ret = gnutls_priority_init( &fd_g_config->cnf_sec_data.prio_cache, GNUTLS_DEFAULT_PRIORITY, NULL), );
	CHECK( GNUTLS_E_SUCCESS, ret );
	
	/* Set default DH params */
	CHECK_GNUTLS_DO( ret = gnutls_dh_params_generate2( fd_g_config->cnf_sec_data.dh_cache, GNUTLS_DEFAULT_DHBITS), );
	CHECK( GNUTLS_E_SUCCESS, ret );
	
	/* Initialize the server addresses */
	{
		struct addrinfo hints, *ai, *aip;
		memset(&hints, 0, sizeof(hints));
		hints.ai_flags  = AI_NUMERICSERV;
		hints.ai_family = AF_INET;
		CHECK( 0, getaddrinfo("localhost", _stringize(TEST_PORT), &hints, &ai) );
		aip = ai;
		while (aip) {
			CHECK( 0, fd_ep_add_merge( &eps, aip->ai_addr, aip->ai_addrlen, EP_FL_DISC ));
			aip = aip->ai_next;
		};
		freeaddrinfo(ai);
	}
	
	/* Start the server(s) */
	{
		/* TCP server */
		listener = fd_cnx_serv_tcp(TEST_PORT, 0, (struct fd_endpoint *)(eps.next));
		CHECK( 1, listener ? 1 : 0 );
		
		/* Accept incoming clients */
		CHECK( 0, fd_cnx_serv_listen(listener));

#ifndef DISABLE_SCTP
		/* SCTP server */
		listener_sctp = fd_cnx_serv_sctp(TEST_PORT, &eps);
		CHECK( 1, listener_sctp ? 1 : 0 );
		
		/* Accept incoming clients */
		CHECK( 0, fd_cnx_serv_listen(listener_sctp));
#endif /* DISABLE_SCTP */

	}	
	
	/* Initialize the CER message */
	{
		struct msg * cer;
		struct dict_object * model = NULL;
		struct avp * oh;
		union avp_value value;

		/* Find the CER dictionary object */
		CHECK( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Capabilities-Exchange-Request", &model, ENOENT ) );

		/* Create the instance */
		CHECK( 0, fd_msg_new ( model, 0, &cer ) );
		
		/* Now find the Origin-Host dictionary object */
		CHECK( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &model, ENOENT ) );

		/* Create the instance */
		CHECK( 0, fd_msg_avp_new ( model, 0, &oh ) );
		value.os.data = "Client.side";
		value.os.len = strlen(value.os.data);
		CHECK( 0, fd_msg_avp_setvalue ( oh, &value ) );
		
		/* Add the AVP */
		CHECK( 0, fd_msg_avp_add( cer, MSG_BRW_LAST_CHILD, oh) );

		#if 1
		/* For debug: dump the object */
		fd_log_debug("Dumping CER\n");
		fd_msg_dump_walk(0, cer);
		#endif
			
		CHECK( 0, fd_msg_bufferize( cer, &cer_buf, &cer_sz ) );
		CHECK( 0, fd_msg_free(cer) );
	}
	
	/* Simple TCP client / server test (no TLS) */
	{
		struct connect_flags cf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_TCP;
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, 0, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		CHECK( 0, fd_cnx_start_clear(server_side, 0) );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		CHECK( 0, fd_cnx_start_clear(client_side, 0) );
		
		/* Send a message and receive it */
		CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* Do it in the other direction */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* Now close the connection */
		fd_cnx_destroy(client_side);
		fd_cnx_destroy(server_side);
	}
		
#ifndef DISABLE_SCTP
	/* Simple SCTP client / server test (no TLS) */
	{
		struct connect_flags cf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_SCTP;
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, 0, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		
		CHECK( 0, fd_cnx_start_clear(server_side, 1) );
		
		/* Send a message and receive it */
		CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
		CHECK( EINVAL, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( 0, fd_cnx_start_clear(client_side, 0) );
		CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* Do it in the other direction */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* Do it one more time to use another stream */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* Now close the connection */
		fd_cnx_destroy(client_side);
		fd_cnx_destroy(server_side);
	}
#endif /* DISABLE_SCTP */
	
	/* TCP Client / server emulating old Diameter behavior (handshake after 1 message exchange) */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_TCP;
		
		memset(&hf, 0, sizeof(hf));
		
		/* Initialize remote certificate */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_allocate_credentials (&hf.creds), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		/* Set the CA */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &ca, GNUTLS_X509_FMT_PEM), );
		CHECK( 1, ret );
		/* Set the key */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &client_cert, &client_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, 0, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* In legacy Diameter, we exchange first one message (CER / CEA) */
		
		CHECK( 0, fd_cnx_start_clear(server_side, 0) );
		CHECK( 0, fd_cnx_start_clear(client_side, 0) );
		
		/* Send a message and receive it */
		CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* And the supposed reply */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* At this point in legacy Diameter we start the handshake */
		CHECK( 0, pthread_create(&thr, 0, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 10; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		}
		
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, 0, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
		
#ifndef DISABLE_SCTP
	/* SCTP Client / server emulating old Diameter behavior (handshake after 1 message exchange) */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_SCTP;
		
		memset(&hf, 0, sizeof(hf));
		
		/* Initialize remote certificate */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_allocate_credentials (&hf.creds), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		/* Set the CA */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &ca, GNUTLS_X509_FMT_PEM), );
		CHECK( 1, ret );
		/* Set the key */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &client_cert, &client_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, 0, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* In legacy Diameter, we exchange first one message (CER / CEA) */
		
		CHECK( 0, fd_cnx_start_clear(server_side, 0) );
		CHECK( 0, fd_cnx_start_clear(client_side, 0) );
		
		/* Send a message and receive it */
		CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* And the supposed reply */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		
		/* At this point in legacy Diameter we start the handshake */
		CHECK( 0, pthread_create(&thr, 0, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 100; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		}
		
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, 0, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
#endif /* DISABLE_SCTP */
	
	/* TCP Client / server emulating new Diameter behavior (handshake at connection directly) */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		int i;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_TCP;
		
		memset(&hf, 0, sizeof(hf));
		
		/* Initialize remote certificate */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_allocate_credentials (&hf.creds), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		/* Set the CA */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &ca, GNUTLS_X509_FMT_PEM), );
		CHECK( 1, ret );
		/* Set the key */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &client_cert, &client_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, 0, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, 0, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 10; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		}
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, 0, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* That's all for the tests yet */
	PASSTEST();
} 
	
