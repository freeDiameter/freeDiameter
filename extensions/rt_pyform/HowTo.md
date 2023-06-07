# rt_pyform extension

## Requirements

This extension will require you have python3-devel-3.10 installed.
If you have fedora this can be done with `dnf install python3-devel`

## Load the extension

Add the following line to your config file

```Conf
LoadExtension = "extensions/rt_pyform.fdx" : "<Your config filename>.conf";
```

## Making an rt_pyform config

The config file you use to initialise the extension needs to look like the following:

```Conf
DirectoryPath = "."        # Directory to search
ModuleName = "script"      # Name of python file. Note there is no .py extension
FunctionName = "transform" # Python function to call
```

Note the this config file should be no longer than 3 lines.

## Python function

The python function much have the correct number of parameters, must return a string, and must use the name specified in the config.
The following is an example of a function that prints out all the values it receives:

```Python
def transform(appId, flags, cmdCode, HBH_ID, E2E_ID, AVP_Code, vendorID, value):
    print('[PYTHON]')
    print(f'|-> appId: {appId}')
    print(f'|-> flags: {hex(flags)}')
    print(f'|-> cmdCode: {cmdCode}')
    print(f'|-> HBH_ID: {hex(HBH_ID)}')
    print(f'|-> E2E_ID: {hex(E2E_ID)}')
    print(f'|-> AVP_Code: {AVP_Code}')
    print(f'|-> vendorID: {vendorID}')
    print(f'|-> value: {value}')
    
    return value
```

Note the order of the arguments and that return is of the same type as the AVP value (string).

## Misc

This extension does not support AVP with non-string values (i.e. numbers).
This extension only supports mandatory AVP.