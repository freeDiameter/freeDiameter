import pprint

# Happy test (no expected errors)
def test_remove_all():
    return {
        'remove_avps' : [
            {'code': 263,
            'value': 'mme01.epc.mnc000.mcc738.3gppnetwork.org;1683674119;26;app_s6a',
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
            {'code': 1408, 'vendor': 10415},
            {'code': 1407, 'vendor': 10415},
            {'code': 260,
            'value': [{'code': 266, 'value': 10415, 'vendor': 0},
                        {'code': 258, 'value': 16777251, 'vendor': 0}],
            'vendor': 0},
            {'code': 260,
            'value': [],
            'vendor': 0},
            {'code': 282,
            'value': 'mme01.epc.mnc000.mcc738.3gppnetwork.org',
            'vendor': 0}
        ]
    }

def test_remove_no_value():
    return {
        'remove_avps' : [
            {'code': 263,
            'vendor': 0},
        ]
    }

def test_remove_nested_children_all():
    return {
        'remove_avps' : [
            {'code': 260,
            'value': [{'code': 266, 'value': 10415, 'vendor': 0},
                        {'code': 258, 'value': 16777251, 'vendor': 0}],
            'vendor': 0},
        ]
    }

def test_remove_nested_children_1():
    return {
        'remove_avps' : [
            {'code': 260,
            'value': [{'code': 266, 'value': 10415, 'vendor': 0},],
            'vendor': 0},
        ]
    }

def test_remove_parent_1():
    return {
        'remove_avps' : [
            {'code': 260,
            'value': [],
            'vendor': 0},
        ]
    }

def test_remove_parent_2():
    return {
        'remove_avps' : [
            {'code': 260,
            'vendor': 0},
        ]
    }


# Sad tests (expect errors)
def test_remove_unknown():
    return {
        'remove_avps' : [
            {'code': 69,
            'value': [],
            'vendor': 0},
        ]
    }

def test_remove_nested_unknown_known_children():
    return {
        'remove_avps' : [
            {'code': 260,
            'value': [{'code': 420, 'value': 10415, 'vendor': 0},   # Unknown
                    {'code': 258, 'value': 16777251, 'vendor': 0}], # Known
            'vendor': 0},
        ]
    }


# Happy
def test_re_add_known():
    return {
        'remove_avps' : [
            {'code': 263,
            'vendor': 0},
        ],
        'add_avps' : [
            {'code': 263,
            'value': 'BALLS.COM',
            'vendor': 0},
        ]
    }

def test_re_add_parent_children():
    return {
        'remove_avps' : [
            {'code': 260,
            'vendor': 0},
        ],
        'add_avps' : [
            {'code': 260,
            'value': [{'code': 266, 'value': 420, 'vendor': 0},
                      {'code': 258, 'value': 69, 'vendor': 0}],
            'vendor': 0},
        ]
    }

# Sad
def test_add_existing(): # No errors are printed for this, nothing happens though
    return {
        'add_avps' : [
            {'code': 263,
            'value': 'BALLS.COM',
            'vendor': 0},
        ]
    }

def test_add_existing_children(): # No errors are printed for this, nothing happens though
    return {
        'add_avps' : [
            {'code': 260,
            'value': [{'code': 266, 'value': 420, 'vendor': 0},
                      {'code': 258, 'value': 69, 'vendor': 0}],
            'vendor': 0},
        ]
    }

def test_add_children_to_non_grouped():
    return {
        'add_avps' : [
            {'code': 263,
            'value': [{'code': 266, 'value': 420, 'vendor': 0},
                      {'code': 258, 'value': 69, 'vendor': 0}],
            'vendor': 0},
        ]
    }

def test_add_unknown():
    return {
        'add_avps' : [
            {'code': 420,
            'value': 'BALLS.COM',
            'vendor': 10},
        ]
    }

def test_re_add_unknow_child():
    return {
        'remove_avps' : [
            {'code': 260,
            'vendor': 0},
        ],
        'add_avps' : [
            {'code': 260,
            'value': [{'code': 420, 'value': 420, 'vendor': 11},
                      {'code': 258, 'value': 69, 'vendor': 0}],
            'vendor': 0},
        ]
    }

def test_re_add_with_different_value_type(): # Results in an empty value
    return {
        'remove_avps' : [
            {'code': 263,
            'vendor': 0},
        ],
        'add_avps' : [
            {'code': 263,
            'value': 112243342,
            'vendor': 0},
        ]
    }

def test_re_add_parent_with_value_instead_of_children(): # Results in an avp with no children
    return {
        'remove_avps' : [
            {'code': 260,
            'vendor': 0},
        ],
        'add_avps' : [
            {'code': 260,
            'value': 'hello',
            'vendor': 0},
        ]
    }

def test_re_add_children_to_unknow_parent():
    return {
        'add_avps' : [
            {'code': 420,
            'value': [{'code': 420, 'value': 420, 'vendor': 11},
                      {'code': 258, 'value': 69, 'vendor': 0}],
            'vendor': 0},
        ]
    }

# Happy
def test_update_known():
    return {
        'update_avps' : [
            {'code': 263,
            'value': 'BALLS.COM',
            'vendor': 0},
        ],
    }


def test_update_children():
    return {
        'update_avps' : [
            {'code': 260,
            'value': [{'code': 266, 'value': 1234, 'vendor': 0},
                      {'code': 258, 'value': 5678, 'vendor': 0}],
            'vendor': 0},
        ]
    }

# Sad
def test_update_unknown():
    return {
        'update_avps' : [
            {'code': 420,
            'value': 'BALLS.COM',
            'vendor': 0},
        ],
    }

def test_update_childless_parent():
    return {
        'update_avps' : [
            {'code': 263, # Shouldnt have children
            'value': [{'code': 266, 'value': 1234, 'vendor': 0},
                      {'code': 258, 'value': 5678, 'vendor': 0}],
            'vendor': 0},
        ]
    }

def test_update_wrong_value_type():
    return {
        'update_avps' : [
            {'code': 263,
            'value': 100,
            'vendor': 0},
        ],
    }

def test_update_unknown_child():
    return {
        'update_avps' : [
            {'code': 260,
            'value': [{'code': 420, 'value': 1234, 'vendor': 0},
                      {'code': 258, 'value': 5678, 'vendor': 0}],
            'vendor': 0},
        ]
    }

def test_update_unsupported_avp():
    return {
        'update_avps' : [
            {'code': 1408,
            'value': 'hello',
            'vendor': 10415},
        ]
    }

# Happy
def test_update_peer_piority_positive():
    return {
        'update_peer_priorities' : {
            'hss01': 420,
            'pgw01.mnc000.mcc738.3gppnetwork.org': 69,
        }
    }

def test_update_peer_piority_negative():
    return {
        'update_peer_priorities' : {
            'hss01': -420,
            'pgw01.mnc000.mcc738.3gppnetwork.org': -69,
        }
    }

# Sad
def test_update_peer_piority_float():
    return {
        'update_peer_priorities' : {
            'hss01': 4.20,
            'pgw01.mnc000.mcc738.3gppnetwork.org': 6.9,
        }
    }

def test_update_peer_piority_string():
    return {
        'update_peer_priorities' : {
            'hss01': 'high',
            'pgw01.mnc000.mcc738.3gppnetwork.org': 'LOW',
        }
    }


def transform(msg_dict, peer_dict):
    print('[PYTHON]')
    pprint.pprint(msg_dict)
    pprint.pprint(peer_dict)

    res_dict = test_update_peer_piority_string()

    print('\n')
    pprint.pprint(res_dict)

    return res_dict
