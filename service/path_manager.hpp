// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <nscp/path_defaults.hpp>
#include <nscp/path_rooting.hpp>
#include <string>

namespace nsclient {
namespace core {

// A ${token} named something this installation cannot resolve.
//
// This used to be silent: an unrecognised key resolved to the executable's
// directory, so `${scripst}/x.bat` became a real path under the install folder
// and whatever depended on it quietly went to the wrong place (#458). A typo in
// a path is an operator error and is now reported as one.
//
// Derives from std::exception on purpose. The two layers that expand
// operator-supplied paths already catch it and report per item -
// settings_registry::notify() names the key it was configuring, and
// init_settings() reports a failed settings load - so raising this surfaces the
// bad token with context instead of terminating anything.
//
// Defined with the rooting helper rather than here, because modules raise the
// same error on their own side of the plugin ABI and cannot include this
// header. Aliased for the core's own callers.
using path_expansion_error = nscp::paths::path_expansion_error;

class path_manager {
  typedef std::map<std::string, std::string> paths_type;

  logging::log_client_accessor log_instance_;
  boost::timed_mutex mutex_;
  boost::filesystem::path basePath;
  boost::filesystem::path tempPath;
  // Path overrides loaded from boot.ini's [paths] section. Populated once
  // during NSCSettingsImpl::boot() before any reader exists, then treated as
  // immutable - no mutex needed on reads. If we ever need to support runtime
  // reload, add synchronisation at that point.
  paths_type overrides_;

  // Highest-precedence overrides from the CLI --path-override flag. Kept in a
  // separate map (rather than merged into overrides_) so precedence is
  // application-order-independent: CLI always beats boot.ini, which always
  // beats the compile-time defaults. CLI overrides are applied before
  // init_settings() so they can even relocate ${boot-conf} itself.
  paths_type cli_overrides_;

  // Which on-disk layout this installation uses. Set once from boot.ini during
  // the settings bootstrap, before anything resolves a path; legacy until then,
  // and legacy forever on unix, where the layout comes from the package prefix.
  nscp::paths::layout layout_ = nscp::paths::layout::legacy;

 public:
  explicit path_manager(const logging::log_client_accessor& log_instance_);
  std::string getFolder(const std::string& key);
  std::string expand_path(std::string file);

  // expand_path, then root the answer at `default_root` when what came back
  // does not name a location of its own.
  //
  // expand_path substitutes tokens; it does not make anything absolute, and it
  // must not - an operator is entitled to point a setting anywhere on the
  // filesystem, and `/var/log/mine.log` is a perfectly good answer. But a value
  // with no token and no root is only meaningful relative to *something*, and
  // the thing it has been relative to until now is the process working
  // directory: C:\Windows\System32 for a Windows service, "/" under a bare init
  // script, the package directory under the shipped systemd unit. That is not a
  // base an operator can predict, and on the write side it means a file the
  // agent creates lands somewhere nobody looks.
  //
  // So the consumer names the root it owns - `${shared-path}` for an
  // attachment, `${log-path}` for the log - and a bare name resolves there
  // instead. Only consumers that genuinely own a namespace should call this; a
  // script *name* is resolved by its provider's search list, not by this.
  //
  // `default_root` is a compile-time literal at every call site, so a root that
  // does not itself resolve to an absolute location is a programming error and
  // is raised as one rather than quietly ignored.
  std::string resolve_path(std::string file, const std::string& default_root);

  // Install the path-override map. Intended to be called exactly once from
  // the settings bootstrap, before any other code resolves paths through
  // this manager. Subsequent calls replace the previous overrides.
  void set_overrides(paths_type overrides);

  // Merge additional overrides on top of whatever set_overrides previously
  // installed. Same-key entries overwrite, missing keys are preserved.
  void add_overrides(paths_type overrides);

  // Install the highest-precedence CLI override layer. Checked ahead of both
  // the boot.ini overrides and the compile-time defaults in getFolder(), so
  // these win no matter when boot.ini's [paths] are applied. Intended to be
  // called once, before init_settings(), from the CLI parser plumbing.
  //
  // Anything already definitively unusable is dropped here rather than in
  // validate_overrides(), because this layer is in force for the whole of
  // init_settings() - which opens boot.ini, creates the shared folder and
  // writes the trust store. Only what cannot be judged until boot.ini has been
  // read waits for that later pass.
  void set_cli_overrides(paths_type overrides);

  // Check every installed override and discard the ones that do not name an
  // absolute location, reporting each. Call once after the settings bootstrap
  // has applied boot.ini's [layout] and [paths]: the CLI layer is installed
  // before that point and cannot be judged until the rest of the picture
  // exists. Idempotent.
  void validate_overrides();

  // Select the on-disk layout (Windows only in practice; a no-op elsewhere
  // because the unix defaults are absolute). Called from the settings
  // bootstrap once boot.ini has been read, before the main settings store is
  // opened and before anything resolves a path - the whole point is that
  // ${shared-path} answers consistently from the first lookup onwards.
  void set_layout(nscp::paths::layout value);
  nscp::paths::layout get_layout() const { return layout_; }

  // Maximum recursion depth for ${var} substitution. Caps the cycle defence
  // ("${a}" -> "${b}" -> "${a}") so a misconfiguration cannot stack-overflow
  // the service. 32 is comfortably more than any sane chain - real templates
  // nest 2-4 levels at most.
  static constexpr int kMaxExpandDepth = 32;

 private:
  // getFolder() with the caller's expansion depth threaded through, so a key
  // whose value is itself a lookup (${nrpe-dh}, which has to expand its
  // candidates before it can stat them) is covered by the same cycle guard as
  // ordinary substitution instead of starting a fresh, unbounded chain.
  std::string resolve_folder(const std::string& key, int depth);
  std::string get_path_for_key(const std::string& key, int depth);

  // Why `value` cannot serve as a path override, or "" when it can. The one
  // predicate both passes ask, so that the early CLI judgement and the full
  // sweep cannot drift apart - which they had: the boot-conf branch that used
  // to carry its own copy never rejected a value that expands to nothing.
  //
  // `unresolved` comes back true when the value named a token that does not
  // resolve *yet*. That is a rejection for a key the bootstrap consumes before
  // boot.ini has been read, and "ask again later" for every other one, because
  // an override is explicitly allowed to be written in terms of a [paths]
  // entry boot.ini has not been read for yet.
  std::string why_unusable(const std::string& value, bool& unresolved);

  // Discard overrides that do not name an absolute location, reporting each
  // one. Called after the map is installed so that an override written in
  // terms of other tokens resolves the same way it will in service.
  //
  // With `defer_unresolved` an override that merely fails to resolve yet is
  // left in place for a later pass; without it, this is the last word and such
  // an override goes too. Either way the map is taken to a fixed point.
  void drop_unusable_overrides(paths_type& map, const char* source, bool defer_unresolved = false);

  // Resolve ${nrpe-dh}: the first candidate folder that actually holds the
  // shipped DH parameters, or the last candidate when none of them do.
  std::string resolve_nrpe_dh(int depth);

  boost::filesystem::path get_app_data_path();
  boost::filesystem::path getBasePath();
  boost::filesystem::path getTempPath();
  std::string expand_path_impl(std::string file, int depth);
  logging::log_client_accessor get_logger() { return log_instance_; }
};
typedef std::shared_ptr<path_manager> path_instance;
}  // namespace core

}  // namespace nsclient
