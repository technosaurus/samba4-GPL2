#TLS and PKINIT crypto blobs
DHFILE=$TLSDIR/dhparms.pem
CAFILE=$TLSDIR/ca.pem
CERTFILE=$TLSDIR/cert.pem
REQKDC=$TLSDIR/req-kdc.der
KDCCERTFILE=$TLSDIR/kdc.pem
KEYFILE=$TLSDIR/key.pem
ADMINKEYFILE=$TLSDIR/adminkey.pem
REQADMIN=$TLSDIR/req-admin.der
ADMINKEYFILE=$TLSDIR/adminkey.pem
ADMINCERTFILE=$TLSDIR/admincert.pem

mkdir -p $TLSDIR 

#This is specified here to avoid draining entropy on every run
cat >$DHFILE<<EOF 
-----BEGIN DH PARAMETERS-----
MGYCYQC/eWD2xkb7uELmqLi+ygPMKyVcpHUo2yCluwnbPutEueuxrG/Cys8j8wLO
svCN/jYNyR2NszOmg7ZWcOC/4z/4pWDVPUZr8qrkhj5MRKJc52MncfaDglvEdJrv
YX70obsCAQI=
-----END DH PARAMETERS-----

EOF

#Likewise, we pregenerate the key material.  This allows the 
#other certificates to be pre-generated
cat >$KEYFILE<<EOF
-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQDKg6pAwCHUMA1DfHDmWhZfd+F0C+9Jxcqvpw9ii9En3E1uflpc
ol3+S9/6I/uaTmJHZre+DF3dTzb/UOZo0Zem8N+IzzkgoGkFafjXuT3BL5UPY2/H
6H+pPqVIRLOmrWImai359YyoKhFyo37Y6HPeU8QcZ+u2rS9geapIWfeuowIDAQAB
AoGAAqDLzFRR/BF1kpsiUfL4WFvTarCe9duhwj7ORc6fs785qAXuwUYAJ0Uvzmy6
HqoGv3t3RfmeHDmjcpPHsbOKnsOQn2MgmthidQlPBMWtQMff5zdoYNUFiPS0XQBq
szNW4PRjaA9KkLQVTwnzdXGkBSkn/nGxkaVu7OR3vJOBoo0CQQDO4upypesnbe6p
9/xqfZ2uim8IwV1fLlFClV7WlCaER8tsQF4lEi0XSzRdXGUD/dilpY88Nb+xok/X
8Z8OvgAXAkEA+pcLsx1gN7kxnARxv54jdzQjC31uesJgMKQXjJ0h75aUZwTNHmZQ
vPxi6u62YiObrN5oivkixwFNncT9MxTxVQJBAMaWUm2SjlLe10UX4Zdm1MEB6OsC
kVoX37CGKO7YbtBzCfTzJGt5Mwc1DSLA2cYnGJqIfSFShptALlwedot0HikCQAJu
jNKEKnbf+TdGY8Q0SKvTebOW2Aeg80YFkaTvsXCdyXrmdQcifw4WdO9KucJiDhSz
Y9hVapz7ykEJtFtWjLECQQDIlfc63I5ZpXfg4/nN4IJXUW6AmPVOYIA5215itgki
cSlMYli1H9MEXH0pQMGv5Qyd0OYIx2DDg96mZ+aFvqSG
-----END RSA PRIVATE KEY-----

EOF

cat >$ADMINKEYFILE<<EOF
-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQD0+OL7TQBj0RejbIH1+g5GeRaWaM9xF43uE5y7jUHEsi5owhZF
5iIoHZeeL6cpDF5y1BZRs0JlA1VqMry1jjKlzFYVEMMFxB6esnXhl0Jpip1JkUMM
XLOP1m/0dqayuHBWozj9f/cdyCJr0wJIX1Z8Pr+EjYRGPn/MF0xdl3JRlwIDAQAB
AoGAP8mjCP628Ebc2eACQzOWjgEvwYCPK4qPmYOf1zJkArzG2t5XAGJ5WGrENRuB
cm3XFh1lpmaADl982UdW3gul4gXUy6w4XjKK4vVfhyHj0kZ/LgaXUK9BAGhroJ2L
osIOUsaC6jdx9EwSRctwdlF3wWJ8NK0g28AkvIk+FlolW4ECQQD7w5ouCDnf58CN
u4nARx4xv5XJXekBvOomkCQAmuOsdOb6b9wn3mm2E3au9fueITjb3soMR31AF6O4
eAY126rXAkEA+RgHzybzZEP8jCuznMqoN2fq/Vrs6+W3M8/G9mzGEMgLLpaf2Jiz
I9tLZ0+OFk9tkRaoCHPfUOCrVWJZ7Y53QQJBAMhoA6rw0WDyUcyApD5yXg6rusf4
ASpo/tqDkqUIpoL464Qe1tjFqtBM3gSXuhs9xsz+o0bzATirmJ+WqxrkKTECQHt2
OLCpKqwAspU7N+w32kaUADoRLisCEdrhWklbwpQgwsIVsCaoEOpt0CLloJRYTANE
yoZeAErTALjyZYZEPcECQQDlUi0N8DFxQ/lOwWyR3Hailft+mPqoPCa8QHlQZnlG
+cfgNl57YHMTZFwgUVFRdJNpjH/WdZ5QxDcIVli0q+Ko
-----END RSA PRIVATE KEY-----

EOF

#generated with 
#hxtool issue-certificate --self-signed --issue-ca --ca-private-key=FILE:$KEYFILE \
#          --subject="CN=CA,$BASEDN" --certificate="FILE:$CAFILE"

cat >$CAFILE<<EOF
-----BEGIN CERTIFICATE-----
MIIChTCCAe6gAwIBAgIUFZoF6jt0R+hQBdF7cWPy0tT3fGwwCwYJKoZIhvcNAQEFMFIxEzAR
BgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxlMRUwEwYKCZImiZPy
LGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMCIYDzIwMDcwMTIzMDU1MzA5WhgPMjAwODAxMjQw
NTUzMDlaMFIxEzARBgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxl
MRUwEwYKCZImiZPyLGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMIGfMA0GCSqGSIb3DQEBAQUA
A4GNADCBiQKBgQDKg6pAwCHUMA1DfHDmWhZfd+F0C+9Jxcqvpw9ii9En3E1uflpcol3+S9/6
I/uaTmJHZre+DF3dTzb/UOZo0Zem8N+IzzkgoGkFafjXuT3BL5UPY2/H6H+pPqVIRLOmrWIm
ai359YyoKhFyo37Y6HPeU8QcZ+u2rS9geapIWfeuowIDAQABo1YwVDAOBgNVHQ8BAf8EBAMC
AqQwEgYDVR0lBAswCQYHKwYBBQIDBTAdBgNVHQ4EFgQUwtm596AMotmzRU7IVdgrUvozyjIw
DwYDVR0TBAgwBgEB/wIBADANBgkqhkiG9w0BAQUFAAOBgQBgzh5uLDmESGYv60iUdEfuk/T9
VCpzb1z3VJVWt3uJoQYbcpR00SKeyMdlfTTLzO6tSPMmlk4hwqfvLkPzGCSObR4DRRYa0BtY
2laBVlg9X59bGpMUvpFQfpvxjvFWNJDL+377ELCVpLNdoR23I9TKXlalj0bY5Ks46CVIrm6W
EA==
-----END CERTIFICATE-----

EOF

#generated with GNUTLS internally in Samba.  

cat >$CERTFILE<<EOF
-----BEGIN CERTIFICATE-----
MIICYTCCAcygAwIBAgIE5M7SRDALBgkqhkiG9w0BAQUwZTEdMBsGA1UEChMUU2Ft
YmEgQWRtaW5pc3RyYXRpb24xNDAyBgNVBAsTK1NhbWJhIC0gdGVtcG9yYXJ5IGF1
dG9nZW5lcmF0ZWQgY2VydGlmaWNhdGUxDjAMBgNVBAMTBVNhbWJhMB4XDTA2MDgw
NDA0MzY1MloXDTA4MDcwNDA0MzY1MlowZTEdMBsGA1UEChMUU2FtYmEgQWRtaW5p
c3RyYXRpb24xNDAyBgNVBAsTK1NhbWJhIC0gdGVtcG9yYXJ5IGF1dG9nZW5lcmF0
ZWQgY2VydGlmaWNhdGUxDjAMBgNVBAMTBVNhbWJhMIGcMAsGCSqGSIb3DQEBAQOB
jAAwgYgCgYDKg6pAwCHUMA1DfHDmWhZfd+F0C+9Jxcqvpw9ii9En3E1uflpcol3+
S9/6I/uaTmJHZre+DF3dTzb/UOZo0Zem8N+IzzkgoGkFafjXuT3BL5UPY2/H6H+p
PqVIRLOmrWImai359YyoKhFyo37Y6HPeU8QcZ+u2rS9geapIWfeuowIDAQABoyUw
IzAMBgNVHRMBAf8EAjAAMBMGA1UdJQQMMAoGCCsGAQUFBwMBMAsGCSqGSIb3DQEB
BQOBgQAmkN6XxvDnoMkGcWLCTwzxGfNNSVcYr7TtL2aJh285Xw9zaxcm/SAZBFyG
LYOChvh6hPU7joMdDwGfbiLrBnMag+BtGlmPLWwp/Kt1wNmrRhduyTQFhN3PP6fz
nBr9vVny2FewB2gHmelaPS//tXdxivSXKz3NFqqXLDJjq7P8wA==
-----END CERTIFICATE-----

EOF

#KDC certificate
# hxtool request-create --subject="CN=krbtgt,cn=users,$basedn" --key=FILE:$KEYFILE $KDCREQ

# hxtool issue-certificate --ca-certificate=FILE:$CAFILE,$KEYFILE --type="pkinit-kdc" --pk-init-principal="krbtgt/$RELAM@$REALM" --req="$KDCREQ" --certificate="FILE:$KDCCERTFILE"

cat >$KDCCERTFILE<<EOF
-----BEGIN CERTIFICATE-----
MIIDDDCCAnWgAwIBAgIUDEhjaOT1ZjHjHHEn+l5eYO05oK8wCwYJKoZIhvcNAQEFMFIxEzAR
BgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxlMRUwEwYKCZImiZPy
LGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMCIYDzIwMDcwMTIzMDcwNzA4WhgPMjAwODAxMjQw
NzA3MDhaMGYxEzARBgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxl
MRUwEwYKCZImiZPyLGQBGQwFc2FtYmExDjAMBgNVBAMMBXVzZXJzMQ8wDQYDVQQDDAZrcmJ0
Z3QwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMqDqkDAIdQwDUN8cOZaFl934XQL70nF
yq+nD2KL0SfcTW5+WlyiXf5L3/oj+5pOYkdmt74MXd1PNv9Q5mjRl6bw34jPOSCgaQVp+Ne5
PcEvlQ9jb8fof6k+pUhEs6atYiZqLfn1jKgqEXKjftjoc95TxBxn67atL2B5qkhZ966jAgMB
AAGjgcgwgcUwDgYDVR0PAQH/BAQDAgWgMBIGA1UdJQQLMAkGBysGAQUCAwUwVAYDVR0RBE0w
S6BJBgYrBgEFAgKgPzA9oBMbEVNBTUJBLkVYQU1QTEUuQ09NoSYwJKADAgEBoR0wGxsGa3Ji
dGd0GxFTQU1CQS5FWEFNUExFLkNPTTAfBgNVHSMEGDAWgBTC2bn3oAyi2bNFTshV2CtS+jPK
MjAdBgNVHQ4EFgQUwtm596AMotmzRU7IVdgrUvozyjIwCQYDVR0TBAIwADANBgkqhkiG9w0B
AQUFAAOBgQCMSgLkIv9RobE0a95H2ECA+5YABBwKXIt4AyN/HpV7iJdRx7B9PE6vM+nboVKY
E7i7ECUc3bu6NgrLu7CKHelNclHWWMiZzSUwhkXyvG/LE9qtr/onNu9NfLt1OV+dwQwyLdEP
n63FxSmsKg3dfi3ryQI/DIKeisvipwDtLqOn9g==
-----END CERTIFICATE-----

EOF

#hxtool request-create --subject="CN=Administrator,cn=users,$basedn" --key=FILE:$ADMINKEYFILE $ADMINREQFILE
#hxtool issue-certificate --ca-certificate=FILE:$CAFILE,$KEYFILE --type="pkinit-client" --pk-init-principal="administrator@$REALM" --req="$ADMINREQFILE" --certificate="FILE:$ADMINCERTFILE"

cat >$ADMINCERTFILE<<EOF
-----BEGIN CERTIFICATE-----
MIICwjCCAiugAwIBAgIUXyECoq4im33ByZDWZMGhtpvHYWEwCwYJKoZIhvcNAQEFMFIxEzAR
BgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxlMRUwEwYKCZImiZPy
LGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMCIYDzIwMDcwMTIzMDcyMzE2WhgPMjAwODAxMjQw
NzIzMTZaMCgxDjAMBgNVBAMMBXVzZXJzMRYwFAYDVQQDDA1BZG1pbmlzdHJhdG9yMIGfMA0G
CSqGSIb3DQEBAQUAA4GNADCBiQKBgQD0+OL7TQBj0RejbIH1+g5GeRaWaM9xF43uE5y7jUHE
si5owhZF5iIoHZeeL6cpDF5y1BZRs0JlA1VqMry1jjKlzFYVEMMFxB6esnXhl0Jpip1JkUMM
XLOP1m/0dqayuHBWozj9f/cdyCJr0wJIX1Z8Pr+EjYRGPn/MF0xdl3JRlwIDAQABo4G8MIG5
MA4GA1UdDwEB/wQEAwIFoDASBgNVHSUECzAJBgcrBgEFAgMEMEgGA1UdEQRBMD+gPQYGKwYB
BQICoDMwMaATGxFTQU1CQS5FWEFNUExFLkNPTaEaMBigAwIBAaERMA8bDWFkbWluaXN0cmF0
b3IwHwYDVR0jBBgwFoAUwtm596AMotmzRU7IVdgrUvozyjIwHQYDVR0OBBYEFCDzVsvJ8IDz
wLYH8EONeUa5oVrGMAkGA1UdEwQCMAAwDQYJKoZIhvcNAQEFBQADgYEAbTCnaPTieVZPV3bH
UmAMbnF9+YN1mCbe2xZJ0xzve+Yw1XO82iv/9kZaZkcRkaQt2qcwsBK/aSPOgfqGx+mJ7hXQ
AGWvAJhnWi25PawNaRysCN8WC6+nWKR4d2O2m5rpj3T9kH5WE7QbG0bCu92dGaS29FvWDCP3
q9pRtDOoAZc=
-----END CERTIFICATE-----

EOF