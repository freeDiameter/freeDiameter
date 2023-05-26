# dbg_peers extension

## Requirements

This requires that you have prometheus-c-library installed from https://github.com/Omnitouch/prometheus-client-c.

## Load the extension

Add the following line to your freediameter config file

```Conf
LoadExtension = "dbg_peers.fdx" : "<Your config filename>.conf";
```

## Making a dbg_peers config

The config file you use to initialise the extension needs to look like the following:

```Conf
UpdatePeriodSec = "1"
ExportMetricsPort = "8000"
```

Note the this config file should be no longer than 2 lines.