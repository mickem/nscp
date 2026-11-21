// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <sstream>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>

namespace check_mk {
class check_mk_exception : public std::exception {
  std::string error_;

 public:
  check_mk_exception(std::string error) : error_(error) {}
  virtual ~check_mk_exception() throw() {}
  virtual const char *what() const throw() { return error_.c_str(); }
};

// A `\r` or `\n` inside anything written on one line of the dump (a line
// item, a section title, a piggyback host) would end that line on the wire and
// let the rest pose as a row or a `<<<section>>>` header of its own. Each
// becomes a space.
inline std::string no_line_breaks(const std::string &value) {
  std::string out = value;
  for (char &c : out) {
    if (c == '\r' || c == '\n') c = ' ';
  }
  return out;
}

struct packet {
  struct section {
    std::string title;

    // Optional Checkmk header decorations. When set, render as
    //   <<<title:sep(N):cached(gen,interval):persist(epoch)>>>
    // and (for `separator`) join each line's items with that ASCII code
    // instead of a space.
    boost::optional<int> separator;
    boost::optional<std::pair<long long, long long> > cached;  // (gen_epoch, interval_seconds)
    boost::optional<long long> persist_until;

    struct line {
      line() {}
      line(const std::string &data) { set_line(data); }
      line(const line &other) : items(other.items) {}
      const line &operator=(const line &other) {
        items = other.items;
        return *this;
      }

      // Render with the section's separator (defaults to space). Items are
      // joined verbatim except for line breaks, see no_line_breaks(). This
      // only guards a line *object*: a Lua script that hands add_line() a
      // string gets one row per line break through section::add_text(), by
      // design, so a string field still has to be cleaned by whoever builds
      // the line (check_mk_format.hpp, the Lua script). Any other escaping
      // appropriate to the chosen separator is up to them as well.
      std::string to_string(int sep_char = ' ') const {
        std::string ret;
        bool first = true;
        const char sep = static_cast<char>(sep_char);
        for (const std::string &item : items) {
          if (first) {
            first = false;
          } else {
            ret.push_back(sep);
          }
          ret += no_line_breaks(item);
        }
        return ret;
      }

      std::string get_item(std::size_t id) {
        if (id >= items.size()) throw check_mk::check_mk_exception("Invalid line");
        std::list<std::string>::const_iterator cit = items.begin();
        for (std::size_t i = 0; i < id; i++) cit++;
        return *cit;
      }

      std::string get_line() { return to_string(); }

      void set_line(const std::string &data) {
        std::istringstream split(data);
        std::string chunk;
        while (std::getline(split, chunk, ' ')) items.push_back(chunk);
      }

      // Replace the item list with a single literal item. Useful when the
      // caller has already formatted the line (perfdata, MRPE blob, etc.) and
      // doesn't want the space-tokenization path.
      void set_raw(const std::string &data) {
        items.clear();
        items.push_back(data);
      }

      std::list<std::string> items;
    };

    std::list<line> lines;

    section() {}
    section(std::string title) : title(title) {}
    section(const section &other)
        : title(other.title), separator(other.separator), cached(other.cached), persist_until(other.persist_until), lines(other.lines) {}
    const section &operator=(const section &other) {
      title = other.title;
      separator = other.separator;
      cached = other.cached;
      persist_until = other.persist_until;
      lines = other.lines;
      return *this;
    }

    void push(std::string data) { lines.push_back(line(data)); }

    std::string render_header() const {
      std::string ret = "<<<" + no_line_breaks(title);
      if (separator) ret += ":sep(" + str::xtos(separator.value()) + ")";
      if (cached) ret += ":cached(" + str::xtos(cached.value().first) + "," + str::xtos(cached.value().second) + ")";
      if (persist_until) ret += ":persist(" + str::xtos(persist_until.value()) + ")";
      ret += ">>>\n";
      return ret;
    }

    std::string to_string() const {
      std::string ret = render_header();
      const int sep = separator ? separator.value() : ' ';
      for (const section::line &l : lines) {
        ret += l.to_string(sep) + "\n";
      }
      return ret;
    }

    bool empty() const { return title.empty() && lines.empty(); }

    check_mk::packet::section::line get_line(std::size_t id) {
      if (id >= lines.size()) throw check_mk::check_mk_exception("Invalid line");
      std::list<line>::const_iterator cit = lines.begin();
      for (std::size_t i = 0; i < id; i++) cit++;
      return *cit;
    }

    void add_line(check_mk::packet::section::line line) { lines.push_back(line); }

    // Add `text` as one row per line it holds. This is what a Lua script's
    // `s:add_line("a\nb")` has always produced on the wire (two rows), and it
    // has to stay that way now that to_string() blanks line breaks inside a
    // line. A `\r\n` counts as one break. Empty lines in the middle are kept
    // as empty rows, as they were sent before; only the empty piece after a
    // final line break is dropped, so `"a\n"` is the one row `a`. Text with no
    // break at all (the empty string included) is one line.
    void add_text(const std::string &text) {
      std::size_t start = 0;
      for (;;) {
        const std::size_t end = text.find_first_of("\r\n", start);
        if (end == std::string::npos) {
          if (start < text.size() || start == 0) lines.push_back(line(text.substr(start)));
          return;
        }
        lines.push_back(line(text.substr(start, end - start)));
        start = end + 1;
        if (text[end] == '\r' && start < text.size() && text[start] == '\n') ++start;
      }
    }
  };

  // Piggyback block: a list of sections that should be relayed to a different
  // monitored host. Emitted on the wire as
  //   <<<<host>>>>
  //   <<<section>>> ...
  //   <<<<>>>>
  struct piggyback_block {
    std::string host;
    std::list<section> section_list;
    piggyback_block() {}
    explicit piggyback_block(std::string host) : host(std::move(host)) {}
    void add_section(section s) { section_list.push_back(std::move(s)); }
  };

  std::list<section> section_list;
  std::list<piggyback_block> piggybacks;

  packet() {}
  packet(const packet &other) : section_list(other.section_list), piggybacks(other.piggybacks) {}
  const packet &operator=(const packet &other) {
    section_list = other.section_list;
    piggybacks = other.piggybacks;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////
  // Write to string

  std::string write() const {
    std::string ret;
    for (const section &s : section_list) ret += s.to_string();
    for (const piggyback_block &pb : piggybacks) {
      ret += "<<<<" + no_line_breaks(pb.host) + ">>>>\n";
      for (const section &s : pb.section_list) ret += s.to_string();
      ret += "<<<<>>>>\n";
    }
    return ret;
  }

  //////////////////////////////////////////////////////////////////////////
  // Read from string

  void add_section(section s) { section_list.push_back(s); }

  // Get-or-create a piggyback block for the given host, returned by
  // reference so the caller can append sections to it.
  piggyback_block &piggyback_for(const std::string &host) {
    for (piggyback_block &pb : piggybacks) {
      if (pb.host == host) return pb;
    }
    piggybacks.push_back(piggyback_block(host));
    return piggybacks.back();
  }

  // Parse the body of a section header (everything between `<<<` and `>>>`)
  // into a section with title + decorations.
  static section parse_section_header(const std::string &inner) {
    section s;
    const std::string::size_type first_colon = inner.find(':');
    s.title = (first_colon == std::string::npos) ? inner : inner.substr(0, first_colon);
    std::string::size_type pos = first_colon;
    while (pos != std::string::npos) {
      const std::string::size_type next = inner.find(':', pos + 1);
      const std::string opt = (next == std::string::npos) ? inner.substr(pos + 1) : inner.substr(pos + 1, next - pos - 1);
      const std::string::size_type lparen = opt.find('(');
      if (lparen != std::string::npos && !opt.empty() && opt[opt.size() - 1] == ')') {
        const std::string name = opt.substr(0, lparen);
        const std::string args = opt.substr(lparen + 1, opt.size() - lparen - 2);
        try {
          if (name == "sep") {
            s.separator = std::stoi(args);
          } else if (name == "cached") {
            const std::string::size_type comma = args.find(',');
            if (comma != std::string::npos) {
              s.cached = std::make_pair(std::stoll(args.substr(0, comma)), std::stoll(args.substr(comma + 1)));
            }
          } else if (name == "persist") {
            s.persist_until = std::stoll(args);
          }
        } catch (const std::exception &) {
          // Malformed option - ignore and continue.
        }
      }
      pos = next;
    }
    return s;
  }

  void read(std::string data) {
    std::istringstream split(data);
    std::string chunk;
    section s;
    piggyback_block *active_pb = NULL;

    auto flush_section = [&]() {
      if (s.empty()) return;
      if (active_pb)
        active_pb->add_section(s);
      else
        section_list.push_back(s);
      s = section();
    };

    while (std::getline(split, chunk)) {
      // Strip a trailing CR if the input is CRLF-terminated.
      if (!chunk.empty() && chunk[chunk.size() - 1] == '\r') chunk.erase(chunk.size() - 1);

      const bool is_pb = chunk.size() >= 8 && chunk.substr(0, 4) == "<<<<" && chunk.substr(chunk.size() - 4, 4) == ">>>>";
      const bool is_section = !is_pb && chunk.size() >= 6 && chunk.substr(0, 3) == "<<<" && chunk.substr(chunk.size() - 3, 3) == ">>>";

      if (is_pb) {
        flush_section();
        const std::string host = chunk.substr(4, chunk.size() - 8);
        if (host.empty()) {
          active_pb = NULL;  // close piggyback
        } else {
          active_pb = &piggyback_for(host);  // open piggyback
        }
      } else if (is_section) {
        flush_section();
        s = parse_section_header(chunk.substr(3, chunk.size() - 6));
      } else {
        s.push(chunk);
      }
    }
    flush_section();
  }

  std::string to_string() const { return write(); }

  section get_section(std::size_t id) {
    if (id >= section_list.size()) throw check_mk::check_mk_exception("Invalid section");
    std::list<section>::const_iterator cit = section_list.begin();
    for (std::size_t i = 0; i < id; i++) cit++;
    return *cit;
  }

  std::vector<char> to_vector() {
    std::string s = to_string();
    return std::vector<char>(s.begin(), s.end());
  }
};
}  // namespace check_mk
