/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2020, WIDE Project and NICT								 *
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

#ifndef NB_STREAMS
#define NB_STREAMS	10
#endif /* NB_STREAMS */

#ifndef GNUTLS_DEFAULT_PRIORITY
# define GNUTLS_DEFAULT_PRIORITY "NORMAL"
#endif /* GNUTLS_DEFAULT_PRIORITY */

#ifndef GNUTLS_DEFAULT_DHBITS
# define GNUTLS_DEFAULT_DHBITS 1024
#endif /* GNUTLS_DEFAULT_DHBITS */


/* The cryptographic data */
static char ca_data[] =		""
"-----BEGIN CERTIFICATE-----\n"
"MIIFljCCA36gAwIBAgIJAIicnqmf2SGHMA0GCSqGSIb3DQEBCwUAMF8xFzAVBgNV\n"
"BAMMDmNhLmxvY2FsZG9tYWluMQswCQYDVQQGEwJGUjEMMAoGA1UECAwDQmRSMQww\n"
"CgYDVQQHDANBaXgxCzAJBgNVBAoMAmZEMQ4wDAYDVQQLDAVUZXN0czAgFw0xOTEx\n"
"MjQxNzA2NDNaGA8yMTE5MTAzMTE3MDY0M1owXzEXMBUGA1UEAwwOY2EubG9jYWxk\n"
"b21haW4xCzAJBgNVBAYTAkZSMQwwCgYDVQQIDANCZFIxDDAKBgNVBAcMA0FpeDEL\n"
"MAkGA1UECgwCZkQxDjAMBgNVBAsMBVRlc3RzMIICIjANBgkqhkiG9w0BAQEFAAOC\n"
"Ag8AMIICCgKCAgEAyj+ATbTTOkXkhnMgp4PQOcN8AppX8yKYFyWKlYfaoq1CGWhR\n"
"zMuvlgKymGPmuYDwI2ap5z9GbMeQzhq/lnz2jg37E0aWhgPEbMDknjMMgrS5N0m0\n"
"Z7Lqhj/JVcuhxKWTtPPbAaNMZFNJ+MZkI3tGPXNaih6qNGQKFplkv22HsIdvnvlB\n"
"RQ0blcaJ9KYEaDeBjjX8fjkUsxw7gHNw2Gi3O0XiftNqNywrYrLzyuKZ6T3Tq9Xp\n"
"KilwhymC3b/MiqXHeYSAHLRw3qCoaPAx01NESgr1sGoMcKyxoAkzvXsVw0fIpO5n\n"
"pShEeOf+/ID+tWJ7EG1G13fsYXTiawsNMjHcnhrtr7lYC43/xiyjAr4V5Uf51Dt4\n"
"aQWbEGOGJr4jniFt0Wis8MFrAYRekyV5ytJ6YCWt5nRs6Aujs8yjD89cGBddixfr\n"
"7NfwR/dZsv7h5cufdfh8W2jzMt9DeAHM5WANIwFhrI4rfMCKS4bG9BFCAD1wEbhg\n"
"7k/wcyOBXDIR1W1yP3SmrQRL/JiOW5jU76VQAURnmqnM9ayUdWKfROhoWuoMRIMT\n"
"IHLxkotlQUECbGZX8gzkvlnxeogPH8KVul99USpDGU66H+RUxVOUNIPGhiZUy6KU\n"
"nFQUHXXF1PB5U8JfhdOp5ODHDY0quUDG3FXpyf0NsNcbYzb8WWssLreIthcCAwEA\n"
"AaNTMFEwHQYDVR0OBBYEFFYkMOz0TqjaKeuXQMYy7Q+H9fYAMB8GA1UdIwQYMBaA\n"
"FFYkMOz0TqjaKeuXQMYy7Q+H9fYAMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcN\n"
"AQELBQADggIBAHV7IfJgmmxvsal+C6m3exoeQwlwQP5+5xF8L+FpUyJj0TWukteM\n"
"JurAESZL7SRqBHt3XGXnNf+ghs+/hkWIoYoFj7cermN3sDqZgSqB0eOUlWEuT+bR\n"
"NUxjb1W890ok7CqN0m7qs72oYSN53De1526sfkdpc8xEiy2UGjiwIdc/3uZ3md+I\n"
"LOpRHoXUrw1rurmi2/F/oyDjmm9ABicNb4B8e7u1gyrdKB2H5z2HRgUbI1sf/l9I\n"
"7u17zrieJQ7wcBlfGfjee+Iop6/ZhZdhyF5grv7OA/YgNgTl3C3MBH22912tHj4o\n"
"3Sxpqt2q0x80Lv4nhrlnVywwp5s1tyDG8LTcOcPpjQwI8zuCTOAZIzgTarFF0/G6\n"
"NN+ZSl+RUdPNN3g0Eo2KB7QxPsSEsHmgS00I/YPhGp6kHcD8WRij253A9odXxKL+\n"
"P4nCbgIP/YGdHbRTTzlVMjHHSWbtWwEYzTIcn+2bHFXBQB7TE0MrlqaN4h+3UZ2r\n"
"YCt0qyql/RJQ+txHAp43WoEg3i8regit3F6h9plDtXpodVacMREofeWlyv1zG5nD\n"
"fjXqkb27a0tcY19OOrOU4QBks3bYLPjr4RReOvI6DSKtcXyptbjxMNihrsNpzfhH\n"
"CQ6aE6Uebc0z+DEH5g9cc7l+RlQkeOmTEXhNIuR9JVt1H4O5YXDwKZ86\n"
"-----END CERTIFICATE-----\n"
;
				
/* Client:
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: 1 (0x1)
    Signature Algorithm: sha256WithRSAEncryption
        Issuer: CN=ca.localdomain, C=FR, ST=BdR, L=Aix, O=fD, OU=Tests
        Validity
            Not Before: Nov 24 17:06:43 2019 GMT
            Not After : Oct 31 17:06:43 2119 GMT
        Subject: C=FR, ST=BdR, O=fD, OU=Tests, CN=client.test
        Subject Public Key Info:
            Public Key Algorithm: rsaEncryption
                Public-Key: (2048 bit)
                Modulus:
                    00:c2:e8:d6:a4:27:56:e5:cb:c9:c1:cb:8e:19:f2:
                    79:35:96:ca:f2:54:ac:3e:8f:be:8e:77:44:13:c3:
                    fd:74:21:45:bb:9e:65:f1:9d:46:af:cc:1e:af:fa:
                    d5:93:91:38:32:0e:3c:b7:5b:a3:9c:7d:4c:94:0a:
                    66:3a:5d:0c:8e:1f:22:59:0d:dd:a6:de:c1:91:0a:
                    8c:70:c0:c6:2c:c1:6f:68:29:45:b4:d5:14:74:8d:
                    31:bb:74:1e:2d:b6:78:74:0b:95:bd:96:35:1d:24:
                    1f:7d:c9:d5:fa:5a:b6:27:87:38:cc:f4:8d:a3:98:
                    d9:ff:32:a3:53:db:eb:b5:e5:69:a9:2b:f4:da:ec:
                    bd:96:f0:80:46:0d:f9:48:03:39:25:4b:a3:2f:7b:
                    59:81:8b:6e:17:67:2e:3b:84:e9:ff:ac:33:7c:60:
                    34:10:00:c9:7b:1f:11:e6:b7:a1:57:01:46:44:ae:
                    2d:8c:ba:d6:08:3d:5d:98:d3:66:74:d8:15:fe:75:
                    09:61:24:5c:a2:a2:b3:0b:bd:b6:ef:98:b4:07:8d:
                    86:ae:29:87:7b:98:3a:3f:ef:4f:f6:c2:ab:40:32:
                    91:06:eb:8e:c8:9d:36:45:84:d4:d3:f5:85:89:ed:
                    2f:6f:e0:40:a3:d9:0b:e2:a7:80:b1:04:ef:27:70:
                    9a:f1
                Exponent: 65537 (0x10001)
        X509v3 extensions:
            X509v3 Basic Constraints: 
                CA:FALSE
            Netscape Comment: 
                OpenSSL Generated Certificate
            X509v3 Subject Key Identifier: 
                52:E8:18:E1:33:09:C4:AF:65:24:25:C4:72:A8:07:BB:B7:3B:C0:31
            X509v3 Authority Key Identifier: 
                keyid:56:24:30:EC:F4:4E:A8:DA:29:EB:97:40:C6:32:ED:0F:87:F5:F6:00

    Signature Algorithm: sha256WithRSAEncryption
         7c:1f:50:97:70:ee:43:3c:e6:3e:ec:17:f5:78:d8:32:d4:73:
         e9:bd:e2:9a:b4:47:d7:c8:e5:0f:38:d3:14:a8:18:ac:fa:8e:
         8e:68:80:95:8e:0b:5b:09:25:d4:83:b6:65:ca:8e:d7:a4:31:
         c1:86:00:cc:1f:c1:60:0f:c2:f1:84:fe:30:9b:f1:d1:1a:96:
         d8:ae:e2:eb:28:87:7d:59:07:84:fe:c5:86:e8:41:12:98:a2:
         d9:d3:bd:ce:63:79:43:62:bd:13:74:fa:a4:24:48:7a:f5:e9:
         d9:d5:07:2e:33:f0:f5:0a:ab:49:41:ae:94:5c:06:bd:2e:5d:
         e8:b3:72:e4:76:13:3e:36:0d:a0:bc:f5:a2:b0:17:da:75:0b:
         ec:bd:0b:5b:86:a9:5f:b4:e3:3a:fa:ab:cd:4e:03:06:16:a8:
         50:10:65:56:60:bb:c4:96:e3:64:b3:7d:e6:5c:92:23:5c:18:
         cf:e7:f7:a1:0f:ac:99:25:b1:9b:67:63:44:30:ab:01:5b:73:
         6d:09:80:60:c3:4b:80:04:b9:d2:47:17:ac:b7:93:58:f6:7c:
         7c:fe:79:07:4d:20:57:68:c1:33:70:65:48:b5:c2:ac:9b:10:
         67:63:1d:d1:28:0b:6e:07:c5:95:bd:d9:5c:42:f2:33:fa:38:
         b1:02:3d:e2:91:b0:2c:04:66:21:35:86:68:70:06:88:a3:e3:
         43:b1:11:24:bc:33:ea:11:45:29:b0:7c:4c:25:62:7e:75:07:
         6f:58:4d:33:25:57:d8:8f:40:11:f2:66:8f:8d:6c:c6:9d:b6:
         d6:b5:96:75:09:b7:57:b0:6f:ed:ac:f7:9f:42:58:ce:cf:2f:
         3c:65:6d:eb:c2:d6:ae:ed:b5:37:03:24:ed:dd:bd:53:c5:93:
         e2:64:b3:8f:8f:8b:37:bf:fd:e4:de:b5:48:7e:64:6c:a1:a6:
         c6:af:72:5c:d0:46:ee:0d:82:18:c9:bb:3e:39:f7:49:90:0c:
         73:ae:23:a4:2f:f3:ff:dd:99:d0:ea:66:d8:f0:97:e9:88:05:
         48:83:3d:18:a5:0c:7c:5d:17:9d:9c:cc:4d:3c:cb:46:a9:dc:
         d0:69:5f:c0:73:50:2e:ea:42:f7:a1:41:e8:09:fa:56:54:15:
         f4:71:e1:bd:99:07:73:94:88:2e:fb:2e:f4:b6:5d:af:a6:81:
         42:3e:4c:b5:da:ab:52:6b:36:9b:66:a1:01:09:ec:c9:d9:1b:
         b0:18:8f:c1:de:7f:fa:a9:83:9f:f0:9d:b0:84:80:83:f6:0e:
         25:a7:82:55:af:63:e1:e5:85:9b:ef:ee:d7:12:ff:09:fc:41:
         ed:ae:f5:25:c9:e6:cd:73
*/
static char client_cert_data[] = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEpTCCAo2gAwIBAgIBATANBgkqhkiG9w0BAQsFADBfMRcwFQYDVQQDDA5jYS5s\n"
"b2NhbGRvbWFpbjELMAkGA1UEBhMCRlIxDDAKBgNVBAgMA0JkUjEMMAoGA1UEBwwD\n"
"QWl4MQswCQYDVQQKDAJmRDEOMAwGA1UECwwFVGVzdHMwIBcNMTkxMTI0MTcwNjQz\n"
"WhgPMjExOTEwMzExNzA2NDNaME4xCzAJBgNVBAYTAkZSMQwwCgYDVQQIDANCZFIx\n"
"CzAJBgNVBAoMAmZEMQ4wDAYDVQQLDAVUZXN0czEUMBIGA1UEAwwLY2xpZW50LnRl\n"
"c3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDC6NakJ1bly8nBy44Z\n"
"8nk1lsryVKw+j76Od0QTw/10IUW7nmXxnUavzB6v+tWTkTgyDjy3W6OcfUyUCmY6\n"
"XQyOHyJZDd2m3sGRCoxwwMYswW9oKUW01RR0jTG7dB4ttnh0C5W9ljUdJB99ydX6\n"
"WrYnhzjM9I2jmNn/MqNT2+u15WmpK/Ta7L2W8IBGDflIAzklS6Mve1mBi24XZy47\n"
"hOn/rDN8YDQQAMl7HxHmt6FXAUZEri2MutYIPV2Y02Z02BX+dQlhJFyiorMLvbbv\n"
"mLQHjYauKYd7mDo/70/2wqtAMpEG647InTZFhNTT9YWJ7S9v4ECj2Qvip4CxBO8n\n"
"cJrxAgMBAAGjezB5MAkGA1UdEwQCMAAwLAYJYIZIAYb4QgENBB8WHU9wZW5TU0wg\n"
"R2VuZXJhdGVkIENlcnRpZmljYXRlMB0GA1UdDgQWBBRS6BjhMwnEr2UkJcRyqAe7\n"
"tzvAMTAfBgNVHSMEGDAWgBRWJDDs9E6o2inrl0DGMu0Ph/X2ADANBgkqhkiG9w0B\n"
"AQsFAAOCAgEAfB9Ql3DuQzzmPuwX9XjYMtRz6b3imrRH18jlDzjTFKgYrPqOjmiA\n"
"lY4LWwkl1IO2ZcqO16QxwYYAzB/BYA/C8YT+MJvx0RqW2K7i6yiHfVkHhP7FhuhB\n"
"Epii2dO9zmN5Q2K9E3T6pCRIevXp2dUHLjPw9QqrSUGulFwGvS5d6LNy5HYTPjYN\n"
"oLz1orAX2nUL7L0LW4apX7TjOvqrzU4DBhaoUBBlVmC7xJbjZLN95lySI1wYz+f3\n"
"oQ+smSWxm2djRDCrAVtzbQmAYMNLgAS50kcXrLeTWPZ8fP55B00gV2jBM3BlSLXC\n"
"rJsQZ2Md0SgLbgfFlb3ZXELyM/o4sQI94pGwLARmITWGaHAGiKPjQ7ERJLwz6hFF\n"
"KbB8TCVifnUHb1hNMyVX2I9AEfJmj41sxp221rWWdQm3V7Bv7az3n0JYzs8vPGVt\n"
"68LWru21NwMk7d29U8WT4mSzj4+LN7/95N61SH5kbKGmxq9yXNBG7g2CGMm7Pjn3\n"
"SZAMc64jpC/z/92Z0Opm2PCX6YgFSIM9GKUMfF0XnZzMTTzLRqnc0GlfwHNQLupC\n"
"96FB6An6VlQV9HHhvZkHc5SILvsu9LZdr6aBQj5MtdqrUms2m2ahAQnsydkbsBiP\n"
"wd5/+qmDn/CdsISAg/YOJaeCVa9j4eWFm+/u1xL/CfxB7a71JcnmzXM=\n"
"-----END CERTIFICATE-----\n"
				;
static char client_priv_data[] = ""
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEpAIBAAKCAQEAwujWpCdW5cvJwcuOGfJ5NZbK8lSsPo++jndEE8P9dCFFu55l\n"
"8Z1Gr8wer/rVk5E4Mg48t1ujnH1MlApmOl0Mjh8iWQ3dpt7BkQqMcMDGLMFvaClF\n"
"tNUUdI0xu3QeLbZ4dAuVvZY1HSQffcnV+lq2J4c4zPSNo5jZ/zKjU9vrteVpqSv0\n"
"2uy9lvCARg35SAM5JUujL3tZgYtuF2cuO4Tp/6wzfGA0EADJex8R5rehVwFGRK4t\n"
"jLrWCD1dmNNmdNgV/nUJYSRcoqKzC72275i0B42GrimHe5g6P+9P9sKrQDKRBuuO\n"
"yJ02RYTU0/WFie0vb+BAo9kL4qeAsQTvJ3Ca8QIDAQABAoIBAG0X+hvDUSYqHHF2\n"
"R8FKMIHINyvQLOCPgG8pXldZ2eFIRkmvbQwBpfD75SlG0ohYPgX2ZhGTH06V62jp\n"
"MTL1pfNehdEmq6uc/ub56oWpwMKIOp0ojLWclmhuJynu4n1fpqf9XADTtELtVxsG\n"
"/9ezMkWJsEc3kpfBFDN82mIztAkItp+p/pChvc48p725AdOf8ZLlXW0rYKfv1J6l\n"
"4VGb2iYwokQJkx0FcgP1uo1m6Pk2xbKW6acn4g8hsvoOZ6MeJ1/Gh6hJjaQ6hw2i\n"
"Xs2effT8ZH0kDq/GikC+U+QJa3lvUdph9rtIYEL1z0WCUk+t7AsjtfqVPzuqYFO2\n"
"UKqteGECgYEA768K7sD0q+bH/P8tFALd+Ynj1bPdBOTsqPCaRiQeC8omoYpovI3z\n"
"bKEqehQA4OQ4PERrysOCQU4G+n/XV20LbvdfNx655swFhNLwbNbPfp6p6eLzfks3\n"
"52FP8B10scM7OCheHiGhyvxObTZjeVZ15eUYx8OaLCRiU9tPAuuo/5UCgYEA0C2E\n"
"bf+56XY+fxnRR6sNkVL4ztjWRku+zKTAikxu7GA9ueNBmkXSBVcTL1rj0aErOc+Z\n"
"IcUmCVoH67txmXed54NhTx7F7R+K1MBSfFS9WPMuOHfoOO0B4pH2V03bZf14WaAJ\n"
"a14WNRCHhp7bgkGPzJINi+X810dIXxw0/fBkhu0CgYBeaF4wCMTW6QIg/wnsdeyl\n"
"G9EoHb6S7PDi4lKCCjvjfO6WdoQmsOsPALRQfYyTCJG1+2VR6oxl2KwbAzv47bSx\n"
"MrLIbUvgQsBxvvyAgmQchbZ7r68lMc+FTelEtPWAB1xOFGhuCeVGpPbnQaMQ6iNq\n"
"OED4fm09sd8hFKMcjWJ1uQKBgQCxgw6YaWXS4EP8RzyAGyiPkaudXaKhAHeqspWH\n"
"mBNBtfMMbe8DqKOkcBJo39zBZOeh/RY7iIudj8qPRR9h2HCp+Api1/+36ZyNO41a\n"
"fTuT/JCeEAjRea+Qzhu7aCU7+33DFsbRacIP749QyGau1E7VBnlMoIkP3LWmfDvn\n"
"iTN/JQKBgQC2va9Ou4NNWu1VyFMO1BOo0Tw3FSPCqsZ+5iPHztw/b98EIkM72Ran\n"
"kcMwgXOMTnBN5GuGdvTARVs/PvYaHkcD7YPFR32yT1JVSOtRt6AQ+EJkVj9trPzM\n"
"Q6TrU0XBPpoXkOSDi0tLFbzdLbJLLbtIDGJfvETYNq4lTKMnvJ2hBQ==\n"
"-----END RSA PRIVATE KEY-----\n"
				;
				
/* Server:
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: 2 (0x2)
    Signature Algorithm: sha256WithRSAEncryption
        Issuer: CN=ca.localdomain, C=FR, ST=BdR, L=Aix, O=fD, OU=Tests
        Validity
            Not Before: Nov 24 17:06:43 2019 GMT
            Not After : Oct 31 17:06:43 2119 GMT
        Subject: C=FR, ST=BdR, O=fD, OU=Tests, CN=server.test
        Subject Public Key Info:
            Public Key Algorithm: rsaEncryption
                Public-Key: (2048 bit)
                Modulus:
                    00:a4:d4:cf:09:7c:35:02:38:34:ee:80:fd:bf:a0:
                    d2:f9:37:c2:3f:1c:3e:74:ac:38:c4:53:ce:f6:c2:
                    03:31:b8:2c:8e:b0:b5:f5:58:34:4b:ef:1d:ef:b6:
                    f0:66:1e:48:54:f1:b7:d5:53:13:98:81:00:a0:60:
                    84:f8:f2:bd:d1:d0:74:95:d8:eb:41:77:c2:ec:48:
                    5c:fa:fe:0c:ab:af:75:47:18:fa:96:27:cb:06:d8:
                    01:af:05:67:cc:10:ae:71:36:7b:6b:d7:fb:c9:c7:
                    e3:6a:e3:20:c3:2f:9e:5e:ef:68:2c:a5:80:c1:77:
                    85:ef:c0:0b:d4:00:57:07:e9:c4:fc:b1:df:93:64:
                    eb:1f:36:e3:81:74:e7:8c:91:8a:3d:09:60:dd:f5:
                    9c:93:21:49:9c:45:6f:6f:3a:d3:39:5c:5c:54:ed:
                    8c:d5:d8:28:d4:58:46:e7:6b:b2:93:09:62:8a:9c:
                    c7:23:dc:7f:71:93:8b:7e:11:67:73:52:3f:99:ea:
                    ec:9f:0e:6b:d3:0a:be:48:c2:c9:6b:05:a3:f4:d7:
                    20:73:83:43:f3:7b:17:1a:fb:73:36:7e:22:a0:68:
                    bc:63:ea:02:0f:7c:05:92:15:8b:b2:dc:89:a9:3d:
                    ac:31:d5:29:4b:0d:95:b7:2c:8e:6a:61:7b:f4:c5:
                    cf:cf
                Exponent: 65537 (0x10001)
        X509v3 extensions:
            X509v3 Basic Constraints: 
                CA:FALSE
            Netscape Comment: 
                OpenSSL Generated Certificate
            X509v3 Subject Key Identifier: 
                79:F6:AB:D6:1F:AF:03:E0:02:90:D8:EB:0E:D2:D4:0F:01:60:43:33
            X509v3 Authority Key Identifier: 
                keyid:56:24:30:EC:F4:4E:A8:DA:29:EB:97:40:C6:32:ED:0F:87:F5:F6:00

    Signature Algorithm: sha256WithRSAEncryption
         c8:69:0d:53:52:2c:5f:d9:8a:e9:96:98:27:8f:88:51:df:51:
         d2:fb:5c:7a:25:bb:4f:80:7a:8a:e9:f7:16:c0:d6:7e:8a:4c:
         78:81:2b:63:8f:bf:98:f2:91:fc:a7:a0:81:64:f3:c9:20:ab:
         52:fc:68:24:05:59:3e:ae:47:7b:c0:e1:97:7c:25:73:6b:a0:
         34:bd:90:61:62:51:67:1e:6c:1e:2d:8c:e1:bd:d6:a6:ba:d9:
         a9:8b:7a:c8:76:d0:25:83:28:5f:9c:1d:d5:ab:40:ce:0c:8e:
         96:8c:95:ba:bd:2f:fc:f3:81:80:15:5c:7d:5f:d2:28:bd:ea:
         68:3e:df:95:df:4a:1c:f3:fc:4d:f4:87:25:f1:4b:b2:7d:f3:
         ba:17:4a:2d:be:6d:a6:d6:f5:b0:cf:fc:2c:ec:5f:70:b5:30:
         3d:2c:c3:dc:33:b8:c8:2e:90:7e:62:2c:51:51:bf:91:80:a8:
         b6:f7:8d:3d:69:03:f4:07:ca:8c:8a:75:f0:4b:1b:44:eb:d1:
         76:fd:61:e1:7d:f7:35:6e:04:3b:5e:c9:a9:f1:d9:7b:ee:43:
         31:84:74:66:18:71:b6:b6:c7:98:3a:42:a6:03:ac:a6:b7:56:
         8c:76:a5:13:33:71:31:0c:43:b8:27:17:7c:c8:3c:0f:3d:16:
         86:0e:13:87:00:5b:af:9c:c3:e3:e7:37:fc:3e:07:54:63:53:
         af:d6:91:dd:1d:e2:25:d8:57:b0:bc:db:a8:2a:f8:23:e9:15:
         1e:01:f3:e3:37:e4:21:85:48:35:66:d0:6e:72:96:ca:09:d7:
         26:99:d5:5d:fa:b2:39:94:15:d6:29:45:e7:5c:c6:ea:61:8f:
         4d:6a:68:32:93:b1:1d:b2:28:8a:b0:43:b7:17:c4:81:eb:4e:
         ef:91:ed:ca:d9:d8:fa:b2:e1:c8:9b:21:27:cf:77:83:98:10:
         af:54:09:9f:ce:6b:63:bd:f9:f8:00:97:3f:53:89:50:dc:6d:
         e9:45:42:18:46:91:3d:b5:60:45:9d:17:b1:69:71:a4:69:5e:
         18:65:cf:06:90:df:5b:db:60:57:ed:de:5c:ef:e8:05:d4:a7:
         10:c9:13:32:e3:b3:c3:70:fc:4e:2f:22:a4:40:e1:e3:8f:aa:
         fc:f6:d6:d6:1d:9a:54:32:9c:01:11:68:69:e7:2a:9f:ca:57:
         60:f8:6c:bd:ff:5c:d4:0a:e4:45:f7:6a:3c:40:cc:27:e3:d0:
         ae:9f:24:de:cc:5d:70:0e:b2:31:a5:62:65:6e:cd:0a:1c:41:
         f4:45:ec:bb:47:f8:2d:80:0c:6f:62:42:8e:12:90:5a:98:56:
         bc:b8:2f:0e:31:d4:f8:f3
*/
static char server_cert_data[] = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEpTCCAo2gAwIBAgIBAjANBgkqhkiG9w0BAQsFADBfMRcwFQYDVQQDDA5jYS5s\n"
"b2NhbGRvbWFpbjELMAkGA1UEBhMCRlIxDDAKBgNVBAgMA0JkUjEMMAoGA1UEBwwD\n"
"QWl4MQswCQYDVQQKDAJmRDEOMAwGA1UECwwFVGVzdHMwIBcNMTkxMTI0MTcwNjQz\n"
"WhgPMjExOTEwMzExNzA2NDNaME4xCzAJBgNVBAYTAkZSMQwwCgYDVQQIDANCZFIx\n"
"CzAJBgNVBAoMAmZEMQ4wDAYDVQQLDAVUZXN0czEUMBIGA1UEAwwLc2VydmVyLnRl\n"
"c3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCk1M8JfDUCODTugP2/\n"
"oNL5N8I/HD50rDjEU872wgMxuCyOsLX1WDRL7x3vtvBmHkhU8bfVUxOYgQCgYIT4\n"
"8r3R0HSV2OtBd8LsSFz6/gyrr3VHGPqWJ8sG2AGvBWfMEK5xNntr1/vJx+Nq4yDD\n"
"L55e72gspYDBd4XvwAvUAFcH6cT8sd+TZOsfNuOBdOeMkYo9CWDd9ZyTIUmcRW9v\n"
"OtM5XFxU7YzV2CjUWEbna7KTCWKKnMcj3H9xk4t+EWdzUj+Z6uyfDmvTCr5Iwslr\n"
"BaP01yBzg0Pzexca+3M2fiKgaLxj6gIPfAWSFYuy3ImpPawx1SlLDZW3LI5qYXv0\n"
"xc/PAgMBAAGjezB5MAkGA1UdEwQCMAAwLAYJYIZIAYb4QgENBB8WHU9wZW5TU0wg\n"
"R2VuZXJhdGVkIENlcnRpZmljYXRlMB0GA1UdDgQWBBR59qvWH68D4AKQ2OsO0tQP\n"
"AWBDMzAfBgNVHSMEGDAWgBRWJDDs9E6o2inrl0DGMu0Ph/X2ADANBgkqhkiG9w0B\n"
"AQsFAAOCAgEAyGkNU1IsX9mK6ZaYJ4+IUd9R0vtceiW7T4B6iun3FsDWfopMeIEr\n"
"Y4+/mPKR/KeggWTzySCrUvxoJAVZPq5He8Dhl3wlc2ugNL2QYWJRZx5sHi2M4b3W\n"
"prrZqYt6yHbQJYMoX5wd1atAzgyOloyVur0v/POBgBVcfV/SKL3qaD7fld9KHPP8\n"
"TfSHJfFLsn3zuhdKLb5tptb1sM/8LOxfcLUwPSzD3DO4yC6QfmIsUVG/kYCotveN\n"
"PWkD9AfKjIp18EsbROvRdv1h4X33NW4EO17JqfHZe+5DMYR0ZhhxtrbHmDpCpgOs\n"
"prdWjHalEzNxMQxDuCcXfMg8Dz0Whg4ThwBbr5zD4+c3/D4HVGNTr9aR3R3iJdhX\n"
"sLzbqCr4I+kVHgHz4zfkIYVINWbQbnKWygnXJpnVXfqyOZQV1ilF51zG6mGPTWpo\n"
"MpOxHbIoirBDtxfEgetO75HtytnY+rLhyJshJ893g5gQr1QJn85rY735+ACXP1OJ\n"
"UNxt6UVCGEaRPbVgRZ0XsWlxpGleGGXPBpDfW9tgV+3eXO/oBdSnEMkTMuOzw3D8\n"
"Ti8ipEDh44+q/PbW1h2aVDKcARFoaecqn8pXYPhsvf9c1ArkRfdqPEDMJ+PQrp8k\n"
"3sxdcA6yMaViZW7NChxB9EXsu0f4LYAMb2JCjhKQWphWvLgvDjHU+PM=\n"
"-----END CERTIFICATE-----\n"
				;
static char server_priv_data[] = ""
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEogIBAAKCAQEApNTPCXw1Ajg07oD9v6DS+TfCPxw+dKw4xFPO9sIDMbgsjrC1\n"
"9Vg0S+8d77bwZh5IVPG31VMTmIEAoGCE+PK90dB0ldjrQXfC7Ehc+v4Mq691Rxj6\n"
"lifLBtgBrwVnzBCucTZ7a9f7ycfjauMgwy+eXu9oLKWAwXeF78AL1ABXB+nE/LHf\n"
"k2TrHzbjgXTnjJGKPQlg3fWckyFJnEVvbzrTOVxcVO2M1dgo1FhG52uykwliipzH\n"
"I9x/cZOLfhFnc1I/mersnw5r0wq+SMLJawWj9Ncgc4ND83sXGvtzNn4ioGi8Y+oC\n"
"D3wFkhWLstyJqT2sMdUpSw2VtyyOamF79MXPzwIDAQABAoIBAC6uushD7jtnsc4O\n"
"qE8afEXq/c+j7yhaEmXAGrCWOBNfxvbOo2oOBhmvajoXBLTXRMMSBm+bbMRXXNcP\n"
"Hvnmvc3rjOlXmyFaitEimXrYcrw5ICz3rRjTwlN439ky3bfUzaLwvDnJ3NzECOuf\n"
"0gqVtPPMOCPU58djU4KYUIFFa2Co5v474RKBKNv18QuxGg6NWkPZhMAp1S0uAm+w\n"
"lwnpSXzS5X0qDC/EklZ9JLlGvvLZKreoWbk+sMv0x/jykwxI567hQq6hRoQ32YM+\n"
"+d/hBDSm69zf0om/XTFMzUSs5ss3TZyVujTTquTKktXzIT3gX+e7SMrt/fJi5ofr\n"
"39ijMRECgYEA2Ye77hHLEf57hf/tTK4gV4KUhatQTI5Rfa1WD1Z+jr99H/hiJw8f\n"
"dKeu8KUkU5REYFznu0fU/hSHXAxQf6Ae2NtM7uYbTEUgbuUKfG+8CT0UqWHkTtIB\n"
"egUc8ZD580VcM/WkK1A7if688COvwGYqrilq3kJYPwDmYsQBrJ/9uWcCgYEAwfs7\n"
"Kbt/g3Y7xaSyvLTTMgWrUGHSRV70vOPYe9qOEkU1HdMKhFQxbUfolfXEHq/gK/G5\n"
"KjxghUkN86T4h7xOXaiM1uYCioVmfIfV2ye95n9jdMvAz+W2z6chYLQs33R0AW7D\n"
"wI+yk7S0EeeAbygL5GtDLz1NNUYsAe4zA/g07VkCgYBznUC096AGoErmnW9yIdnu\n"
"qOhnYvX6uml1cnsbC82X8Q5/v3Prbo45YiYIoLz97v+od7hyQWti32ZR9fmS23eJ\n"
"qoTpNEOt9c+XIQTBvmEiR+SDYhQGEHfYcn8/pN4z/dlThGsM2kfKwCmLOGxgxexJ\n"
"RJoYJwrf8YqBU8vQA/jGYQKBgFzZ7sXZE/5PN+kxHsUpIPaOtCPzzvyZqThJL7SJ\n"
"NvwT2IsAG8afrlEK3I/7ZfbyZLFkMSfAYssp3t3DF5uRTUmThmbPDmRT2my9cGZI\n"
"raOJUofzh8V7xXe8HkP1uozzZxvQcy7XGbmOA6uWTmnml1qs5SnVhAF+J059QDok\n"
"MuFxAoGAQAwkelYrjyHFIxCX4/5/o36FsSKWRnvzGVhPlUZ6lPMVSsBYuTqfNDZv\n"
"K3UFwXsqiepM4Mso1TNEmoj5OhlJRkUi2WQPcvCzhuNZOBrtENxeLvjzMkIv3QfN\n"
"PtGTm8nE7MyIChUCfsGwh1yfhGfT9s1yeRUpYCjGkD2lC2/fmmI=\n"
"-----END RSA PRIVATE KEY-----\n"
				;

/* Expired:
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number: 3 (0x3)
    Signature Algorithm: sha256WithRSAEncryption
        Issuer: CN=ca.localdomain, C=FR, ST=BdR, L=Aix, O=fD, OU=Tests
        Validity
            Not Before: Nov 23 17:29:33 2019 GMT
            Not After : Nov 24 17:29:33 2019 GMT
        Subject: C=FR, ST=BdR, O=fD, OU=Tests, CN=expired.test
        Subject Public Key Info:
            Public Key Algorithm: rsaEncryption
                Public-Key: (2048 bit)
                Modulus:
                    00:e8:11:78:5c:1a:9f:60:46:dc:0a:f7:71:6c:39:
                    0c:b7:5d:b9:08:92:d8:a7:aa:b5:34:26:a6:07:18:
                    65:f9:77:16:31:f4:d9:fd:fe:88:3e:d1:10:9b:e3:
                    f0:15:cc:ea:4e:1f:33:7d:a0:06:5c:65:41:db:d0:
                    99:ce:99:4d:22:8d:bf:0d:aa:f1:30:f8:43:19:02:
                    9d:2d:14:90:db:08:55:92:f4:7e:4d:46:fa:3a:39:
                    43:c2:a6:6a:75:a7:52:f1:92:ba:2d:df:ba:ca:c8:
                    fb:53:e1:3b:fd:b6:6f:50:7a:bf:de:4b:24:13:52:
                    31:de:a0:1f:35:2e:02:12:bd:15:70:51:8a:62:61:
                    74:41:16:aa:22:41:ac:83:dd:a1:2b:bc:62:ab:e3:
                    6d:1c:53:73:2e:70:55:f5:f0:54:27:2d:b4:31:41:
                    90:b9:4b:3b:15:b8:16:dc:67:cb:4f:ea:2a:fe:69:
                    13:7f:22:df:a8:96:6d:eb:42:cb:ca:ad:8b:18:5f:
                    b4:a3:4e:96:80:c4:33:8b:7b:0c:11:86:7d:22:9f:
                    3e:ed:7d:ca:e8:e0:1a:fa:80:b0:8d:58:c9:ef:37:
                    5a:a1:d4:df:1f:ac:45:5b:66:7d:92:be:25:53:ef:
                    1b:e4:68:77:66:36:9c:87:c9:82:20:64:dc:f5:1a:
                    64:77
                Exponent: 65537 (0x10001)
        X509v3 extensions:
            X509v3 Basic Constraints: 
                CA:FALSE
            Netscape Comment: 
                OpenSSL Generated Certificate
            X509v3 Subject Key Identifier: 
                03:44:CB:35:55:E0:87:7B:6F:3F:A5:06:19:70:99:F2:21:B1:C2:48
            X509v3 Authority Key Identifier: 
                keyid:56:24:30:EC:F4:4E:A8:DA:29:EB:97:40:C6:32:ED:0F:87:F5:F6:00

    Signature Algorithm: sha256WithRSAEncryption
         9c:d1:af:3c:6c:41:1c:fe:dd:e2:60:b0:af:0b:f4:ea:64:e0:
         e3:13:54:ee:23:93:fb:61:88:a8:9d:9e:21:e8:6b:68:b0:3f:
         82:5b:95:22:ac:7b:06:d9:b4:49:b6:bd:4f:2e:4b:4a:cc:97:
         b3:c4:1b:31:fd:60:ae:ca:54:aa:1f:a1:0b:f0:84:66:5f:02:
         83:cf:bc:43:aa:5f:f6:fb:17:52:9d:f0:0e:2a:d7:f3:76:63:
         ca:59:ce:20:84:c5:c9:3a:bb:e7:8e:d8:04:95:4e:84:76:52:
         6d:fb:e1:fb:ae:23:64:a9:a1:34:c2:a2:8a:62:69:31:1d:88:
         ef:f1:e3:ba:d3:23:8f:e3:93:0d:a8:5e:51:ce:fa:af:2c:68:
         52:40:66:84:f0:4d:cc:1b:6f:dc:fc:c7:ee:85:18:4c:ec:96:
         31:4f:36:69:ba:05:ed:27:a2:bb:b1:51:de:e9:46:ea:22:67:
         8d:ee:85:60:19:f7:5b:0e:cb:4b:04:cc:ea:17:c9:59:dd:c0:
         59:03:9d:4a:94:fa:97:47:51:95:34:a3:1e:c2:f1:ff:af:d9:
         04:48:bd:6e:6c:01:ab:5f:76:77:a3:42:3d:24:9c:00:32:1f:
         f1:65:38:78:da:c8:5d:65:df:5f:9c:26:18:51:2e:56:33:73:
         7d:d3:dc:dc:3b:7a:d2:28:cb:58:92:13:bb:f8:a2:96:c0:64:
         27:c1:06:03:c3:41:28:3e:ee:c3:a6:aa:f2:20:1b:4b:78:6a:
         14:a7:eb:c5:60:ba:06:fb:1d:d5:68:c7:03:62:72:e2:dd:27:
         a8:5a:dc:e8:c2:12:ba:66:9a:72:ce:a8:a3:c2:dc:b4:20:13:
         65:68:c4:bc:40:06:2a:ed:a8:95:70:9e:be:7f:d8:59:8f:bc:
         14:6a:37:9e:84:c0:c2:fb:bd:18:5d:d1:2a:25:8d:10:e1:51:
         48:66:1e:8a:c4:0f:e5:0f:46:af:e3:5c:5e:a7:45:4b:3b:e3:
         1b:bd:f9:b7:75:05:b4:39:3b:38:69:00:04:86:e7:fa:6d:20:
         5d:e7:ff:39:a5:73:5c:9c:0b:50:1d:d2:76:20:b9:28:3b:a5:
         e1:24:e8:a0:85:94:39:15:8b:c8:d4:76:9e:79:82:46:56:4f:
         f4:3c:18:54:2a:77:82:9b:17:0c:4f:aa:e1:28:53:19:1c:1e:
         92:f9:8d:06:82:c5:5a:40:46:79:a4:68:54:82:d0:5f:33:1e:
         e1:83:dd:c2:57:ac:88:5a:31:71:b7:45:23:89:99:d6:8c:33:
         96:72:2f:6c:ee:46:31:24:ed:68:48:1d:05:b7:a2:d7:a8:5b:
         41:1b:1d:15:39:14:69:79
*/
static char expired_cert_data[]= ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEpDCCAoygAwIBAgIBAzANBgkqhkiG9w0BAQsFADBfMRcwFQYDVQQDDA5jYS5s\n"
"b2NhbGRvbWFpbjELMAkGA1UEBhMCRlIxDDAKBgNVBAgMA0JkUjEMMAoGA1UEBwwD\n"
"QWl4MQswCQYDVQQKDAJmRDEOMAwGA1UECwwFVGVzdHMwHhcNMTkxMTIzMTcyOTMz\n"
"WhcNMTkxMTI0MTcyOTMzWjBPMQswCQYDVQQGEwJGUjEMMAoGA1UECAwDQmRSMQsw\n"
"CQYDVQQKDAJmRDEOMAwGA1UECwwFVGVzdHMxFTATBgNVBAMMDGV4cGlyZWQudGVz\n"
"dDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAOgReFwan2BG3Ar3cWw5\n"
"DLdduQiS2KeqtTQmpgcYZfl3FjH02f3+iD7REJvj8BXM6k4fM32gBlxlQdvQmc6Z\n"
"TSKNvw2q8TD4QxkCnS0UkNsIVZL0fk1G+jo5Q8KmanWnUvGSui3fusrI+1PhO/22\n"
"b1B6v95LJBNSMd6gHzUuAhK9FXBRimJhdEEWqiJBrIPdoSu8YqvjbRxTcy5wVfXw\n"
"VCcttDFBkLlLOxW4Ftxny0/qKv5pE38i36iWbetCy8qtixhftKNOloDEM4t7DBGG\n"
"fSKfPu19yujgGvqAsI1Yye83WqHU3x+sRVtmfZK+JVPvG+Rod2Y2nIfJgiBk3PUa\n"
"ZHcCAwEAAaN7MHkwCQYDVR0TBAIwADAsBglghkgBhvhCAQ0EHxYdT3BlblNTTCBH\n"
"ZW5lcmF0ZWQgQ2VydGlmaWNhdGUwHQYDVR0OBBYEFANEyzVV4Id7bz+lBhlwmfIh\n"
"scJIMB8GA1UdIwQYMBaAFFYkMOz0TqjaKeuXQMYy7Q+H9fYAMA0GCSqGSIb3DQEB\n"
"CwUAA4ICAQCc0a88bEEc/t3iYLCvC/TqZODjE1TuI5P7YYionZ4h6GtosD+CW5Ui\n"
"rHsG2bRJtr1PLktKzJezxBsx/WCuylSqH6EL8IRmXwKDz7xDql/2+xdSnfAOKtfz\n"
"dmPKWc4ghMXJOrvnjtgElU6EdlJt++H7riNkqaE0wqKKYmkxHYjv8eO60yOP45MN\n"
"qF5RzvqvLGhSQGaE8E3MG2/c/MfuhRhM7JYxTzZpugXtJ6K7sVHe6UbqImeN7oVg\n"
"GfdbDstLBMzqF8lZ3cBZA51KlPqXR1GVNKMewvH/r9kESL1ubAGrX3Z3o0I9JJwA\n"
"Mh/xZTh42shdZd9fnCYYUS5WM3N909zcO3rSKMtYkhO7+KKWwGQnwQYDw0EoPu7D\n"
"pqryIBtLeGoUp+vFYLoG+x3VaMcDYnLi3SeoWtzowhK6Zppyzqijwty0IBNlaMS8\n"
"QAYq7aiVcJ6+f9hZj7wUajeehMDC+70YXdEqJY0Q4VFIZh6KxA/lD0av41xep0VL\n"
"O+Mbvfm3dQW0OTs4aQAEhuf6bSBd5/85pXNcnAtQHdJ2ILkoO6XhJOighZQ5FYvI\n"
"1HaeeYJGVk/0PBhUKneCmxcMT6rhKFMZHB6S+Y0GgsVaQEZ5pGhUgtBfMx7hg93C\n"
"V6yIWjFxt0UjiZnWjDOWci9s7kYxJO1oSB0Ft6LXqFtBGx0VORRpeQ==\n"
"-----END CERTIFICATE-----\n"

				;
static char expired_priv_data[]= ""
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEowIBAAKCAQEA6BF4XBqfYEbcCvdxbDkMt125CJLYp6q1NCamBxhl+XcWMfTZ\n"
"/f6IPtEQm+PwFczqTh8zfaAGXGVB29CZzplNIo2/DarxMPhDGQKdLRSQ2whVkvR+\n"
"TUb6OjlDwqZqdadS8ZK6Ld+6ysj7U+E7/bZvUHq/3kskE1Ix3qAfNS4CEr0VcFGK\n"
"YmF0QRaqIkGsg92hK7xiq+NtHFNzLnBV9fBUJy20MUGQuUs7FbgW3GfLT+oq/mkT\n"
"fyLfqJZt60LLyq2LGF+0o06WgMQzi3sMEYZ9Ip8+7X3K6OAa+oCwjVjJ7zdaodTf\n"
"H6xFW2Z9kr4lU+8b5Gh3Zjach8mCIGTc9RpkdwIDAQABAoIBADS9c6sK6dmJgQyE\n"
"+6PquzcY96o5JT/FjfTSK71FEDfHlqW3zarpo7ES9kFXZgKuVTl34c3VBl0NLhB0\n"
"sd+/+0W1DQxgIvxMD4OpkmriV6FPnZMOOX36eXet9/ZOt7cRVcpM3D78r4jScDu5\n"
"3lZklZumqeAtF3/EmEzN+wms8Q+stltBR3xo7pbvmllI41Igq0UAHmbAodKBhsXc\n"
"RorKLGsVsE4EeM5tTw1Ca5wmLZ2FvJC30eAkCzB5VkdAEiRfwooT+lwM9BjMJ8he\n"
"2cVGMRcYW+52t9WMUG8z4zbvHg3WsBwKHgc6AQ9A6al7bh/end6rowiqUKRgo1a0\n"
"3cKIznECgYEA98eWvy0JIUp6kb4FzfCC3moyIroPCloadgQfPCFDpC88zK4K6LQg\n"
"qdGpWWCyNpz/MsEr8moWo9UXZo9kxoMQXS+siR8MzEff2QMHH646ZOHmODvneTL6\n"
"pOWbcCKUofJDPXB+jk9Jbeqcq3eaTxE72girt2Fc28Z2WTqyQ03M2hsCgYEA78Rx\n"
"dNxNpncsdBeWglxr4/gbDYi5QMZ/GDIRXEMCkqiHsGmW/7LVSfWwU/kDsBFKtMA0\n"
"qZdNApSuhhgSP6Sae3bwoJE6iLSSzZ2eIpAOR6uBQA20UxS85cdEtRNx4HgTmoQc\n"
"fl+zPiKeAnd0sllxzzD/uFkMm3q6dx1Qnq7JhNUCgYA1FezlLqfrGwLhoL2rnqDb\n"
"OBNfv08jn6RVrpJmlpXgYkJ7c7pgmt74ozPOM1pbOw2t+RcKS9Jn7WqjDYvAwCnM\n"
"XT9w7VcXmTqLSwvGrNRT8tYZLDRukg6DL7I6tx3zCG2Bh02F/dS0mPRSBEDqOb4B\n"
"+6OWmWSEraODgIDazJtcCwKBgE7whU4tTh91cxxRu8r1tMvcnsOI9T0fXS7RJSgU\n"
"I9+3Pt1VFlLfRRvRmRk8jftE5iy2b2A6oS8tVnxtpmxvvDDUCwtCZVwm34J012CX\n"
"vyvXinlVSb5kwICCZ9uaKE74GbQwtNTimzfk29MCE1i43CCUCE1gfCcgdA3NiAKG\n"
"l3B1AoGBANpEOZLSVd/XfBfw8dwPdtC9wjMqHcKTOuaqaCXSiKKMn2vnaSRIs08U\n"
"rhj3Qzm6p33Hsxm3WibwcW7pBTC2vsa9DBv6ZJcXeB2qmmDxpIydVn64yd6WD3+M\n"
"bvEZZHKFcDcjAeoP6RJiFb+wppk3byqxjjSUr1JhKg1NtWriOSo7\n"
"-----END RSA PRIVATE KEY-----\n"
				;

/* Unknown CA certificate :
				Certificate:
				    Data:
        				Version: 3 (0x2)
        				Serial Number: 1 (0x1)
        				Signature Algorithm: sha1WithRSAEncryption
        				Issuer: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=chavroux.cowaddict.org/emailAddress=sdecugis@nict.go.jp
        				Validity
        				    Not Before: Oct 28 08:04:40 2009 GMT
        				    Not After : Oct 28 08:04:40 2010 GMT
        				Subject: C=JP, ST=Tokyo, L=Koganei, O=WIDE, OU=AAA WG, CN=unknown.cs/emailAddress=unknown@ca
        				Subject Public Key Info:
        				    Public Key Algorithm: rsaEncryption
        				    RSA Public Key: (1024 bit)
                				Modulus (1024 bit):
                				    00:e6:3a:d5:8a:14:c8:15:d0:f0:5c:03:c3:af:33:
                				    51:2c:17:b7:65:ac:45:e8:48:2d:ae:70:fd:7c:79:
                				    3a:c7:80:c8:50:53:d0:19:d8:3a:26:a8:16:4d:4c:
                				    04:17:09:df:69:9b:59:2b:89:c8:e0:60:bb:1d:37:
                				    82:d2:3f:17:39:c9:8f:5d:76:e1:0f:6e:08:9a:8f:
                				    16:4a:ea:83:86:f9:bd:15:14:56:68:87:79:05:f9:
                				    5f:66:11:bd:22:46:26:64:be:57:16:51:66:41:50:
                				    ac:f2:b1:ca:d0:38:11:4b:4c:b2:ee:25:36:6e:d3:
                				    b9:63:72:c4:84:82:1c:2b:27
                				Exponent: 65537 (0x10001)
        				X509v3 extensions:
        				    X509v3 Basic Constraints: 
                				CA:FALSE
        				    Netscape Comment: 
                				OpenSSL Generated Certificate
        				    X509v3 Subject Key Identifier: 
                				BA:5A:9D:D2:B0:4B:72:D6:1F:00:11:0B:B5:7B:59:DF:08:38:81:BE
        				    X509v3 Authority Key Identifier: 
                				keyid:52:C5:A4:63:B8:DB:AC:F2:92:34:2F:72:56:71:C8:11:8E:76:E6:DF

				    Signature Algorithm: sha1WithRSAEncryption
        				90:8f:3b:bd:e3:a1:ca:6a:92:a6:fd:f0:64:ae:46:83:32:35:
        				61:80:57:8b:30:12:70:02:e1:51:d9:87:c8:af:d9:4b:b9:6d:
        				bf:ab:86:5f:19:1f:dc:af:84:67:bf:3c:bf:33:f3:7c:c6:81:
        				7b:e4:e9:26:1d:bc:d6:8c:ab:72:94:7f:85:33:95:d9:24:ec:
        				fd:7b:d2:fd:50:3e:e5:61:4f:75:51:ae:c6:4a:ec:df:cf:aa:
        				73:a5:08:f7:f3:9a:40:66:48:f0:8e:9b:43:b1:30:f3:e3:c8:
        				36:3f:68:36:6a:1c:aa:16:40:49:b4:73:9a:71:f1:17:6c:0b:
        				d3:e1:a7:b7:40:de:2c:3c:36:7c:d4:dd:d6:94:c9:d7:5f:f5:
        				ae:35:56:e8:cc:65:9c:bb:3d:e8:7a:ca:0e:ed:78:03:41:cb:
        				fd:80:81:de:f9:de:b2:14:4b:81:24:36:de:29:c1:06:11:86:
        				8c:a9:b0:0c:c7:57:cf:79:a7:3a:84:0c:27:dc:86:6d:cb:44:
        				2d:26:dc:7e:fb:17:d6:b2:3d:31:03:d3:f1:ab:5d:91:5d:94:
        				e4:94:88:70:96:b3:7c:0f:15:fe:c8:c6:4d:99:37:ab:09:0c:
        				da:ba:b6:0e:fa:5e:bb:4b:ce:04:21:06:09:a9:2c:27:86:76:
        				cc:ee:73:6f
*/
static char notrust_ca_data[] =	"-----BEGIN CERTIFICATE-----\n"
				"MIIEqjCCA5KgAwIBAgIJAP3UMghSlH9PMA0GCSqGSIb3DQEBBQUAMIGUMQswCQYD\n"
				"VQQGEwJKUDEOMAwGA1UECAwFVG9reW8xEDAOBgNVBAcMB0tvZ2FuZWkxDTALBgNV\n"
				"BAoMBFdJREUxDzANBgNVBAsMBkFBQSBXRzEfMB0GA1UEAwwWY2hhdnJvdXguY293\n"
				"YWRkaWN0Lm9yZzEiMCAGCSqGSIb3DQEJARYTc2RlY3VnaXNAbmljdC5nby5qcDAe\n"
				"Fw0wOTEwMjgwODAzNDRaFw0xOTEwMjYwODAzNDRaMIGUMQswCQYDVQQGEwJKUDEO\n"
				"MAwGA1UECAwFVG9reW8xEDAOBgNVBAcMB0tvZ2FuZWkxDTALBgNVBAoMBFdJREUx\n"
				"DzANBgNVBAsMBkFBQSBXRzEfMB0GA1UEAwwWY2hhdnJvdXguY293YWRkaWN0Lm9y\n"
				"ZzEiMCAGCSqGSIb3DQEJARYTc2RlY3VnaXNAbmljdC5nby5qcDCCASIwDQYJKoZI\n"
				"hvcNAQEBBQADggEPADCCAQoCggEBALKW9iSUggF5mbvYe1Xk128Csfiijx+fwH5y\n"
				"ZqWrHNt0YG/tZSwyCDMWBLXTeuYsntg5y0mcpsrN8v02tvrPiCzDfRPyz3mG68us\n"
				"DPEEgQ1kqL2Gsti2DUcsdyZcDM+4rgsWRivgOTVyoNimv5f+xgmPYoElkgelLwZK\n"
				"WxGt1VCebOxP3qZA3hSHWE1hJgL4svful7RD1PbwPzidxJKITyAiJoPKWQA9cjSa\n"
				"gVzRQ7S4vmYALJn7xe+dMFRcfAK8RMv7/gJF6Rw7zufW0DIZK98KZs6aL0lmMPVk\n"
				"f31N2uvndf+cjy0n4luwEoXY+TeJZY205lbwHrzR0rH75FSm0RsCAwEAAaOB/DCB\n"
				"+TAdBgNVHQ4EFgQUUsWkY7jbrPKSNC9yVnHIEY525t8wgckGA1UdIwSBwTCBvoAU\n"
				"UsWkY7jbrPKSNC9yVnHIEY525t+hgZqkgZcwgZQxCzAJBgNVBAYTAkpQMQ4wDAYD\n"
				"VQQIDAVUb2t5bzEQMA4GA1UEBwwHS29nYW5laTENMAsGA1UECgwEV0lERTEPMA0G\n"
				"A1UECwwGQUFBIFdHMR8wHQYDVQQDDBZjaGF2cm91eC5jb3dhZGRpY3Qub3JnMSIw\n"
				"IAYJKoZIhvcNAQkBFhNzZGVjdWdpc0BuaWN0LmdvLmpwggkA/dQyCFKUf08wDAYD\n"
				"VR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOCAQEACANo6IR3OQlQaXHJaprVVDvl\n"
				"oMJC0FRbVCK503sbmWTJL98UqxRdsTZNIL07gXlK0oUKyiNijIXiLG8d5IlUrDxF\n"
				"H/Vsu6s8k3/PpAUVeiO2oygWqvU5NGvt0jg54MrOJKhYYPWrzbmHty+cAXyoNzOR\n"
				"+W5RX6HRQgxvZWQq2Ok46VX622R1nNjFmCBYT7I7/gWG+hkbIAoH6d9sULLjpC+B\n"
				"bI+L/N7ac9/Og8pGIgpUI60Gn5zO93+E+Nhg+1BlcDHGnQD6vFNs8LYp5CCX/Zj1\n"
				"tWFVXZnx58odaU3M4t9/ZQnkZdx9YJIroETbN0PoqlnSagBjgUvbWwn4YCotCA==\n"
				"-----END CERTIFICATE-----\n";
				
static char notrust_cert_data[]="-----BEGIN CERTIFICATE-----\n"
				"MIIDhjCCAm6gAwIBAgIBATANBgkqhkiG9w0BAQUFADCBlDELMAkGA1UEBhMCSlAx\n"
				"DjAMBgNVBAgMBVRva3lvMRAwDgYDVQQHDAdLb2dhbmVpMQ0wCwYDVQQKDARXSURF\n"
				"MQ8wDQYDVQQLDAZBQUEgV0cxHzAdBgNVBAMMFmNoYXZyb3V4LmNvd2FkZGljdC5v\n"
				"cmcxIjAgBgkqhkiG9w0BCQEWE3NkZWN1Z2lzQG5pY3QuZ28uanAwHhcNMDkxMDI4\n"
				"MDgwNDQwWhcNMTAxMDI4MDgwNDQwWjB/MQswCQYDVQQGEwJKUDEOMAwGA1UECAwF\n"
				"VG9reW8xEDAOBgNVBAcMB0tvZ2FuZWkxDTALBgNVBAoMBFdJREUxDzANBgNVBAsM\n"
				"BkFBQSBXRzETMBEGA1UEAwwKdW5rbm93bi5jczEZMBcGCSqGSIb3DQEJARYKdW5r\n"
				"bm93bkBjYTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA5jrVihTIFdDwXAPD\n"
				"rzNRLBe3ZaxF6EgtrnD9fHk6x4DIUFPQGdg6JqgWTUwEFwnfaZtZK4nI4GC7HTeC\n"
				"0j8XOcmPXXbhD24Imo8WSuqDhvm9FRRWaId5BflfZhG9IkYmZL5XFlFmQVCs8rHK\n"
				"0DgRS0yy7iU2btO5Y3LEhIIcKycCAwEAAaN7MHkwCQYDVR0TBAIwADAsBglghkgB\n"
				"hvhCAQ0EHxYdT3BlblNTTCBHZW5lcmF0ZWQgQ2VydGlmaWNhdGUwHQYDVR0OBBYE\n"
				"FLpandKwS3LWHwARC7V7Wd8IOIG+MB8GA1UdIwQYMBaAFFLFpGO426zykjQvclZx\n"
				"yBGOdubfMA0GCSqGSIb3DQEBBQUAA4IBAQCQjzu946HKapKm/fBkrkaDMjVhgFeL\n"
				"MBJwAuFR2YfIr9lLuW2/q4ZfGR/cr4Rnvzy/M/N8xoF75OkmHbzWjKtylH+FM5XZ\n"
				"JOz9e9L9UD7lYU91Ua7GSuzfz6pzpQj385pAZkjwjptDsTDz48g2P2g2ahyqFkBJ\n"
				"tHOacfEXbAvT4ae3QN4sPDZ81N3WlMnXX/WuNVbozGWcuz3oesoO7XgDQcv9gIHe\n"
				"+d6yFEuBJDbeKcEGEYaMqbAMx1fPeac6hAwn3IZty0QtJtx++xfWsj0xA9Pxq12R\n"
				"XZTklIhwlrN8DxX+yMZNmTerCQzaurYO+l67S84EIQYJqSwnhnbM7nNv\n"
				"-----END CERTIFICATE-----\n";
static char notrust_priv_data[]="-----BEGIN RSA PRIVATE KEY-----\n"
				"MIICXQIBAAKBgQDmOtWKFMgV0PBcA8OvM1EsF7dlrEXoSC2ucP18eTrHgMhQU9AZ\n"
				"2DomqBZNTAQXCd9pm1kricjgYLsdN4LSPxc5yY9dduEPbgiajxZK6oOG+b0VFFZo\n"
				"h3kF+V9mEb0iRiZkvlcWUWZBUKzyscrQOBFLTLLuJTZu07ljcsSEghwrJwIDAQAB\n"
				"AoGAeRec1SGVE5Rvt5XrSK0vFofq2DlCE6hTDpszWFLTDbe4pDdRDybhfw+Nm15O\n"
				"EGgK8BrbTcEMvKdkAzv9POQeLDE8JImgesHZFxN3jnkK+b762BGRDt57DzvMJsfj\n"
				"1LBle+UBnZB1CvjrINvu+tNMVPlUpjIstbpMq0D+s01+ijECQQD8MHTv/M+Uc86u\n"
				"1SFywgs+eQPQ8g0OoTLxzqo6YhW8FtwLjoRCZx2TNQS5gYBuQrixd/yE0Spfv9aS\n"
				"UtlAaOc1AkEA6bVufggHVHcgiWqS8CHzb6g/GRxQixVshOsoVLMkCSz04zlwIfXF\n"
				"c03hh5RJVv7jmuBmhHbayujMgvinw75oawJAQb9oXUDt5Wgj1FTgeYi5YbovEoRo\n"
				"fw3ruDsHCl2UCQt0ptarCJzVixFhf/ORRi3C9RGxFfdqMrhS+qb62N4AmQJBALYU\n"
				"T1BLiwJoiWXmLTJ/EP0V9Irov2uMtm5cE6DhrJqlduksz8r1gu7RZ3tMsVLg5Iy+\n"
				"dcCQJOffNa54caQUTZ8CQQDTs/70Nr6F6ktrtmtU/S7lIitpQJCu9u/SPyBYPmFZ\n"
				"9Axy6Ee66Php+eWDNP4Ln4axrapD0732wD8DcmGDVHij\n"
				"-----END RSA PRIVATE KEY-----\n";

/* Diffie-Hellman parameters, generated with GNUTLS certtool:
certtool --generate-dh-params
				Generator: 06

				Prime: ea:c3:75:0b:32:cf:d9:17:98:5c:da:d1
					e0:1d:b9:7c:be:29:60:b0:6f:68:a9:f6
					8d:75:05:59:69:04:ae:39:7c:2b:74:04
					3c:e2:da:28:8a:9b:93:aa:67:05:a7:3e
					06:3e:0d:31:63:88:55:ad:5a:bd:41:22
					b7:58:a7:45:b3:d5:03:ad:de:3c:8d:69
					42:bf:84:3d:c1:90:e7:39:6a:4b:87:01
					19:e5:f3:a4:e5:8e:e2:45:d5:0c:6b:17
					22:2b:2e:50:83:91:0c:5b:82:fc:72:27
					49:3b:9f:29:11:53:c7:90:b8:8d:87:73
					1a:7b:05:ab:cb:30:59:16:71:30:60:1b
					4c:80:15:3a:a2:d3:47:b7:4a:61:de:64
					7e:79:de:88:53:b7:7a:c6:a2:9a:bb:55
					40:2d:7a:71:c7:41:b5:29:df:d7:5c:fb
					42:e4:d8:5e:0b:99:d3:3c:93:0f:33:51
					8b:f4:60:e4:c5:b5:58:21:c0:51:c4:43
					25:7c:37:fe:5c:d3:62:6c:2a:af:a7:2a
					82:d5:d3:e2:bb:5d:ad:84:15:f6:78:d9
					d5:a8:f7:f0:48:5c:8d:e0:3d:04:ac:cf
					aa:34:3f:5d:f2:0d:3d:ee:ec:b8:d8:e8
					ad:dc:d3:40:59:a0:fd:45:62:47:63:c0
					bd:f5:df:8b
*/
static char dh_params_data[] =  "-----BEGIN DH PARAMETERS-----\n"
				"MIIBCAKCAQEA6sN1CzLP2ReYXNrR4B25fL4pYLBvaKn2jXUFWWkErjl8K3QEPOLa\n"
				"KIqbk6pnBac+Bj4NMWOIVa1avUEit1inRbPVA63ePI1pQr+EPcGQ5zlqS4cBGeXz\n"
				"pOWO4kXVDGsXIisuUIORDFuC/HInSTufKRFTx5C4jYdzGnsFq8swWRZxMGAbTIAV\n"
				"OqLTR7dKYd5kfnneiFO3esaimrtVQC16ccdBtSnf11z7QuTYXguZ0zyTDzNRi/Rg\n"
				"5MW1WCHAUcRDJXw3/lzTYmwqr6cqgtXT4rtdrYQV9njZ1aj38EhcjeA9BKzPqjQ/\n"
				"XfINPe7suNjordzTQFmg/UViR2PAvfXfiwIBBg==\n"
				"-----END DH PARAMETERS-----\n";


/* List server endpoints */
static struct fd_list eps = FD_LIST_INITIALIZER(eps);

/* Pass parameters to the connect thread */
struct connect_flags {
	int	proto;
	int	expect_failure; /* 0 or 1 */
};

/* Client's side of the connection established from a separate thread */
static void * connect_thr(void * arg)
{
	struct connect_flags * cf = arg;
	struct cnxctx * cnx = NULL;
	
	fd_log_threadname ( "testcnx:connect" );
	
	/* Connect to the server */
	switch (cf->proto) {
		case IPPROTO_TCP:
			{
				struct fd_endpoint * ep = (struct fd_endpoint *)(eps.next);
				cnx = fd_cnx_cli_connect_tcp( &ep->sa, sSAlen(&ep->ss) );
				CHECK( 1, (cnx ? 1 : 0) ^ cf->expect_failure );
			}
			break;
#ifndef DISABLE_SCTP
		case IPPROTO_SCTP:
			{
				cnx = fd_cnx_cli_connect_sctp(0, TEST_PORT, &eps, NULL);
				CHECK( 1, (cnx ? 1 : 0) ^ cf->expect_failure );
			}
			break;
#endif /* DISABLE_SCTP */
		default:
			CHECK( 0, 1 );
	}
	
	/* exit */
	return cnx;
}

/* Parameters to the handshake thread */
struct handshake_flags {
	struct cnxctx * cnx;
	gnutls_certificate_credentials_t	creds;
	int algo;
	int ret;
};

/* Handshake the client's side */
static void * handshake_thr(void * arg)
{
	struct handshake_flags * hf = arg;
	fd_log_threadname ( "testcnx:handshake" );
	hf->ret = fd_cnx_handshake(hf->cnx, GNUTLS_CLIENT, hf->algo, NULL, hf->creds);
	return NULL;
}

/* Terminate the client's connection side */
static void * destroy_thr(void * arg)
{
	struct cnxctx * cnx = arg;
	fd_log_threadname ( "testcnx:destroy" );
	fd_cnx_destroy(cnx);
	return NULL;
}
	
/* Main test routine */
int main(int argc, char *argv[])
{
	gnutls_datum_t ca 		= { (uint8_t *)ca_data, 		sizeof(ca_data) 	  };
	gnutls_datum_t server_cert 	= { (uint8_t *)server_cert_data, 	sizeof(server_cert_data)  };
	gnutls_datum_t server_priv 	= { (uint8_t *)server_priv_data, 	sizeof(server_priv_data)  };
	gnutls_datum_t client_cert	= { (uint8_t *)client_cert_data, 	sizeof(client_cert_data)  };
	gnutls_datum_t client_priv 	= { (uint8_t *)client_priv_data, 	sizeof(client_priv_data)  };
	gnutls_datum_t expired_cert 	= { (uint8_t *)expired_cert_data, 	sizeof(expired_cert_data) };
	gnutls_datum_t expired_priv 	= { (uint8_t *)expired_priv_data, 	sizeof(expired_priv_data) };
	gnutls_datum_t notrust_ca 	= { (uint8_t *)notrust_ca_data, 	sizeof(notrust_ca_data)   };
	gnutls_datum_t notrust_cert 	= { (uint8_t *)notrust_cert_data, 	sizeof(notrust_cert_data) };
	gnutls_datum_t notrust_priv 	= { (uint8_t *)notrust_priv_data, 	sizeof(notrust_priv_data) };
	gnutls_datum_t dh_params	= { (uint8_t *)dh_params_data, 		sizeof(dh_params_data) 	  };
	
	/* Listening socket, server side */
	struct cnxctx * listener;
#ifndef DISABLE_SCTP
	struct cnxctx * listener_sctp;
#endif /* DISABLE_SCTP */
	
	/* Server & client connected sockets */
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
	
	/* Restrain the # of streams */
	fd_g_config->cnf_sctp_str = NB_STREAMS;
	
	/* Set the CA parameter in the config */
	CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( fd_g_config->cnf_sec_data.credentials,
									 &ca,
									 GNUTLS_X509_FMT_PEM), );
	CHECK( 1, ret );
	
	#ifdef GNUTLS_VERSION_300
	{
		/* We import these CA in the trust list */
		gnutls_x509_crt_t * calist;
		unsigned int cacount = 0;
		
		CHECK_GNUTLS_DO( ret = gnutls_x509_crt_list_import2(&calist, &cacount, &ca, GNUTLS_X509_FMT_PEM, 
							GNUTLS_X509_CRT_LIST_FAIL_IF_UNSORTED), );
		CHECK( 1, cacount );

		CHECK_GNUTLS_DO( ret = gnutls_x509_trust_list_add_cas (fd_g_config->cnf_sec_data.trustlist, calist, cacount, 0), );
		CHECK( 1, ret );
	}
		
	/* Use certificate verification during the handshake */
	gnutls_certificate_set_verify_function (fd_g_config->cnf_sec_data.credentials, fd_tls_verify_credentials_2);
	
	#endif /* GNUTLS_VERSION_300 */
							
	
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
	CHECK_GNUTLS_DO( ret = gnutls_dh_params_import_pkcs3( fd_g_config->cnf_sec_data.dh_cache, &dh_params, GNUTLS_X509_FMT_PEM), );
	CHECK( GNUTLS_E_SUCCESS, ret );
	
	
	/* Initialize the server address (this should give a safe loopback address + port, even on non-standard configs) */
	{
		struct addrinfo hints, *ai, *aip;
		memset(&hints, 0, sizeof(hints));
		hints.ai_flags  = AI_NUMERICSERV;
		hints.ai_family = AF_INET;
		CHECK( 0, getaddrinfo("localhost", _stringize(TEST_PORT), &hints, &ai) );
		aip = ai;
		while (aip) {
			CHECK( 0, fd_ep_add_merge( &eps, aip->ai_addr, aip->ai_addrlen, EP_FL_DISC | EP_ACCEPTALL ));
			aip = aip->ai_next;
		};
		freeaddrinfo(ai);
		
		CHECK( 0, FD_IS_LIST_EMPTY(&eps) ? 1 : 0 );
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
		value.os.data = (uint8_t *)"Client.side";
		value.os.len = strlen((char *)value.os.data);
		CHECK( 0, fd_msg_avp_setvalue ( oh, &value ) );
		
		/* Add the AVP */
		CHECK( 0, fd_msg_avp_add( cer, MSG_BRW_LAST_CHILD, oh) );

		#if 0
		/* For debug: dump the object */
		fd_log_debug("Dumping CER");
		fd_log_debug("%s", fd_msg_dump_treeview(FD_DUMP_TEST_PARAMS, cer, fd_g_config->cnf_dict, 0, 1));
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

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
		free(rcv_buf);
		
		/* Do it in the other direction */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		free(rcv_buf);
		
		/* Now close the connections */
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

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
		free(rcv_buf);
		
		/* Do it in the other direction */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		free(rcv_buf);
		
		/* Do it one more time to use another stream */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		free(rcv_buf);
		
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

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
		free(rcv_buf);
		
		/* And the supposed reply */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		free(rcv_buf);
		
		/* At this point in legacy Diameter we start the handshake */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT /* No impact on TCP */, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 2 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
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
		hf.algo = ALGO_HANDSHAKE_3436; /* this is mandatory for old TLS mechanism */
		
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

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
		free(rcv_buf);
		
		/* And the supposed reply */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
		CHECK( cer_sz, rcv_sz );
		CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
		free(rcv_buf);
		
		/* At this point in legacy Diameter we start the handshake */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_3436, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 2 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 2 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
#ifndef DISABLE_SCTP
	
	
	/* SCTP Client / server emulating new Diameter behavior (DTLS handshake at connection directly) */
	TODO("Enabled after DTLS implementation");
	if (0)
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected messages, and replies */
		for (i = 0; i < 2 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* SCTP Client / server emulating old intermediary Diameter behavior (TLS handshake at connection directly) */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_SCTP;
		
		memset(&hf, 0, sizeof(hf));
		hf.algo = ALGO_HANDSHAKE_3436; /* this is mandatory for old TLS mechanism */
		
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_3436, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected messages, and replies */
		for (i = 0; i < 2 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
#endif /* DISABLE_SCTP */
	
	/* Test with different number of streams between server and client */
#ifndef DISABLE_SCTP
	/* DTLS / SCTP style */
	TODO("Enabled after DTLS implementation");
	if (0)
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
		
		/* Start the client thread with more streams than the server */
		fd_g_config->cnf_sctp_str = 2 * NB_STREAMS;
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 4 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Do the same test but with more streams on the server this time */
		fd_g_config->cnf_sctp_str = NB_STREAMS / 2;
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 2 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* TLS / SCTP style */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_SCTP;
		
		memset(&hf, 0, sizeof(hf));
		hf.algo = ALGO_HANDSHAKE_3436;
		
		/* Initialize remote certificate */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_allocate_credentials (&hf.creds), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		/* Set the CA */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &ca, GNUTLS_X509_FMT_PEM), );
		CHECK( 1, ret );
		/* Set the key */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &client_cert, &client_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread with more streams than the server */
		fd_g_config->cnf_sctp_str = 2 * NB_STREAMS;
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_3436, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 4 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Do the same test but with more streams on the server this time */
		fd_g_config->cnf_sctp_str = NB_STREAMS / 2;
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_3436, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Send a few TLS protected message, and replies */
		for (i = 0; i < 2 * NB_STREAMS; i++) {
			CHECK( 0, fd_cnx_send(server_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(client_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);

			CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
			CHECK( 0, fd_cnx_receive(server_side, NULL, &rcv_buf, &rcv_sz));
			CHECK( cer_sz, rcv_sz );
			CHECK( 0, memcmp( rcv_buf, cer_buf, cer_sz ) );
			free(rcv_buf);
		}
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
#endif /* DISABLE_SCTP */
	
	
	/* Basic operation tested successfully, now test we detect error conditions */

	/* Untrusted certificate, TCP */
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
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &notrust_ca, GNUTLS_X509_FMT_PEM), );
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &ca, GNUTLS_X509_FMT_PEM), );
		CHECK( 1, ret );
		/* Set the key */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &notrust_cert, &notrust_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( EINVAL, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		fd_cnx_destroy(server_side);
		
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* Same in SCTP */
#ifndef DISABLE_SCTP
	/* DTLS */
	TODO("Enabled after DTLS implementation");
	if (0)
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
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &notrust_ca, GNUTLS_X509_FMT_PEM), );
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &ca, GNUTLS_X509_FMT_PEM), );
		CHECK( 1, ret );
		/* Set the key */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &notrust_cert, &notrust_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( EINVAL, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* TLS */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_SCTP;
		
		memset(&hf, 0, sizeof(hf));
		hf.algo = ALGO_HANDSHAKE_3436;
		
		/* Initialize remote certificate */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_allocate_credentials (&hf.creds), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		/* Set the CA */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &notrust_ca, GNUTLS_X509_FMT_PEM), );
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_trust_mem( hf.creds, &ca, GNUTLS_X509_FMT_PEM), );
		CHECK( 1, ret );
		/* Set the key */
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &notrust_cert, &notrust_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( EINVAL, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_3436, NULL, NULL) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
#endif /* DISABLE_SCTP */
	
	/* Expired certificate */
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
		CHECK_GNUTLS_DO( ret = gnutls_certificate_set_x509_key_mem( hf.creds, &expired_cert, &expired_priv, GNUTLS_X509_FMT_PEM), );
		CHECK( GNUTLS_E_SUCCESS, ret );
		
		/* Start the client thread */
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake directly */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( EINVAL, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* Non matching hostname */
	
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Set the correct hostname we expect from the client (in the server) */
		fd_cnx_sethostname(server_side, "client.test");
		
		/* Start the handshake, check it is successful */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Do it again with an invalid hostname */
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Set the correct hostname we expect from the client (in the server) */
		fd_cnx_sethostname(server_side, "nomatch.test");
		
		/* Start the handshake, check it is successful */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( EINVAL, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		CHECK( 0, pthread_join(thr, NULL) );
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* Test the other functions of the module */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		char * str;
		const gnutls_datum_t *cert_list;
		unsigned int cert_list_size;
		struct fifo * myfifo = NULL;
		struct timespec now;
		int ev_code;
		
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Test some simple functions */
		
		/* fd_cnx_getid */
		str = fd_cnx_getid(server_side);
		CHECK( 1, str ? 1 : 0 );
		CHECK( 1, (str[0] != '\0') ? 1 : 0 );
		
		/* fd_cnx_getproto */
		i = fd_cnx_getproto(server_side);
		CHECK( IPPROTO_TCP, i);
		
		/* fd_cnx_getTLS */
		i = fd_cnx_getTLS(server_side);
		CHECK( 1, i ? 1 : 0 );
		
		/* fd_cnx_getcred */
		CHECK( 0, fd_cnx_getcred(server_side, &cert_list, &cert_list_size) );
		CHECK( 1, (cert_list_size > 0) ? 1 : 0 );
		/* We could also verify that the cert_list really contains the client_cert and ca certificates */
		
		/* fd_cnx_getremoteid */
		str = fd_cnx_getremoteid(server_side);
		CHECK( 1, str ? 1 : 0 );
		CHECK( 1, (str[0] != '\0') ? 1 : 0 );
		
		/* fd_cnx_recv_setaltfifo */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_fifo_new(&myfifo, 0) );
		CHECK( 0, fd_cnx_recv_setaltfifo(server_side, myfifo) );
		CHECK( 0, clock_gettime(CLOCK_REALTIME, &now) );
		do {
			CHECK( 0, fd_event_timedget(myfifo, &now, ETIMEDOUT, &ev_code, NULL, (void *)&rcv_buf) );
			free(rcv_buf);
		} while (ev_code != FDEVP_CNX_MSG_RECV);
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		fd_event_destroy(&myfifo, free);
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
#ifndef DISABLE_SCTP
	/* And re-test with a SCTP connection */
	TODO("Enabled after DTLS implementation");
	if (0)
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		char * str;
		const gnutls_datum_t *cert_list;
		unsigned int cert_list_size;
		struct fifo * myfifo = NULL;
		struct timespec now;
		int ev_code;
		
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_DEFAULT, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Test some simple functions */
		
		/* fd_cnx_getid */
		str = fd_cnx_getid(server_side);
		CHECK( 1, str ? 1 : 0 );
		CHECK( 1, (str[0] != '\0') ? 1 : 0 );
		
		/* fd_cnx_getproto */
		i = fd_cnx_getproto(server_side);
		CHECK( IPPROTO_SCTP, i);
		
		/* fd_cnx_getTLS */
		i = fd_cnx_getTLS(server_side);
		CHECK( 1, i ? 1 : 0 );
		
		/* fd_cnx_getcred */
		CHECK( 0, fd_cnx_getcred(server_side, &cert_list, &cert_list_size) );
		CHECK( 1, (cert_list_size > 0) ? 1 : 0 );
		/* We could also verify that the cert_list really contains the client_cert and ca certificates */
		
		/* fd_cnx_getremoteid */
		str = fd_cnx_getremoteid(server_side);
		CHECK( 1, str ? 1 : 0 );
		CHECK( 1, (str[0] != '\0') ? 1 : 0 );
		
		/* fd_cnx_recv_setaltfifo */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_fifo_new(&myfifo, 0) );
		CHECK( 0, fd_cnx_recv_setaltfifo(server_side, myfifo) );
		CHECK( 0, clock_gettime(CLOCK_REALTIME, &now) );
		do {
			CHECK( 0, fd_event_timedget(myfifo, &now, ETIMEDOUT, &ev_code, NULL, (void *)&rcv_buf) );
			free(rcv_buf);
		} while (ev_code != FDEVP_CNX_MSG_RECV);
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		fd_event_destroy(&myfifo, free);
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
	
	/* TLS */
	{
		struct connect_flags cf;
		struct handshake_flags hf;
		char * str;
		const gnutls_datum_t *cert_list;
		unsigned int cert_list_size;
		struct fifo * myfifo = NULL;
		struct timespec now;
		int ev_code;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_SCTP;
		
		memset(&hf, 0, sizeof(hf));
		hf.algo = ALGO_HANDSHAKE_3436;
		
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
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );

		/* Accept the connection of the client */
		server_side = fd_cnx_serv_accept(listener_sctp);
		CHECK( 1, server_side ? 1 : 0 );
		
		/* Retrieve the client connection object */
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 1, client_side ? 1 : 0 );
		hf.cnx = client_side;
		
		/* Start the handshake */
		CHECK( 0, pthread_create(&thr, NULL, handshake_thr, &hf) );
		CHECK( 0, fd_cnx_handshake(server_side, GNUTLS_SERVER, ALGO_HANDSHAKE_3436, NULL, NULL) );
		CHECK( 0, pthread_join(thr, NULL) );
		CHECK( 0, hf.ret );
		
		/* Test some simple functions */
		
		/* fd_cnx_getid */
		str = fd_cnx_getid(server_side);
		CHECK( 1, str ? 1 : 0 );
		CHECK( 1, (str[0] != '\0') ? 1 : 0 );
		
		/* fd_cnx_getproto */
		i = fd_cnx_getproto(server_side);
		CHECK( IPPROTO_SCTP, i);
		
		/* fd_cnx_getTLS */
		i = fd_cnx_getTLS(server_side);
		CHECK( 1, i ? 1 : 0 );
		
		/* fd_cnx_getcred */
		CHECK( 0, fd_cnx_getcred(server_side, &cert_list, &cert_list_size) );
		CHECK( 1, (cert_list_size > 0) ? 1 : 0 );
		/* We could also verify that the cert_list really contains the client_cert and ca certificates */
		
		/* fd_cnx_getremoteid */
		str = fd_cnx_getremoteid(server_side);
		CHECK( 1, str ? 1 : 0 );
		CHECK( 1, (str[0] != '\0') ? 1 : 0 );
		
		/* fd_cnx_recv_setaltfifo */
		CHECK( 0, fd_cnx_send(client_side, cer_buf, cer_sz));
		CHECK( 0, fd_fifo_new(&myfifo, 0) );
		CHECK( 0, fd_cnx_recv_setaltfifo(server_side, myfifo) );
		CHECK( 0, clock_gettime(CLOCK_REALTIME, &now) );
		do {
			CHECK( 0, fd_event_timedget(myfifo, &now, ETIMEDOUT, &ev_code, NULL, (void *)&rcv_buf) );
			free(rcv_buf);
		} while (ev_code != FDEVP_CNX_MSG_RECV);
		
		/* Now close the connection */
		CHECK( 0, pthread_create(&thr, NULL, destroy_thr, client_side) );
		fd_cnx_destroy(server_side);
		CHECK( 0, pthread_join(thr, NULL) );
		
		fd_event_destroy(&myfifo, free);
		
		/* Free the credentials */
		gnutls_certificate_free_keys(hf.creds);
		gnutls_certificate_free_cas(hf.creds);
		gnutls_certificate_free_credentials(hf.creds);
	}
#endif /* DISABLE_SCTP */
	

	/* Destroy the servers */
	{
		fd_cnx_destroy(listener);
#ifndef DISABLE_SCTP
		fd_cnx_destroy(listener_sctp);
#endif /* DISABLE_SCTP */
	}
	
	/* Check that connection attempt fails then */
	{
		struct connect_flags cf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_TCP;
		cf.expect_failure = 1;
		
		/* Start the client thread, that should fail */
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 0, client_side ? 1 : 0 );
	}
		
#ifndef DISABLE_SCTP
	{
		struct connect_flags cf;
		
		memset(&cf, 0, sizeof(cf));
		cf.proto = IPPROTO_SCTP;
		cf.expect_failure = 1;
		
		/* Start the client thread, that should fail */
		CHECK( 0, pthread_create(&thr, NULL, connect_thr, &cf) );
		CHECK( 0, pthread_join( thr, (void *)&client_side ) );
		CHECK( 0, client_side ? 1 : 0 );
	}
#endif /* DISABLE_SCTP */
	
	
	/* That's all for the tests yet */
	PASSTEST();
} 
	
