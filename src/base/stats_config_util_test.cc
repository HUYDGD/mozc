// Copyright 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <map>
#include <string>

#include "base/base.h"
#include "base/singleton.h"
#include "base/stats_config_util.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"
#ifdef OS_WINDOWS
#include "shared/opensource/patching/sidestep/cross/auto_testing_hook.h"
#endif

#ifdef OS_WINDOWS

// Users of the sidestep library need to supply these functions.
// See sidestep/cross/integration.h
// for more details.
// TODO(mazda): Make a library for these implementations.
namespace sidestep {
void AssertImpl(bool assertion_is_true, const char* message) {
  DCHECK(assertion_is_true) << message;
}

void LogImpl(const char* message) {
  DLOG(INFO) << message;
}
}  // sidestep

namespace {
const wchar_t kOmahaGUID[] = L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kOmahaUsageKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kOmahaUsageKeyForEveryone[] =
    L"Software\\Google\\Update\\ClientStateMedium\\"
    L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kSendStatsName[] = L"usagestats";

#define INT2HKEY(value) ((HKEY)(ULONG_PTR)((LONG)(value)))
const HKEY kHKCU_ClientState = INT2HKEY(1);
const HKEY kHKLM_ClientState = INT2HKEY(2);
const HKEY kHKLM_ClientStateMedium = INT2HKEY(3);
#undef INT2HKEY

const int kRunLevelLow = 0;
const int kRunLevelMedium = 1;
const int kRunLevelHigh = 2;

bool TryGetKnownKey(HKEY key, LPCWSTR sub_key, HKEY *result_key) {
  HKEY dummy = NULL;
  HKEY &result = (result_key != NULL ? *result_key : dummy);
  if (HKEY_CURRENT_USER == key) {
    if (wstring(kOmahaUsageKey) == sub_key) {
      result = kHKCU_ClientState;
      return true;
    }
  } else if (HKEY_LOCAL_MACHINE == key) {
    if (wstring(kOmahaUsageKey) == sub_key) {
      result = kHKLM_ClientState;
      return true;
    } else if (wstring(kOmahaUsageKeyForEveryone) == sub_key) {
      result = kHKLM_ClientStateMedium;
      return true;
    }
  }
  return false;
}

// Win32 registry emulator for unit testing.  To separate internal state,
// set unique id at the template parameter.
// This template class is mainly used for migration codes of http://b/2451942
// and http://b/2452672
template<int Id>
class RegistryEmulator {
 public:
  template<int Id>
  class PropertySelector {
   public:
    PropertySelector() : run_level_(kRunLevelMedium) {
    }
    bool contains_key_in_usagestats_map(HKEY key) const {
      return usagestats_map_.find(key) != usagestats_map_.end();
    }
    void clear_usagestats_map() {
      usagestats_map_.clear();
    }
    void erase_entry_from_usagestats_map(HKEY key) {
      usagestats_map_.erase(key);
    }
    void set_entry_to_usagestats_map(HKEY key, DWORD value) {
      usagestats_map_[key] = value;
    }
    DWORD get_entry_from_usagestats_map(HKEY key) const {
      map<HKEY, DWORD>::const_iterator i = usagestats_map_.find(key);
      if (i == usagestats_map_.end()) {
        return 0;
      }
      return i->second;
    }
    map<HKEY, DWORD> & usagestats_map() const {
      return usagestats_map_;
    }
    int run_level() const {
      return run_level_;
    }
    void set_run_level(int run_level) {
      run_level_ = run_level;
    }
   private:
    map<HKEY, DWORD> usagestats_map_;
    int              run_level_;
  };
  typedef PropertySelector<Id> Property;
  RegistryEmulator()
    : hook_reg_create_(
          sidestep::MakeTestingHook(RegCreateKeyExW, TestRegCreateKeyExW)),
      hook_reg_set_(
          sidestep::MakeTestingHook(RegSetValueExW, TestRegSetValueExW)),
      hook_reg_close_(
          sidestep::MakeTestingHook(RegCloseKey, TestRegCloseKey)),
      hook_reg_open_(
          sidestep::MakeTestingHook(RegOpenKeyExW, TestRegOpenKeyExW)),
      hook_reg_query_(
          sidestep::MakeTestingHook(RegQueryValueExW, TestRegQueryValueExW)),
      hook_reg_delete_value_(
          sidestep::MakeTestingHook(RegDeleteValueW, TestRegDeleteValueW)) {
  }
  static void SetRunLevel(int run_level) {
    mozc::Singleton<Property>::get()->set_run_level(run_level);
  }
  static bool HasUsagestatsValue(HKEY key) {
    if (!mozc::Singleton<Property>::get()->
        contains_key_in_usagestats_map(key)) {
      return false;
    }
    return true;
  }
  static bool GetUsagestatsValue(HKEY key, DWORD *value) {
    if (!HasUsagestatsValue(key)) {
      return false;
    }
    if (value != NULL) {
      *value =
          mozc::Singleton<Property>::get()->get_entry_from_usagestats_map(key);
    }
    return true;
  }
  static bool CheckWritable(HKEY key) {
    // Note that kHKLM_ClientStateMedium does not require admin rights.
    if (key == kHKLM_ClientState) {
      // Requires admin rights to update the value
      if (mozc::Singleton<Property>::get()->run_level() <
          kRunLevelHigh) {
        return false;
      }
    } else if (key == kHKLM_ClientStateMedium) {
      if (mozc::Singleton<Property>::get()->run_level() <
          kRunLevelMedium) {
        return false;
      }
    } else if (key == kHKCU_ClientState) {
      if (mozc::Singleton<Property>::get()->run_level() <
          kRunLevelMedium) {
        return false;
      }
    }
    return true;
  }
  static void SetUsagestatsValue(HKEY key, DWORD value) {
    mozc::Singleton<Property>::get()->set_entry_to_usagestats_map(key, value);
  }
  static void DeleteUsagestatsValue(HKEY key) {
    if (!HasUsagestatsValue(key)) {
      return;
    }
    mozc::Singleton<Property>::get()->erase_entry_from_usagestats_map(key);
  }
  static void ClearUsagestatsValue() {
    mozc::Singleton<Property>::get()->clear_usagestats_map();
  }
  static LSTATUS WINAPI TestRegCreateKeyExW(
      HKEY key, LPCWSTR sub_key, DWORD reserved, LPWSTR class_name,
      DWORD options, REGSAM sam, LPSECURITY_ATTRIBUTES security_attributes,
      PHKEY result, LPDWORD disposition) {
    HKEY dummy = NULL;
    HKEY &result_key = result != NULL ? *result : dummy;
    if (!TryGetKnownKey(key, sub_key, &result_key)) {
      return ERROR_ACCESS_DENIED;
    }
    if (!CheckWritable(result_key)) {
      return ERROR_ACCESS_DENIED;
    }
    return ERROR_SUCCESS;
  }
  static LSTATUS WINAPI TestRegSetValueExW(
      HKEY key, LPCWSTR value_name, DWORD reserved, DWORD type,
      const BYTE *data, DWORD num_data) {
    if (type != REG_DWORD || wstring(kSendStatsName) != value_name) {
      // Do nothing for other cases.
      return ERROR_SUCCESS;
    }
    if (!CheckWritable(key)) {
      return ERROR_ACCESS_DENIED;
    }
    SetUsagestatsValue(key, *reinterpret_cast<const DWORD*>(data));
    return ERROR_SUCCESS;
  }
  static LSTATUS WINAPI TestRegCloseKey(HKEY key) {
    return ERROR_SUCCESS;
  }
  static LSTATUS WINAPI TestRegOpenKeyExW(
      HKEY key, LPCWSTR sub_key, DWORD options, REGSAM sam, PHKEY result) {
    if (!TryGetKnownKey(key, sub_key, result)) {
      return ERROR_FILE_NOT_FOUND;
    }
    return ERROR_SUCCESS;
  }
  static LSTATUS WINAPI TestRegQueryValueExW(
      HKEY key, LPCWSTR value_name, LPDWORD reserved, LPDWORD type,
      LPBYTE data, LPDWORD num_data) {
    if (wstring(kSendStatsName) != value_name) {
      return ERROR_SUCCESS;
    }
    if (!HasUsagestatsValue(key)) {
      return ERROR_FILE_NOT_FOUND;
    }
    GetUsagestatsValue(key, reinterpret_cast<DWORD*>(data));
    if (type != NULL) { *type = REG_DWORD; }
    return ERROR_SUCCESS;
  }
  static LSTATUS WINAPI TestRegDeleteValueW(HKEY key, LPCWSTR value_name) {
    if (wstring(kSendStatsName) != value_name) {
      return ERROR_SUCCESS;
    }
    if (!HasUsagestatsValue(key)) {
      return ERROR_FILE_NOT_FOUND;
    }
    DeleteUsagestatsValue(key);
    return ERROR_SUCCESS;
  }
  sidestep::AutoTestingHook hook_reg_create_;
  sidestep::AutoTestingHook hook_reg_set_;
  sidestep::AutoTestingHook hook_reg_close_;
  sidestep::AutoTestingHook hook_reg_open_;
  sidestep::AutoTestingHook hook_reg_query_;
  sidestep::AutoTestingHook hook_reg_delete_value_;
};

class StatsConfigUtilTestWin : public testing::Test {
 protected:
  static void SetUpTestCase() {
    // A quick fix of b/2669319.  If mozc::Util::GetSystemDir is first called
    // when registry APIs are hooked by sidestep, GetSystemDir fails
    // unexpectedly because GetSystemDir also depends on registry API
    // internally.  The second call of mozc::Util::GetSystemDir works well
    // because it caches the result of the first call.  So any registry API
    // access occurs in the second call.  We call mozc::Util::GetSystemDir here
    // so that it works even when registry APIs are hooked.
    // TODO(yukawa): remove this quick fix as a part of b/2769852.
    mozc::Util::GetSystemDir();
  }
};
}  // namespace

namespace mozc {
#if defined(CHANNEL_DEV)
TEST_F(StatsConfigUtilTestWin, SetEnabledIgnoresRegistrySettings) {
  // In dev channel, settings in the registry are simply ignored and
  // StatsConfigUtil::IsEnabled always returns true.
  RegistryEmulator<__COUNTER__> test;
  test.SetRunLevel(kRunLevelHigh);

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (None, None)
  test.ClearUsagestatsValue();
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (None, Disabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 0);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (None, Enabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Disabled, None)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 0);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Disabled, Disabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 0);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 0);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Disabled, Enabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 0);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Enabled, None)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Enabled, Disabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 0);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Enabled, Enabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());
}

TEST_F(StatsConfigUtilTestWin, SetEnabledNeverFailsForRunLevelMedium) {
  // In dev channel, StatsConfigUtil::SetEnabled does not update the
  // the registry but always returns true.
  RegistryEmulator<__COUNTER__> test;
  test.SetRunLevel(kRunLevelMedium);
  EXPECT_TRUE(StatsConfigUtil::SetEnabled(true));
  EXPECT_TRUE(StatsConfigUtil::SetEnabled(false));
}

TEST_F(StatsConfigUtilTestWin, SetEnabledNeverFailsForRunLevelLow) {
  // In dev channel, StatsConfigUtil::SetEnabled does not update the
  // the registry but always returns true.
  RegistryEmulator<__COUNTER__> test;
  test.SetRunLevel(kRunLevelLow);
  EXPECT_TRUE(StatsConfigUtil::SetEnabled(true));
  EXPECT_TRUE(StatsConfigUtil::SetEnabled(false));
}
#endif  // CHANNEL_DEV

#if !defined(CHANNEL_DEV)
TEST_F(StatsConfigUtilTestWin, SetEnabledForRunLevelHigh) {
  // In beta and stable channel, StatsConfigUtil::SetEnabled requires
  // sufficient rights.
  RegistryEmulator<__COUNTER__> test;
  test.SetRunLevel(kRunLevelHigh);

  // Check if SetEnabled(true) works as expected.
  EXPECT_TRUE(StatsConfigUtil::SetEnabled(true));
  EXPECT_FALSE(test.HasUsagestatsValue(kHKCU_ClientState));
  EXPECT_TRUE(test.HasUsagestatsValue(kHKLM_ClientState));
  DWORD value = 0;
  test.GetUsagestatsValue(kHKLM_ClientState, &value);
  EXPECT_EQ(1, value);
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientStateMedium));

  // Check if SetEnabled(false) works as expected.
  EXPECT_TRUE(StatsConfigUtil::SetEnabled(false));
  EXPECT_TRUE(test.HasUsagestatsValue(kHKLM_ClientState));
  test.GetUsagestatsValue(kHKLM_ClientState, &value);
  EXPECT_EQ(0, value);
}

TEST_F(StatsConfigUtilTestWin, SetEnabledForRunLevelMedium) {
  // In beta and stable channels, StatsConfigUtil::SetEnabled requires
  // sufficient rights.
  RegistryEmulator<__COUNTER__> test;

  test.SetRunLevel(kRunLevelMedium);
  // Check if SetEnabled(true) fails as expected.
  EXPECT_FALSE(StatsConfigUtil::SetEnabled(true));
  EXPECT_FALSE(test.HasUsagestatsValue(kHKCU_ClientState));
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientState));
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientStateMedium));
}

TEST_F(StatsConfigUtilTestWin, SetEnabledForRunLevelLow) {
  // In beta and stable channels, StatsConfigUtil::SetEnabled requires
  // sufficient rights.
  RegistryEmulator<__COUNTER__> test;

  test.SetRunLevel(kRunLevelLow);
  // Check if SetEnabled(true) fails as expected.
  EXPECT_FALSE(StatsConfigUtil::SetEnabled(true));
  EXPECT_FALSE(test.HasUsagestatsValue(kHKCU_ClientState));
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientState));
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientStateMedium));
}

TEST_F(StatsConfigUtilTestWin, IsEnabled) {
  RegistryEmulator<__COUNTER__> test;
  test.SetRunLevel(kRunLevelHigh);

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (None, None)
  test.ClearUsagestatsValue();
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (None, Disabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 0);
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (None, Enabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Disabled, None)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 0);
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Disabled, Disabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 0);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 0);
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Disabled, Enabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 0);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Enabled, None)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Enabled, Disabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 0);
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());

  // (kHKLM_ClientState, kHKLM_ClientStateMedium) == (Enabled, Enabled)
  test.ClearUsagestatsValue();
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  test.SetUsagestatsValue(kHKLM_ClientStateMedium, 1);
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());
}
#endif  // !CHANNEL_DEV

TEST_F(StatsConfigUtilTestWin, RemoveDisablingHKCUEntryInIsEnabled) {
  RegistryEmulator<__COUNTER__> test;

  test.SetRunLevel(kRunLevelHigh);
  // Enabling usagestats with proper style.
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  test.SetRunLevel(kRunLevelMedium);

  // Disable usagestats with wrong style.
  test.SetUsagestatsValue(kHKCU_ClientState, 0);

#if defined(CHANNEL_DEV)
  // In dev channel, IsEnabled always returns true regardless of the registry
  // settings.
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());
#else
  // In beta and stable channels, disabling entry with wrong style should be
  // honored even if it is enabled in |kHKLM_ClientState|
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());
#endif

  // IsEnabled has silently removed the wrong tyle entry.
  EXPECT_FALSE(test.HasUsagestatsValue(kHKCU_ClientState));

  // Disabling entry has been migrated to more appropriate place in IsEnabled.
  EXPECT_TRUE(test.HasUsagestatsValue(kHKLM_ClientStateMedium));
  DWORD value = 0;
  test.GetUsagestatsValue(kHKLM_ClientStateMedium, &value);
  EXPECT_EQ(0, value);
}

TEST_F(StatsConfigUtilTestWin, IsEnabledForRunLevelLow) {
  RegistryEmulator<__COUNTER__> test;

  test.SetRunLevel(kRunLevelHigh);
  // Enabling usagestats with proper style.
  test.SetUsagestatsValue(kHKLM_ClientState, 1);
  // Disable usagestats with wrong style.
  test.SetUsagestatsValue(kHKCU_ClientState, 0);

  test.SetRunLevel(kRunLevelLow);

#if defined(CHANNEL_DEV)
  // In dev channel, IsEnabled always returns true regardless of the registry
  // settings.
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());
#else
  // In beta and stable channels, disabling entry with wrong style should be
  // honored even if it is enabled in |kHKLM_ClientState|
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());
#endif

  // If the runlevel is low, we cannot remove the entry under HKCU.
  // The previous states should remain as it were.
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientStateMedium));
  EXPECT_TRUE(test.HasUsagestatsValue(kHKCU_ClientState));
  DWORD value = 0;
  test.GetUsagestatsValue(kHKCU_ClientState, &value);
  EXPECT_EQ(0, value);
}

TEST_F(StatsConfigUtilTestWin, RemoveEnablingHKCUEntryInIsEnabled) {
  RegistryEmulator<__COUNTER__> test;
  test.SetRunLevel(kRunLevelMedium);
  // Enabling usagestats with wrong style (should be fixed).
  test.SetUsagestatsValue(kHKCU_ClientState, 1);

#if defined(CHANNEL_DEV)
  // In dev channel, IsEnabled always returns true regardless of the registry
  // settings.
  EXPECT_TRUE(StatsConfigUtil::IsEnabled());
#else
  // In beta and stable channels, enabling entry with wrong style will be
  // ignored.
  EXPECT_FALSE(StatsConfigUtil::IsEnabled());
#endif

  // IsEnabled has silently removed the wrong tyle entry.
  EXPECT_FALSE(test.HasUsagestatsValue(kHKCU_ClientState));

  // Enabling entry has not been migrated in IsEnabled.
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientStateMedium));
}

TEST_F(StatsConfigUtilTestWin,
       RemoveEnablingHKCUEntryInIsEnabledForRunLevelLow) {
  RegistryEmulator<__COUNTER__> test;
  test.SetRunLevel(kRunLevelMedium);
  // Enabling usagestats with wrong style (should be fixed).
  test.SetUsagestatsValue(kHKCU_ClientState, 1);
  test.SetRunLevel(kRunLevelLow);

  // If the runlevel is low, we cannot remove the entry under HKCU.
  // The previous states should remain as it were.
  EXPECT_FALSE(test.HasUsagestatsValue(kHKLM_ClientStateMedium));
  EXPECT_TRUE(test.HasUsagestatsValue(kHKCU_ClientState));
  DWORD value = 1;
  test.GetUsagestatsValue(kHKCU_ClientState, &value);
  EXPECT_EQ(1, value);
}
}  // namespace mozc
#endif  // OS_WINDOWS

