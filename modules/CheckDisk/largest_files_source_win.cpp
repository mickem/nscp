// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <error/error.hpp>
#include <functional>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <queue>
#include <str/utf8.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "largest_files_scan.hpp"
#include "ntfs_mft.hpp"

// The Windows data sources for check_largest_files.
//
// The fast one reads the volume's NTFS Master File Table directly: one
// sequential read of every file record on the volume, instead of a directory
// enumeration per folder. That needs the volume opened for raw reads, which
// takes administrator rights (the service account has them), and a local
// NTFS volume. Anything else - ReFS, FAT, a network share, an unelevated
// caller - falls back to walking the tree with FindFirstFileEx.
//
// Neither follows a link. The walker skips every reparse point whose tag is a
// name surrogate (symbolic links, junctions, mount points); the MFT holds only
// what is physically on the volume, so the children a junction appears to
// have are not there to be found, and the link records themselves are
// skipped by the same rule.

namespace largest_files {

namespace {
using steady = std::chrono::steady_clock;

std::wstring to_wide(const std::string &s) { return utf8::cvt<std::wstring>(s); }
std::string to_utf8(const std::wstring &s) { return utf8::cvt<std::string>(s); }
std::string to_utf8(const std::u16string &s) { return to_utf8(std::wstring(s.begin(), s.end())); }

bool is_link(const DWORD attributes, const DWORD tag) { return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && (tag & ntfs::reparse_name_surrogate) != 0; }

// The path as the check reports it: backslashes, and no trailing separator
// except on a drive root.
std::wstring normalise(std::wstring path) {
  std::replace(path.begin(), path.end(), L'/', L'\\');
  while (path.size() > 1 && path.back() == L'\\' && !(path.size() == 3 && path[1] == L':')) path.pop_back();
  if (path.size() == 2 && path[1] == L':') path += L'\\';
  return path;
}

std::wstring join(const std::wstring &dir, const std::wstring &name) {
  if (!dir.empty() && dir.back() == L'\\') return dir + name;
  return dir + L'\\' + name;
}

// The same path for the I/O calls, in the \\?\ form so a deep tree is not
// cut off at MAX_PATH.
std::wstring long_path(const std::wstring &path) {
  if (path.compare(0, 4, L"\\\\?\\") == 0) return path;
  if (path.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + path.substr(2);
  return L"\\\\?\\" + path;
}

std::string last_error() { return error::lookup::last_error(); }

struct handle {
  HANDLE h = INVALID_HANDLE_VALUE;
  explicit handle(HANDLE h_) : h(h_) {}
  ~handle() {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  }
  handle(const handle &) = delete;
  handle &operator=(const handle &) = delete;
  bool ok() const { return h != INVALID_HANDLE_VALUE; }
};

// A page-aligned buffer: raw volume reads must be sector aligned in both
// offset and length, and in memory.
struct aligned_buffer {
  unsigned char *data = nullptr;
  std::size_t size = 0;
  explicit aligned_buffer(std::size_t size_) : size(size_) {
    data = static_cast<unsigned char *>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  }
  ~aligned_buffer() {
    if (data) VirtualFree(data, 0, MEM_RELEASE);
  }
  aligned_buffer(const aligned_buffer &) = delete;
  aligned_buffer &operator=(const aligned_buffer &) = delete;
};

bool read_at(HANDLE volume, const std::uint64_t offset, unsigned char *buffer, const DWORD length) {
  OVERLAPPED ov = {};
  ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
  ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
  DWORD got = 0;
  return ReadFile(volume, buffer, length, &got, &ov) && got == length;
}

// --- the tree walker --------------------------------------------------------

scan_result walk(const scan_request &request, const std::wstring &root) {
  scan_result result;
  result.method = "walk";
  top_n ranking(request.top, request.min_size);
  folder_rollup rollup(ranking, request.folder_depth);
  const std::wstring pattern = to_wide(request.pattern);

  struct frame {
    std::wstring path;
    int level;
    HANDLE find;
    bool first;
    WIN32_FIND_DATAW data;
  };
  std::vector<frame> stack;
  auto open = [&](const std::wstring &path, const int level) -> bool {
    frame f;
    f.path = path;
    f.level = level;
    f.first = true;
    // Basic info skips the short name, and the large fetch asks for bigger
    // batches per round trip, which is most of the walk's cost on a share.
    f.find = FindFirstFileExW((long_path(path) + L"\\*").c_str(), FindExInfoBasic, &f.data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (f.find == INVALID_HANDLE_VALUE) {
      if (GetLastError() != ERROR_FILE_NOT_FOUND) result.unreadable++;
      return false;
    }
    stack.push_back(std::move(f));
    return true;
  };

  if (!open(root, 0)) {
    result.error = "Unable to read folder " + to_utf8(root) + ": " + last_error();
    return result;
  }
  if (request.folders) rollup.enter(to_utf8(root), 0);

  unsigned counter = 0;
  while (!stack.empty()) {
    frame &f = stack.back();
    if (!f.first && !FindNextFileW(f.find, &f.data)) {
      FindClose(f.find);
      stack.pop_back();
      if (request.folders) rollup.leave();
      continue;
    }
    f.first = false;
    if ((++counter & 0x3FF) == 0 && steady::now() >= request.deadline) {
      result.timed_out = true;
      break;
    }
    const WIN32_FIND_DATAW &d = f.data;
    // dwReserved0 holds the reparse tag whenever the reparse attribute is set.
    if (is_link(d.dwFileAttributes, d.dwReserved0)) {
      result.links_skipped++;
      continue;
    }
    if ((d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      if (wcscmp(d.cFileName, L".") == 0 || wcscmp(d.cFileName, L"..") == 0) continue;
      result.folders_seen++;
      // `f` dangles once open() has pushed the child.
      const std::wstring child = join(f.path, d.cFileName);
      const int level = f.level + 1;
      if (open(child, level) && request.folders) rollup.enter(rollup.ranks(level) ? to_utf8(child) : std::string(), level);
      continue;
    }
    result.files_seen++;
    if (!pattern.empty() && !wildcard_match(pattern.c_str(), d.cFileName, true)) continue;
    const unsigned long long size = (static_cast<unsigned long long>(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
    if (request.folders) {
      rollup.add_file(size);
    } else if (ranking.accepts(size)) {
      ranking.add(entry{to_utf8(join(f.path, d.cFileName)), size, 1});
    }
  }
  for (const frame &f : stack) FindClose(f.find);
  if (request.folders) rollup.finish();
  result.entries = ranking.take_sorted();
  return result;
}

// --- the MFT reader ---------------------------------------------------------

class mft_scanner {
 public:
  mft_scanner(const scan_request &request, std::wstring root) : request_(request), root_(std::move(root)) {}

  // False (with `why`) when the MFT cannot be used for this root; the caller
  // then walks instead.
  bool run(scan_result &result, std::string &why) {
    if (!open(why)) return false;
    if (!read_table(result, why)) return false;
    if (request_.folders)
      rank_folders(result);
    else
      rank_files(result);
    result.method = "mft";
    return true;
  }

 private:
  static constexpr std::uint32_t none = 0xFFFFFFFF;
  static constexpr std::int32_t depth_unknown = -2;
  static constexpr std::int32_t depth_pending = -3;

  enum : std::uint8_t {
    f_used = 0x01,
    f_dir = 0x02,
    f_named = 0x04,
    f_dos_name = 0x08,
    f_match = 0x10,
    f_reparse = 0x20,
    // A reparse point whose tag was read and is not a name surrogate: a
    // deduplicated or cloud-backed file, which is counted.
    f_reparse_data = 0x40,
  };

  bool is_link(const std::uint8_t f) const { return (f & f_reparse) != 0 && (f & f_reparse_data) == 0; }

  bool open(std::string &why) {
    handle dir(CreateFileW(long_path(root_).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (!dir.ok()) {
      why = "unable to open the folder: " + last_error();
      return false;
    }
    wchar_t fs_name[MAX_PATH + 1] = {};
    if (!GetVolumeInformationByHandleW(dir.h, nullptr, 0, nullptr, nullptr, nullptr, fs_name, MAX_PATH)) {
      why = "unable to read the volume information: " + last_error();
      return false;
    }
    if (wcscmp(fs_name, L"NTFS") != 0) {
      why = "the volume is " + to_utf8(fs_name) + ", not NTFS";
      return false;
    }
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!GetFileInformationByHandle(dir.h, &info)) {
      why = "unable to read the folder's file id: " + last_error();
      return false;
    }
    root_index_ = ntfs::record_index((static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow);

    // The volume the folder physically lives on, which is not necessarily
    // the drive letter it was named through: a mounted volume or a junction
    // on the way puts it somewhere else. A network share has no volume GUID
    // path, and is refused here.
    std::wstring final_path(32768, L'\0');
    const DWORD n = GetFinalPathNameByHandleW(dir.h, &final_path[0], static_cast<DWORD>(final_path.size()), VOLUME_NAME_GUID | FILE_NAME_NORMALIZED);
    if (n == 0 || n >= final_path.size()) {
      why = "the folder is not on a local volume";
      return false;
    }
    final_path.resize(n);
    const std::size_t brace = final_path.find(L'}');
    if (brace == std::wstring::npos) {
      why = "the folder is not on a local volume";
      return false;
    }
    const std::wstring volume_name = final_path.substr(0, brace + 1);

    volume_ = CreateFileW(volume_name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (volume_ == INVALID_HANDLE_VALUE) {
      why = "unable to open the volume for reading (this needs administrator rights): " + last_error();
      return false;
    }

    NTFS_VOLUME_DATA_BUFFER data = {};
    DWORD returned = 0;
    if (!DeviceIoControl(volume_, FSCTL_GET_NTFS_VOLUME_DATA, nullptr, 0, &data, sizeof(data), &returned, nullptr)) {
      why = "unable to read the NTFS volume data: " + last_error();
      return false;
    }
    cluster_ = data.BytesPerCluster;
    sector_ = data.BytesPerSector;
    record_size_ = data.BytesPerFileRecordSegment;
    if (sector_ == 0 || cluster_ == 0 || record_size_ < 512 || record_size_ > 65536 || (record_size_ & (record_size_ - 1)) != 0 ||
        (sector_ & (sector_ - 1)) != 0) {
      why = "unexpected NTFS geometry";
      return false;
    }
    const std::uint64_t records = static_cast<std::uint64_t>(data.MftValidDataLength.QuadPart) / record_size_;
    if (records == 0 || records >= none) {
      why = "unexpected MFT size";
      return false;
    }
    total_ = static_cast<std::uint32_t>(records);
    if (root_index_ >= total_) {
      why = "the folder's file id is outside the MFT";
      return false;
    }

    // The table's own layout is in its first record, $MFT.
    std::vector<unsigned char> first;
    if (!read_stream_at(static_cast<std::uint64_t>(data.MftStartLcn.QuadPart) * cluster_, record_size_, first)) {
      why = "unable to read the $MFT record: " + last_error();
      return false;
    }
    ntfs::record mft;
    if (!ntfs::parse_record(first.data(), first.size(), mft, true) || mft.data_runs.empty()) {
      why = "unable to parse the $MFT record";
      return false;
    }
    std::uint64_t covered = 0;
    for (const ntfs::run &r : mft.data_runs) {
      // A sparse run in the MFT means something this reader does not expect.
      if (r.lcn < 0) {
        why = "the $MFT has a sparse run";
        return false;
      }
      covered += r.clusters * cluster_;
    }
    // A table fragmented enough that its run list continues in an extension
    // record is rare, and not worth reading the attribute list for.
    if (covered < static_cast<std::uint64_t>(total_) * record_size_) {
      why = "the $MFT is too fragmented to read directly";
      return false;
    }
    runs_ = mft.data_runs;
    return true;
  }

 public:
  ~mft_scanner() {
    if (volume_ != INVALID_HANDLE_VALUE) CloseHandle(volume_);
  }

 private:
  // Read `length` bytes at a volume offset that need not be sector aligned.
  bool read_stream_at(const std::uint64_t offset, const std::size_t length, std::vector<unsigned char> &out) {
    const std::uint64_t start = offset & ~static_cast<std::uint64_t>(sector_ - 1);
    const std::uint64_t end = (offset + length + sector_ - 1) & ~static_cast<std::uint64_t>(sector_ - 1);
    aligned_buffer buffer(static_cast<std::size_t>(end - start));
    if (!buffer.data || !read_at(volume_, start, buffer.data, static_cast<DWORD>(end - start))) return false;
    out.assign(buffer.data + (offset - start), buffer.data + (offset - start) + length);
    return true;
  }

  // Read record `index` again, by mapping its offset in the table onto the
  // runs. A record may straddle two runs when clusters are smaller than
  // records, so it is read piecewise.
  bool read_record(const std::uint32_t index, std::vector<unsigned char> &out) {
    out.clear();
    std::uint64_t want = static_cast<std::uint64_t>(index) * record_size_;
    std::size_t left = record_size_;
    std::uint64_t run_start = 0;
    for (const ntfs::run &r : runs_) {
      const std::uint64_t run_bytes = r.clusters * cluster_;
      if (want < run_start + run_bytes) {
        const std::size_t take = static_cast<std::size_t>((std::min)(static_cast<std::uint64_t>(left), run_start + run_bytes - want));
        std::vector<unsigned char> piece;
        if (!read_stream_at(static_cast<std::uint64_t>(r.lcn) * cluster_ + (want - run_start), take, piece)) return false;
        out.insert(out.end(), piece.begin(), piece.end());
        left -= take;
        want += take;
        if (left == 0) return true;
      }
      run_start += run_bytes;
    }
    return false;
  }

  bool read_table(scan_result &result, std::string &why) {
    size_.assign(total_, 0);
    parent_.assign(total_, none);
    flags_.assign(total_, 0);
    pattern_ = [this] {
      const std::wstring w = to_wide(request_.pattern);
      return std::u16string(w.begin(), w.end());
    }();

    // Large sequential reads, a whole number of clusters (and so of sectors).
    const std::size_t chunk =
        static_cast<std::size_t>((std::max)(static_cast<std::uint64_t>(cluster_), static_cast<std::uint64_t>((4u << 20) / cluster_ * cluster_)));
    aligned_buffer buffer(chunk);
    if (!buffer.data) {
      why = "out of memory";
      return false;
    }
    std::vector<unsigned char> carry;
    std::uint32_t next = 0;
    auto handle_record = [&](unsigned char *p) {
      if (next < total_) take(next, p);
      next++;
    };
    auto feed = [&](unsigned char *p, std::size_t length) {
      if (!carry.empty()) {
        const std::size_t take_n = (std::min)(record_size_ - carry.size(), length);
        carry.insert(carry.end(), p, p + take_n);
        p += take_n;
        length -= take_n;
        if (carry.size() == record_size_) {
          handle_record(carry.data());
          carry.clear();
        }
      }
      while (length >= record_size_) {
        handle_record(p);
        p += record_size_;
        length -= record_size_;
      }
      if (length > 0) carry.assign(p, p + length);
    };

    for (const ntfs::run &r : runs_) {
      std::uint64_t offset = static_cast<std::uint64_t>(r.lcn) * cluster_;
      std::uint64_t left = r.clusters * cluster_;
      while (left > 0 && next < total_) {
        if (steady::now() >= request_.deadline) {
          result.timed_out = true;
          return true;
        }
        const DWORD length = static_cast<DWORD>((std::min)(left, static_cast<std::uint64_t>(chunk)));
        if (!read_at(volume_, offset, buffer.data, length)) {
          why = "reading the MFT failed: " + last_error();
          return false;
        }
        feed(buffer.data, length);
        offset += length;
        left -= length;
      }
    }
    return true;
  }

  void take(const std::uint32_t index, unsigned char *data) {
    ntfs::record r;
    if (!ntfs::parse_record(data, record_size_, r) || !r.in_use) return;
    const std::uint64_t target64 = r.base_record != 0 ? r.base_record : index;
    if (target64 >= total_) return;
    const std::uint32_t target = static_cast<std::uint32_t>(target64);
    std::uint8_t &f = flags_[target];
    if (r.base_record == 0) {
      f |= f_used;
      if (r.is_directory) f |= f_dir;
      if (r.has_attributes && (r.attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) f |= f_reparse;
    }
    if (r.is_reparse_point) {
      f |= f_reparse;
      if (!r.is_link()) f |= f_reparse_data;
    }
    if (r.has_name && (!(f & f_named) || ((f & f_dos_name) && r.name_namespace != ntfs::namespace_dos))) {
      f |= f_named;
      if (r.name_namespace == ntfs::namespace_dos)
        f |= f_dos_name;
      else
        f &= ~f_dos_name;
      parent_[target] = r.parent < total_ ? static_cast<std::uint32_t>(r.parent) : none;
      if (pattern_.empty() || wildcard_match(pattern_.c_str(), r.name.c_str(), true))
        f |= f_match;
      else
        f &= ~f_match;
      // Only folder names are needed to build paths; the few files that make
      // the list have their record read again. A name found in an extension
      // record is kept whatever it names, as it could not be read back from
      // the base record.
      if (r.is_directory || r.base_record != 0) names_[target] = r.name;
    }
    // A hard link is another $FILE_NAME with its own parent. The file is in
    // the scan if any of its links is, so the other parents are kept too.
    for (const ntfs::file_name &n : r.names) {
      if (n.name_namespace != ntfs::namespace_dos && n.parent < total_ && n.parent != parent_[target])
        other_parents_.emplace(target, static_cast<std::uint32_t>(n.parent));
    }
    if (r.has_data) size_[target] = r.data_size;
  }

  // The folder's depth below the root, or -1 when it is not beneath it.
  std::int32_t depth_of(const std::uint32_t start) {
    if (start == none || start >= total_) return -1;
    chain_.clear();
    std::uint32_t cur = start;
    std::int32_t base = -1;
    while (true) {
      if (cur == none || cur >= total_) break;
      const std::int32_t known = depth_[cur];
      if (known != depth_unknown) {
        base = known == depth_pending ? -1 : known;
        break;
      }
      const std::uint8_t f = flags_[cur];
      // Metadata records (and the volume root, when it is not the scan root)
      // end a chain outside the scan.
      if (!(f & f_used) || !(f & f_dir) || is_link(f) || cur < ntfs::first_user_record) {
        depth_[cur] = -1;
        break;
      }
      depth_[cur] = depth_pending;
      chain_.push_back(cur);
      cur = parent_[cur];
    }
    for (auto it = chain_.rbegin(); it != chain_.rend(); ++it) depth_[*it] = base < 0 ? -1 : ++base;
    return depth_[start];
  }

  void prepare_depths() {
    depth_.assign(total_, depth_unknown);
    depth_[root_index_] = 0;
  }

  bool expired(std::uint32_t i, scan_result &result) {
    if ((i & 0xFFFF) != 0 || steady::now() < request_.deadline) return false;
    result.timed_out = true;
    return true;
  }

  // The parent through which a file is in the scan: its first name's, or
  // failing that one of its other links'. `none` when it is outside.
  std::uint32_t parent_in_scan(const std::uint32_t index) {
    if (depth_of(parent_[index]) >= 0) return parent_[index];
    const auto links = other_parents_.equal_range(index);
    for (auto it = links.first; it != links.second; ++it) {
      if (depth_of(it->second) >= 0) return it->second;
    }
    return none;
  }

  // The path of folder `index`, beneath the root.
  std::wstring path_of(std::uint32_t index) {
    std::vector<const std::u16string *> parts;
    // A deque, as the pointers taken into it must survive later additions.
    std::deque<std::u16string> fallback;
    while (index != none && index < total_ && index != root_index_ && parts.size() < 4096) {
      auto it = names_.find(index);
      if (it != names_.end()) {
        parts.push_back(&it->second);
      } else {
        const std::string missing = "#" + std::to_string(index);
        fallback.emplace_back(missing.begin(), missing.end());
        parts.push_back(&fallback.back());
      }
      index = parent_[index];
    }
    std::wstring path = root_;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) path = join(path, std::wstring((*it)->begin(), (*it)->end()));
    return path;
  }

  void rank_files(scan_result &result) {
    prepare_depths();
    typedef std::pair<std::uint64_t, std::uint32_t> candidate;
    std::priority_queue<candidate, std::vector<candidate>, std::greater<candidate>> best;
    for (std::uint32_t i = ntfs::first_user_record; i < total_; i++) {
      if (expired(i, result)) break;
      const std::uint8_t f = flags_[i];
      if ((f & (f_used | f_dir | f_named)) != (f_used | f_named)) continue;
      if (is_link(f)) {
        if (parent_in_scan(i) != none) result.links_skipped++;
        continue;
      }
      if (parent_in_scan(i) == none) continue;
      result.files_seen++;
      if (!(f & f_match) || size_[i] < request_.min_size || request_.top == 0) continue;
      if (best.size() < request_.top) {
        best.emplace(size_[i], i);
      } else if (size_[i] > best.top().first) {
        best.pop();
        best.emplace(size_[i], i);
      }
    }
    count_folders(result);

    top_n ranking(request_.top, request_.min_size);
    std::vector<unsigned char> buffer;
    while (!best.empty()) {
      const candidate c = best.top();
      best.pop();
      const std::uint32_t parent = parent_in_scan(c.second);
      ntfs::record r;
      std::u16string name;
      if (read_record(c.second, buffer) && ntfs::parse_record(buffer.data(), buffer.size(), r) && r.has_name) {
        // The name of the link that is in the scan.
        name = r.name;
        for (const ntfs::file_name &n : r.names) {
          if (n.parent == parent && n.name_namespace != ntfs::namespace_dos) {
            name = n.name;
            break;
          }
        }
      } else {
        auto it = names_.find(c.second);
        if (it != names_.end()) name = it->second;
      }
      if (name.empty()) {
        const std::string missing = "#" + std::to_string(c.second);
        name.assign(missing.begin(), missing.end());
      }
      ranking.add(entry{to_utf8(join(path_of(parent), std::wstring(name.begin(), name.end()))), c.first, 1});
    }
    result.entries = ranking.take_sorted();
  }

  void count_folders(scan_result &result) {
    for (std::uint32_t i = ntfs::first_user_record; i < total_; i++) {
      if ((flags_[i] & (f_used | f_dir)) != (f_used | f_dir)) continue;
      if (is_link(flags_[i])) {
        if (depth_of(parent_[i]) >= 0) result.links_skipped++;
        continue;
      }
      if (depth_of(i) > 0) result.folders_seen++;
    }
  }

  void rank_folders(scan_result &result) {
    prepare_depths();
    std::vector<std::uint64_t> totals(total_, 0);
    std::vector<std::uint64_t> counts(total_, 0);
    const int max_depth = request_.folder_depth;
    for (std::uint32_t i = ntfs::first_user_record; i < total_; i++) {
      if (expired(i, result)) break;
      const std::uint8_t f = flags_[i];
      if ((f & (f_used | f_dir | f_named)) != (f_used | f_named)) continue;
      if (is_link(f)) {
        if (parent_in_scan(i) != none) result.links_skipped++;
        continue;
      }
      // A hard-linked file counts once, under the first of its links that is
      // in the scan: it takes up its space once.
      const std::uint32_t parent = parent_in_scan(i);
      if (parent == none) continue;
      result.files_seen++;
      if (!(f & f_match)) continue;
      // Every folder on the way up gets the file; the ones deeper than the
      // ranked levels are only passed through.
      std::int32_t depth = depth_[parent];
      for (std::uint32_t d = parent; depth > 0; d = parent_[d], depth = depth_[d]) {
        if (max_depth < 0 || depth <= max_depth) {
          totals[d] += size_[i];
          counts[d]++;
        }
      }
    }
    count_folders(result);
    top_n ranking(request_.top, request_.min_size);
    for (std::uint32_t i = ntfs::first_user_record; i < total_; i++) {
      if (depth_[i] < 1 || (max_depth >= 0 && depth_[i] > max_depth)) continue;
      if (ranking.accepts(totals[i])) ranking.add(entry{to_utf8(path_of(i)), totals[i], counts[i]});
    }
    result.entries = ranking.take_sorted();
  }

  const scan_request &request_;
  std::wstring root_;
  HANDLE volume_ = INVALID_HANDLE_VALUE;
  std::uint32_t cluster_ = 0;
  std::uint32_t sector_ = 0;
  std::size_t record_size_ = 0;
  std::uint32_t total_ = 0;
  std::uint64_t root_index_ = 0;
  std::vector<ntfs::run> runs_;
  std::u16string pattern_;

  std::vector<std::uint64_t> size_;
  std::vector<std::uint32_t> parent_;
  std::vector<std::uint8_t> flags_;
  std::unordered_map<std::uint32_t, std::u16string> names_;
  std::unordered_multimap<std::uint32_t, std::uint32_t> other_parents_;
  std::vector<std::int32_t> depth_;
  std::vector<std::uint32_t> chain_;
};

}  // namespace

scan_result scan(const scan_request &request) {
  const std::wstring root = normalise(to_wide(request.root));
  const DWORD attributes = GetFileAttributesW(long_path(root).c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    scan_result result;
    result.error = "Path not found: " + request.root;
    return result;
  }
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    scan_result result;
    result.error = "Not a folder: " + request.root;
    return result;
  }

  if (request.method != scan_method::walk) {
    scan_result result;
    std::string why;
    mft_scanner mft(request, root);
    if (mft.run(result, why)) return result;
    if (request.method == scan_method::mft) {
      scan_result failed;
      failed.error = "Unable to read the MFT for " + request.root + ": " + why;
      return failed;
    }
    NSC_DEBUG_MSG("check_largest_files: not using the MFT for " + request.root + " (" + why + "), walking the tree instead");
  }
  return walk(request, root);
}

}  // namespace largest_files
