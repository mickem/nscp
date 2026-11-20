// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef WIN32

#include <string>

// The Windows calls behind running the service as something other than
// LocalSystem. What is decided - which account, which groups, in what order -
// lives in nscp/service_account.hpp and service/service_account_switch.hpp,
// where it can be tested without a Windows host.
namespace nsclient {
namespace windows_service_account {

// Add the account `member_sid` to the local group `group_sid`, or remove it.
// Both are string SIDs, so the group is found whatever it is called in the
// host's language.
//
// Already being a member when adding, and not being one when removing, are
// both success: the caller asks for a state, not a transition. A group that
// does not exist on this version of Windows is a failure, with `error` saying
// which one; the caller decides whether that matters.
bool set_group_membership(const std::string &member_sid, const std::string &group_sid, bool add, std::string &error);

// One line naming the account this process runs as and the privileges its
// token holds, for the log: `NT SERVICE\nscp (S-1-5-80-...), privileges:
// SeChangeNotifyPrivilege`. What a support request needs to show which mode an
// agent is in. Never throws; what cannot be read is said in the text.
std::string describe_process_identity();

}  // namespace windows_service_account
}  // namespace nsclient

#endif
