usage: sysevent [-h] -t TYPE [-j JSON] [-k KEY] [-v VALUE]

optional arguments:
  -h, --help            show this help message and exit
  -t TYPE, --type TYPE  sets EventType field
  -j JSON, --json JSON  a JSON dictionary to be sent as extra data fields
  -k KEY, --key KEY     an extra data field key, requires an equal number of
                        -v arguments
  -v VALUE, --value VALUE
                        an extra data field value, requires an equal number of
                        -k arguments

Sends a node event to the metadata-exporter, which re-exports it in the form
of a MONROE.META.NODE.EVENT:

# sysevent -t Test

results in:

MONROE.META.NODE.EVENT {"EventType":"Test","SequenceNumber":24888,
"Timestamp":1458287955,"DataVersion":1,"DataId":"MONROE.META.NODE.EVENT"}

