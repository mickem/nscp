-- ---------------------------------------------------------------------------
-- default_check_mk.lua
--
-- Emits a check_mk agent dump over the CheckMKServer TCP listener.
--
-- All sections are sourced from native NSClient++ check commands using their
-- `fetch-only` mode, which returns raw OS data without running any threshold
-- logic. The Lua layer only wraps that data in `<<<section>>>` headers.
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
local function build_check_mk(packet)
  add_section(packet, 'check_mk', {
    'Version: nsclient++',
    'AgentOS: ' .. AGENT_OS,
    'Hostname: ' .. (hostname() or 'unknown'),
  })
end

local function build_systemtime(packet)
  add_section(packet, 'systemtime', { tostring(os.time()) })
end

-- Read uptime seconds from the metrics store. If the store hasn't ticked yet
-- (first request after a cold start) the section is omitted for that fetch;
-- Checkmk discovery just retries on the next cycle.
local function build_uptime(packet, metrics)
  local boot = metrics:value('system.uptime.boot.raw')
  if boot and boot ~= '' and tonumber(boot) then
    add_section(packet, 'uptime', { trim(boot) })
  end
end

-- Build /proc/meminfo-style output from the system.mem.* metrics. Same
-- "skip on cold start" behavior as build_uptime.
local function build_mem(packet, metrics)
  local phys_total = metrics:value('system.mem.physical.total')
  local phys_avail = metrics:value('system.mem.physical.avail')
  if not (phys_total and phys_avail) then return end
  local body = {
    meminfo_line('MemTotal', phys_total),
    meminfo_line('MemFree',  phys_avail),
  }
  local page_total = metrics:value('system.mem.page.total')
  local page_avail = metrics:value('system.mem.page.avail')
  if page_total and page_avail then
    body[#body + 1] = meminfo_line('SwapTotal', page_total)
    body[#body + 1] = meminfo_line('SwapFree',  page_avail)
  end
  add_section(packet, 'mem', body)
end

-- These sections are row-per-thing (one entry per process / service / drive),
-- which the metrics store doesn't represent. They stay on `check_X fetch-only`.
local function build_ps(packet, core)       fetch_section(packet, core, 'check_process',  'ps')       end
local function build_services(packet, core) fetch_section(packet, core, 'check_service',  'services') end
local function build_df_win(packet, core)   fetch_section(packet, core, 'check_drivesize','df')       end

-- Linux has no check_drivesize equivalent in nscp; fall back to /bin/df until
-- a native fetch-only is added.
local function build_df_linux(packet)
  local p = io.popen('df -PT -x tmpfs -x devtmpfs -x squashfs -x overlay 2>/dev/null')
  if not p then return end
  local raw = split_lines(p:read('*a') or '')
  p:close()
  local body = {}
  for i, line in ipairs(raw) do
    if i > 1 then
      local fs, ftype, total, used, avail, pct, mp = line:match(
        '^(%S+)%s+(%S+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%%%s+(.+)$')
      if fs then
        body[#body + 1] = string.format('%s %s %s %s %s %s%% %s', fs, ftype, total, used, avail, pct, mp)
      end
    end
  end
  if #body > 0 then add_section(packet, 'df', body) end
end

-- ---------------------------------------------------------------------------
-- local-check / MRPE bridge driven by settings
-- ---------------------------------------------------------------------------
-- core:simple_query returns lowercase code names (see lua_wrapper::push_code).
local STATE_TO_INT = { ok = 0, warning = 1, critical = 2, unknown = 3 }

local function squash(s)
  return ((s or ''):gsub('[\r\n]+', ' '):gsub('%s+', ' '))
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
  if name:find('%s') or name == '' then
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
  if not perf or perf == '' then return '-' end
  perf = perf:gsub("'([^']*)'(=)", function(label, eq)
    return label:gsub('%s+', '_') .. eq
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
  if perf and perf ~= '' then text = text .. '|' .. perf end
  return string.format('%s%s %d %s', prefix, name, code, text)
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

  safe('check_mk',  build_check_mk, packet)
  safe('uptime',    build_uptime,   packet, metrics)
  safe('mem',       build_mem,      packet, metrics)
  safe('ps',        build_ps,       packet, core)
  if IS_WIN then
    safe('systemtime', build_systemtime, packet)
    safe('df',         build_df_win,     packet, core)
    safe('services',   build_services,   packet, core)
  else
    safe('df',       build_df_linux, packet)
    safe('services', build_services, packet, core)
  end
  safe('local', build_local, packet, core, settings, submissions)
  safe('mrpe',  build_mrpe,  packet, core, settings, submissions)
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
