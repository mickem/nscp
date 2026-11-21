-- ---------------------------------------------------------------------------
-- default_check_mk.lua
--
-- Emits a check_mk agent dump over the CheckMKServer TCP listener.
--
-- Row-per-thing sections (ps, df, services) come from native NSClient++ check
-- commands in their `fetch-only` mode, which returns the section body already
-- in Checkmk's format without running any threshold logic. Counter sections
-- on Linux (kernel, cpu, mem) are the /proc files themselves, sent as-is the
-- way the official agent sends them. The Lua layer only adds the
-- `<<<section>>>` headers.
--
-- Local-check entries are configured under
--   /settings/check_mk/server/local/<service name> = command=<nscp_cmd> [args ...]
-- MRPE entries the same way under /settings/check_mk/server/mrpe.
-- ---------------------------------------------------------------------------

local IS_WIN = (package.config:sub(1, 1) == '\\')
local AGENT_OS = IS_WIN and 'windows' or 'linux'

local function trim(s) return (s:gsub('^%s+', ''):gsub('%s+$', '')) end

local function split_lines(str)
  local t = {}
  for ln in (str or ''):gmatch('[^\r\n]+') do t[#t + 1] = ln end
  return t
end

local function hostname()
  local h = os.getenv('COMPUTERNAME') or os.getenv('HOSTNAME')
  if h and h ~= '' then return h end
  local p = io.popen('hostname')
  if not p then return 'unknown' end
  local out = p:read('*a') or ''
  p:close()
  return trim(out)
end

-- The lines of a file, or nil when it cannot be read.
local function read_lines(path)
  local f = io.open(path, 'r')
  if not f then return nil end
  local content = f:read('*a')
  f:close()
  return split_lines(content)
end

local function append_all(target, lines)
  for _, ln in ipairs(lines or {}) do target[#target + 1] = ln end
  return target
end

local function add_section(packet, title, body)
  local s = section.new()
  s:set_title(title)
  for _, ln in ipairs(body or {}) do s:add_line(ln) end
  packet:add_section(s)
end

-- Run a check via fetch-only and stuff the lines into a section. Returns
-- true if the section was added (i.e. the check returned ok with content).
local function fetch_section(packet, core, command, section_name)
  local status, msg = core:simple_query(command, { 'fetch-only' })
  if status ~= 'ok' then
    nscp.error('check_mk: ' .. command .. ' fetch-only returned ' .. tostring(status) .. ': ' .. tostring(msg))
    return false
  end
  if not msg or msg == '' then return false end
  add_section(packet, section_name, split_lines(msg))
  return true
end

-- Format an integer as right-padded /proc/meminfo style ("Key:    value kB").
local function meminfo_line(key, bytes)
  local kb = math.floor(tonumber(bytes or 0) / 1024)
  return string.format('%-14s %d kB', key .. ':', kb)
end

-- ---------------------------------------------------------------------------
-- Sections
-- ---------------------------------------------------------------------------

-- The OS lines of the check_mk header, which Checkmk turns into the host
-- labels cmk/os_type, cmk/os_platform, cmk/os_name and cmk/os_version. The
-- official agents read them from /etc/os-release (Linux) and the OS caption
-- and version (Windows); check_os_version reads the same sources. Resolved on
-- the first fetch and kept for the life of the script: the OS does not change
-- under a running agent, and the Windows check also queries WMI for the BIOS.
local os_lines_cache = nil
local function os_lines(core)
  if os_lines_cache then return os_lines_cache end
  local syntax = IS_WIN and '${version}|${kernel_version}' or '${distribution}|${distribution_name}|${version}'
  local ok, _, msg = pcall(function()
    return core:simple_query('check_os_version', { 'top-syntax=${list}', 'ok-syntax=${list}', 'detail-syntax=' .. syntax })
  end)
  local lines = { 'OSType: ' .. AGENT_OS }
  if ok and msg and msg ~= '' then
    local fields = {}
    for field in (msg .. '|'):gmatch('([^|]*)|') do fields[#fields + 1] = trim(field) end
    local platform, name, version
    if IS_WIN then
      name, version = fields[1], fields[2]
    else
      platform, name, version = fields[1], fields[2], fields[3]
    end
    if platform and platform ~= '' then lines[#lines + 1] = 'OSPlatform: ' .. platform end
    if name and name ~= '' then lines[#lines + 1] = 'OSName: ' .. name end
    if version and version ~= '' then lines[#lines + 1] = 'OSVersion: ' .. version end
    os_lines_cache = lines
  end
  -- Not cached on failure (CheckSystem not loaded yet, a transient error):
  -- the next fetch tries again and the header still carries OSType.
  return lines
end

local function build_check_mk(packet, core)
  local body = {
    'Version: nsclient++',
    'AgentOS: ' .. AGENT_OS,
    'Hostname: ' .. (hostname() or 'unknown'),
  }
  append_all(body, os_lines(core))
  add_section(packet, 'check_mk', body)
end

local function build_systemtime(packet)
  add_section(packet, 'systemtime', { tostring(os.time()) })
end

-- Uptime in seconds, read when the dump is built: Checkmk computes "up since"
-- as its own clock minus this number, so a value from the metrics store (up
-- to one `metrics interval`, 10 s by default, old) moved the boot time
-- forward by as much. Linux sends /proc/uptime as-is, as the official agent
-- does; Windows takes the seconds from check_uptime's perfdata. The metrics
-- store is the fallback, and if it has not ticked yet either (a cold start)
-- the section is left out for that fetch.
local function fresh_uptime(core)
  if not IS_WIN then
    local lines = read_lines('/proc/uptime')
    return lines and lines[1] and trim(lines[1]) or nil
  end
  local ok, _, _, perf = pcall(function() return core:simple_query('check_uptime', {}) end)
  if not ok or not perf then return nil end
  return perf:match("uptime'?=(%d+)")
end

local function build_uptime(packet, core, metrics)
  local uptime = fresh_uptime(core)
  if not uptime or uptime == '' then
    uptime = metrics:value('system.uptime.boot.raw')
    if not (uptime and tonumber(uptime)) then return end
  end
  add_section(packet, 'uptime', { trim(uptime) })
end

-- Windows: the eight lines the official Windows agent sends, built from the
-- GlobalMemoryStatusEx metrics. Page* is the commit limit and the commit
-- still available (ullTotalPageFile / ullAvailPageFile), Virtual* the address
-- space; the Page* lines are what make Checkmk pick its Windows memory check
-- (mem_win) instead of the Linux one. Swap* is 0, as the official agent
-- sends it: Windows has no separate swap. Skipped for the first fetch after a
-- cold start, before the collector has ticked, like build_uptime.
local function build_mem_win(packet, metrics)
  local phys_total = metrics:value('system.mem.physical.total')
  local phys_avail = metrics:value('system.mem.physical.avail')
  if not (phys_total and phys_avail) then return end
  local body = {
    meminfo_line('MemTotal',  phys_total),
    meminfo_line('MemFree',   phys_avail),
    meminfo_line('SwapTotal', 0),
    meminfo_line('SwapFree',  0),
  }
  local commit_total = metrics:value('system.mem.commited.total')
  local commit_avail = metrics:value('system.mem.commited.avail')
  if commit_total and commit_avail then
    body[#body + 1] = meminfo_line('PageTotal', commit_total)
    body[#body + 1] = meminfo_line('PageFree',  commit_avail)
  end
  local virt_total = metrics:value('system.mem.virtual.total')
  local virt_avail = metrics:value('system.mem.virtual.avail')
  if virt_total and virt_avail then
    body[#body + 1] = meminfo_line('VirtualTotal', virt_total)
    body[#body + 1] = meminfo_line('VirtualFree',  virt_avail)
  end
  add_section(packet, 'mem', body)
end

-- Linux: /proc/meminfo as-is, which is what Checkmk's mem check parses
-- (MemAvailable, Buffers, Cached, Slab, Committed_AS, HugePages_* ...). Like
-- the official agent, drop the summary lines very old kernels print
-- ("Mem:", "Swap:", "...total:").
local function build_mem_linux(packet)
  local lines = read_lines('/proc/meminfo')
  if not lines then return end
  local body = {}
  for _, ln in ipairs(lines) do
    if not (ln:match('^Mem:') or ln:match('^Swap:') or ln:match('total:')) then body[#body + 1] = ln end
  end
  if #body > 0 then add_section(packet, 'mem', body) end
end

-- Linux: the load averages with the CPU count appended to the same line, then
-- the kernel's thread limit - the line layout of the official agent's
-- `<<<cpu>>>`, which Checkmk's CPU load and thread checks read.
local function build_cpu_linux(packet)
  local loadavg = read_lines('/proc/loadavg')
  if not loadavg or not loadavg[1] then return end
  local cpus = 0
  for _, ln in ipairs(read_lines('/proc/cpuinfo') or {}) do
    if ln:match('^processor') then cpus = cpus + 1 end
  end
  local body = { trim(loadavg[1]) .. ' ' .. tostring(cpus) }
  local threads_max = read_lines('/proc/sys/kernel/threads-max')
  if threads_max and threads_max[1] then body[#body + 1] = trim(threads_max[1]) end
  add_section(packet, 'cpu', body)
end

-- Linux: the current epoch, then /proc/vmstat and /proc/stat as-is. Checkmk
-- computes CPU utilisation, context switches, process creations and major
-- page faults from the deltas between two fetches, so these are the raw
-- cumulative counters, not rates.
local function build_kernel_linux(packet)
  local stat = read_lines('/proc/stat')
  if not stat then return end
  local body = { tostring(os.time()) }
  append_all(body, read_lines('/proc/vmstat'))
  append_all(body, stat)
  add_section(packet, 'kernel', body)
end

-- These sections are row-per-thing (one entry per process / service / drive),
-- which the metrics store doesn't represent. They stay on `check_X fetch-only`.
local function build_ps(packet, core)       fetch_section(packet, core, 'check_process',  'ps')       end
local function build_services(packet, core) fetch_section(packet, core, 'check_service',  'services') end
-- On Linux the df body also carries the [df_inodes_start] block.
local function build_df(packet, core)       fetch_section(packet, core, 'check_drivesize','df')       end

-- ---------------------------------------------------------------------------
-- local-check / MRPE bridge driven by settings
-- ---------------------------------------------------------------------------
-- core:simple_query returns lowercase code names (see lua_wrapper::push_code).
local STATE_TO_INT = { ok = 0, warning = 1, critical = 2, unknown = 3 }

-- A control character in a field (above all `\r` / `\n`) would end the line
-- early and let the rest pose as a row or a `<<<section>>>` header of its own.
-- Service names come from settings keys and passive submissions, messages and
-- perfdata from whatever the check (or the submitter) produced, so every field
-- on a local/mrpe line goes through one of these two.
--
-- no_controls() swaps each control character for one space and touches
-- nothing else, so a name keeps its exact spacing (it is the service's
-- identity in Checkmk) and perfdata keeps its exact labels.
--
-- The byte classes here are spelled out on purpose: `%c` and `%s` follow the
-- C locale, and the agent runs under the user's (cli_parser calls
-- setlocale(LC_ALL, "")). In a single-byte locale bytes in 0x80-0xFF can count
-- as control or space characters, and those are UTF-8 lead and continuation
-- bytes - a name like `Ácme` would go out with a space in the middle and turn
-- into another service. These are exactly the bytes sanitize_field() in
-- include/check/check_mk_format.hpp replaces: 0x00-0x1F and DEL.
local CONTROL_BYTE = '[%z\1-\31\127]'

local function no_controls(s)
  return (tostring(s or ''):gsub(CONTROL_BYTE, ' '))
end

-- squash() is for message text, which has always had every run of whitespace
-- folded into one space. After no_controls() the only whitespace left is the
-- plain space, so folding spaces is that, without asking the locale.
local function squash(s)
  return (no_controls(s):gsub(' +', ' '))
end

-- Parse a settings spec like: "command=check_cpu warn=load>80 crit=load>90"
local function parse_spec(spec)
  local cmd
  local args = {}
  for token in spec:gmatch('%S+') do
    if not cmd and token:sub(1, 8) == 'command=' then
      cmd = token:sub(9)
    else
      args[#args + 1] = token
    end
  end
  return cmd, args
end

local function quote_local_name(name)
  name = no_controls(name)
  if name:find(' ', 1, true) or name == '' then
    return '"' .. (name:gsub('"', '\'')) .. '"'
  end
  return name
end

-- Checkmk's `<<<local>>>` parser rejects Nagios-style single-quoted labels
-- (`'name with spaces'=value`), even though that's valid Nagios perfdata and
-- the MRPE parser does accept it. Strip the quotes and replace internal spaces
-- with underscores so the metric label is a single token. Multiple metrics
-- stay space-separated, which Checkmk does accept.
local function normalize_local_perfdata(perf)
  perf = no_controls(perf)
  if not perf:find('[^ ]') then return '-' end
  perf = perf:gsub("'([^']*)'(=)", function(label, eq)
    return label:gsub(' +', '_') .. eq
  end)
  if perf == '' then return '-' end
  return perf
end

local function enum_settings_section(settings, path)
  local ok, keys = pcall(function() return settings:get_section(path) end)
  if not ok or type(keys) ~= 'table' then return {} end
  return keys
end

-- Format a local-check line: `[cached(g,t) ]<state> "<name>" <perf> <text>`
local function format_local_line(name, code, message, perf, generated, ttl)
  local prefix = ''
  if generated and ttl and ttl > 0 then
    prefix = string.format('cached(%d,%d) ', generated, ttl)
  end
  local p = normalize_local_perfdata(perf)
  return string.format('%s%d %s %s %s', prefix, code, quote_local_name(name), p, squash(message))
end

-- Format an MRPE line: `[cached(g,t) ]<name> <code> <text-with-pipe-perf>`
local function format_mrpe_line(name, code, message, perf, generated, ttl)
  local prefix = ''
  if generated and ttl and ttl > 0 then
    prefix = string.format('cached(%d,%d) ', generated, ttl)
  end
  local text = squash(message)
  perf = no_controls(perf)
  if perf:find('[^ ]') then text = text .. '|' .. perf end
  return string.format('%s%s %d %s', prefix, no_controls(name), code, text)
end

local function build_local(packet, core, settings, submissions)
  local body = {}
  -- Synchronous entries: run a check command on every fetch.
  for _, name in ipairs(enum_settings_section(settings, '/settings/check_mk/server/local')) do
    local spec = settings:get_string('/settings/check_mk/server/local', name, '')
    if spec ~= '' then
      local cmd, args = parse_spec(spec)
      if cmd then
        local status, message, perf = core:simple_query(cmd, args)
        local code = STATE_TO_INT[status] or 3
        body[#body + 1] = format_local_line(name, code, message, perf)
      end
    end
  end
  -- Cached entries: passive submissions on the check_mk-local channel.
  for _, e in ipairs(submissions:get('local')) do
    body[#body + 1] = format_local_line(e.name, e.code, e.message, e.perf, e.generated, e.ttl)
  end
  if #body > 0 then add_section(packet, 'local', body) end
end

local function build_mrpe(packet, core, settings, submissions)
  local body = {}
  for _, name in ipairs(enum_settings_section(settings, '/settings/check_mk/server/mrpe')) do
    local spec = settings:get_string('/settings/check_mk/server/mrpe', name, '')
    if spec ~= '' then
      local cmd, args = parse_spec(spec)
      if cmd then
        local status, message, perf = core:simple_query(cmd, args)
        local code = STATE_TO_INT[status] or 3
        body[#body + 1] = format_mrpe_line(name, code, message, perf)
      end
    end
  end
  for _, e in ipairs(submissions:get('mrpe')) do
    body[#body + 1] = format_mrpe_line(e.name, e.code, e.message, e.perf, e.generated, e.ttl)
  end
  if #body > 0 then add_section(packet, 'mrpe', body) end
end

-- ---------------------------------------------------------------------------
-- entry points
-- ---------------------------------------------------------------------------
local function safe(label, fn, ...)
  local ok, err = pcall(fn, ...)
  if not ok then nscp.error('check_mk section "' .. label .. '" failed: ' .. tostring(err)) end
end

function server_process(packet)
  local core = Core()
  local settings = Settings()
  local metrics = Metrics()
  local submissions = Submissions()

  safe('check_mk',  build_check_mk, packet, core)
  if IS_WIN then
    safe('mem',        build_mem_win,    packet, metrics)
  else
    safe('mem',    build_mem_linux,    packet)
    safe('cpu',    build_cpu_linux,    packet)
    safe('kernel', build_kernel_linux, packet)
  end
  safe('df',       build_df,       packet, core)
  safe('ps',       build_ps,       packet, core)
  safe('services', build_services, packet, core)
  safe('local', build_local, packet, core, settings, submissions)
  safe('mrpe',  build_mrpe,  packet, core, settings, submissions)
  -- Last: Checkmk reads both against its own clock when the fetch ends, and
  -- the sections above take seconds on a busy host (ps, services), which
  -- showed up as a System Time offset and an "up since" late by as much.
  safe('uptime', build_uptime, packet, core, metrics)
  if IS_WIN then safe('systemtime', build_systemtime, packet) end
end

function client_process(packet)
  local cnt = packet:size_section()
  nscp.print('Got sections: ' .. cnt)
  for i = 1, cnt do
    local s = packet:get_section(i)
    nscp.print(' + ' .. s:get_title())
    for j = 1, s:size_line() do
      nscp.print('    + ' .. s:get_line(j):get_line())
    end
  end
end

local reg = mk.new()
reg:client_callback(client_process)
reg:server_callback(server_process)
reg = nil
