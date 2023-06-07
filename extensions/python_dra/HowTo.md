# python_dra extension

## Requirements

This extension will require you have python3-devel-3.10 installed.
If you have fedora this can be done with `dnf install python3-devel`

## Load the extension

Add the following line to your config file

```Conf
LoadExtension = "extensions/python_dra.fdx" : "<Your config filename>.conf";
```

## Making an python_dra config

The config file you use to initialise the extension needs to look like the following:

```Conf
DirectoryPath = "."        # Directory to search
ModuleName = "script"      # Name of python file. Note there is no .py extension
FunctionName = "transform" # Python function to call
```

Note the this config file should be no longer than 3 lines.

## Python function

The python function much have the correct number of parameters and must use the name specified in the config.
The following is an example of a function that prints out all the values it receives and returs a result dict (note this result dict is included as an example and is not expected to work without errors):

```Python
def transform(msg_dict, peer_dict=None):
    print('[PYTHON]')
    print(msg_dict)
    print(peer_dict)

    res_dict = {
        'remove_avps' : [
            # Remove 1407:10415 (Can remove unknown AVPs)
            {'code': 1407, 'vendor': 10415},
            # Remove only the children of 260:0
            {'code': 260,
            'value': [{'code': 266, 'value': 10415, 'vendor': 0},
                        {'code': 258, 'value': 16777251, 'vendor': 0}],
            'vendor': 0},
            # Remove 260:0 (and all its children)
            {'code': 260, 'vendor': 0},
        ],
        'update_avps' : [
            # Change value of 263:0 (AVP must be known to fd via dict)
            {'code': 263,
            'value': 'Hello, World!',
            'vendor': 0},
            # Change child value of 266:0 (AVP must be known to fd via dict)
            {'code': 260,
            'value': [{'code': 266, 'value': 420, 'vendor': 0}],
            'vendor': 0},
        ],
        'add_avps' : [
            # Add a new AVP (AVP must be known to fd via dict)
            {'code': 263,
            'value': 'I didnt exist before!',
            'vendor': 0},
            # Add a new child AVP (AVP must be known to fd via dict)
            {'code': 260,
            'value': [{'code': 263, 'value': 'Im a new child', 'vendor': 0}],
            'vendor': 0},
        ],
        'update_peer_priorities' : {
            'hss01': 420,
            'pgw01.mnc000.mcc738.3gppnetwork.org': -69,
        },
    }

    return res_dict
```

Note: Not all the fields need to be included in the results dict. E.g. returning a dict with only the remove_avps field is valid.
      A default value for the peer_dict is required as diameter answers will not have routing candidates

To process the result C performs the transformations in the following order: `remove_avps`, `update_avps`, `add_avps`, then `update_peer_priorities`.

## Misc

If printed using pprint the parameters will look something like the following:

```Python
# msg_dict
{'app_id': 16777251,
 'avp_list': [{'code': 263,
               'value': 'mme01.epc.mnc000.mcc738.3gppnetwork.org;1683761848;82;app_s6a',
               'vendor': 0},
              {'code': 277, 'value': 1, 'vendor': 0},
              {'code': 264,
               'value': 'mme01.epc.mnc000.mcc738.3gppnetwork.org',
               'vendor': 0},
              {'code': 296,
               'value': 'epc.mnc000.mcc738.3gppnetwork.org',
               'vendor': 0},
              {'code': 283,
               'value': 'epc.mnc000.mcc738.3gppnetwork.org',
               'vendor': 0},
              {'code': 1, 'value': '738000000000001', 'vendor': 0},
              {'code': 1401, 'vendor': 10415},
              {'code': 1032, 'vendor': 10415},
              {'code': 1405, 'vendor': 10415},
              {'code': 1407, 'vendor': 10415},
              {'code': 1615, 'vendor': 10415},
              {'code': 260,
               'value': [{'code': 266, 'value': 10415, 'vendor': 0},
                         {'code': 258, 'value': 16777251, 'vendor': 0}],
               'vendor': 0},
              {'code': 282,
               'value': 'mme01.epc.mnc000.mcc738.3gppnetwork.org',
               'vendor': 0}],
 'cmd_code': 316,
 'e2e_id': 2878173823,
 'flags': 192,
 'hbh_id': 1949902174}

# peer_dict
{'hss01': {'priority': 13,
           'realm': 'epc.mnc000.mcc738.3gppnetwork.org',
           'supported_applications': [10,
                                      16777216,
                                      16777217,
                                      16777236,
                                      16777238,
                                      16777251,
                                      16777252,
                                      16777291]},
 'pgw01.mnc000.mcc738.3gppnetwork.org': {'priority': -57,
                                         'realm': 'epc.mnc000.mcc738.3gppnetwork.org',
                                         'supported_applications': [4,
                                                                    16777238,
                                                                    16777272]}}
```

This extension alone will only supports mandatory AVP, to extend the supported AVPs you will probably have to utilise a dict_* extensions.

For some examples on the tests that were done to ensure no crashes occur see tests.py.