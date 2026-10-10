"""Mailbox-level profile-switch contract used by hardware scripts/tests."""

MAILBOX_COMMAND_SWITCH_PROFILE = 7
COMMAND_RESULT_OK = 0
COMMAND_RESULT_ACCEPTED = 1


def request_profile_command(profile, token, arg1=0, arg64=0, flags=0):
    if profile not in (0, 1):
        raise ValueError('profile must be 0 or 1')
    return dict(token=token, command=MAILBOX_COMMAND_SWITCH_PROFILE,
                arg0=profile, arg1=arg1, arg64=arg64, flags=flags)


def classify_response(response, requested_profile, current_profile):
    result = response['result']
    if requested_profile == current_profile:
        return result == COMMAND_RESULT_OK
    return result == COMMAND_RESULT_ACCEPTED


def self_test():
    assert request_profile_command(1, 10)['command'] == 7
    assert classify_response({'result': 1}, 1, 0)
    assert not classify_response({'result': 0}, 1, 0)
    assert classify_response({'result': 0}, 0, 0)
    assert not classify_response({'result': 1}, 0, 0)


if __name__ == '__main__':
    self_test()
    print('profile mailbox contract: PASS')
