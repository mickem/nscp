/*
 * Filter for the "Upgrading" and "Security notices" pages.
 *
 * Both pages are assembled by docs/hooks/notes.py, which marks every note as
 * <div class="note-entry" data-modules="A B" data-version="0.18.1"> (upgrade
 * notes inherit their version from the enclosing <div class="note-version">;
 * advisory table rows carry the same class and attributes). This script
 * enables the (otherwise hidden) filter form and hides the notes that do not
 * match the version range, the ticked modules, the "may need action only" switch
 * (data-action="required" or "conditional") or, on the Upgrading page, the
 * security-only switch.
 *
 * The selection is stored in localStorage — shared by both pages, so a reader
 * ticks their modules once — and mirrored into the query string
 * (?from=0.16.4&to=0.18.1&security=1&action=1&modules=NRPEServer,CheckSystem) so a
 * filtered view can be bookmarked or shared. Query-string parameters win over
 * the stored selection.
 */
(function () {
  'use strict';

  var STORAGE_KEY = 'nscp.notes.filter';

  function versionKey(v) {
    return String(v).split('.').map(function (p) { return parseInt(p, 10) || 0; });
  }

  function compareVersions(a, b) {
    var ka = versionKey(a), kb = versionKey(b);
    var n = Math.max(ka.length, kb.length);
    for (var i = 0; i < n; i++) {
      var d = (ka[i] || 0) - (kb[i] || 0);
      if (d !== 0) { return d; }
    }
    return 0;
  }

  function loadStored() {
    try {
      var raw = window.localStorage.getItem(STORAGE_KEY);
      return raw ? JSON.parse(raw) : null;
    } catch (e) {
      return null;
    }
  }

  function store(state) {
    try {
      window.localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
    } catch (e) { /* private mode, disabled storage: the filter still works for this visit */ }
  }

  function clearStored() {
    try { window.localStorage.removeItem(STORAGE_KEY); } catch (e) { /* ignore */ }
  }

  function fromQuery(all) {
    var params = new URLSearchParams(window.location.search);
    if (!params.has('from') && !params.has('to') && !params.has('security') &&
        !params.has('action') && !params.has('modules')) {
      return null;
    }
    var checked = params.has('modules')
      ? params.get('modules').split(',').filter(Boolean)
      : all.slice();
    return {
      from: params.get('from') || '',
      to: params.get('to') || '',
      security: params.get('security') === '1',
      action: params.get('action') === '1',
      unchecked: all.filter(function (m) { return checked.indexOf(m) < 0; })
    };
  }

  function toQuery(state, all) {
    var params = new URLSearchParams();
    if (state.from) { params.set('from', state.from); }
    if (state.to) { params.set('to', state.to); }
    if (state.security) { params.set('security', '1'); }
    if (state.action) { params.set('action', '1'); }
    if (state.unchecked.length) {
      params.set('modules', all.filter(function (m) { return state.unchecked.indexOf(m) < 0; }).join(','));
    }
    var q = params.toString();
    return window.location.pathname + (q ? '?' + q : '') + window.location.hash;
  }

  function isFiltered(state) {
    return !!(state.from || state.to || state.security || state.action || state.unchecked.length);
  }

  function init() {
    var form = document.getElementById('note-filter');
    if (!form || form.dataset.ready) { return; }
    form.dataset.ready = '1';

    var fromSel = document.getElementById('note-from');
    var toSel = document.getElementById('note-to');
    var securityBox = document.getElementById('note-security'); // absent on the notices page
    var actionBox = document.getElementById('note-action');
    var summary = document.getElementById('note-modules-summary');
    var status = document.getElementById('note-status');
    var boxes = Array.prototype.slice.call(form.querySelectorAll('input[name="module"]'));
    var all = boxes.map(function (b) { return b.value; });
    var entries = Array.prototype.slice.call(document.querySelectorAll('.note-entry'));
    var notes = entries.filter(function (e) { return e.tagName !== 'TR'; });
    var versions = Array.prototype.slice.call(document.querySelectorAll('.note-version'));

    var defaults = { from: '', to: '', security: false, action: false, unchecked: [] };
    var state = fromQuery(all);
    // A deep link (#0180 from the security notices, say) must land on a visible
    // section, so a stored selection is not applied when the page is opened
    // with a fragment and no explicit query.
    if (!state && !window.location.hash) {
      state = loadStored();
    }
    if (!state) { state = defaults; }
    state = {
      from: state.from || '',
      to: state.to || '',
      security: !!state.security && !!securityBox,
      action: !!state.action,
      unchecked: (state.unchecked || []).filter(function (m) { return all.indexOf(m) >= 0; })
    };

    function entryVersion(entry) {
      if (entry.dataset.version) { return entry.dataset.version; }
      var section = entry.closest('.note-version');
      return section ? section.dataset.version : '';
    }

    function matches(entry, checked) {
      var version = entryVersion(entry);
      var mods = (entry.dataset.modules || '').split(/\s+/).filter(Boolean);
      // An entry with no version (an advisory against an unsupported line)
      // is shown unless the reader says which version they come from.
      if (version) {
        if (state.from && compareVersions(version, state.from) <= 0) { return false; }
        if (state.to && compareVersions(version, state.to) > 0) { return false; }
      } else if (state.from) {
        return false;
      }
      if (state.security && entry.dataset.security !== '1') { return false; }
      if (state.action && entry.dataset.action === 'none') { return false; }
      return mods.some(function (m) { return checked.indexOf(m) >= 0; });
    }

    function render() {
      fromSel.value = state.from;
      toSel.value = state.to;
      if (securityBox) { securityBox.checked = state.security; }
      actionBox.checked = state.action;
      boxes.forEach(function (b) { b.checked = state.unchecked.indexOf(b.value) < 0; });

      var checked = all.filter(function (m) { return state.unchecked.indexOf(m) < 0; });
      var shown = 0;
      entries.forEach(function (entry) {
        var visible = matches(entry, checked);
        entry.hidden = !visible;
        if (visible && entry.tagName !== 'TR') { shown++; }
      });
      versions.forEach(function (section) {
        section.hidden = !section.querySelector('.note-entry:not([hidden])');
      });

      summary.textContent = checked.length === all.length ? 'all'
        : checked.length === 0 ? 'none' : checked.length + ' of ' + all.length;
      var filtered = isFiltered(state);
      status.textContent = filtered
        ? 'Showing ' + shown + ' of ' + notes.length + ' entries.' + (shown ? '' : ' Nothing matches this selection.')
        : 'Showing all ' + notes.length + ' entries.';
      form.classList.toggle('note-filter--active', filtered);
    }

    function syncUrl() {
      try {
        window.history.replaceState(null, '', toQuery(state, all));
      } catch (e) { /* file:// or a sandboxed view: the filter still applies */ }
    }

    function update() {
      store(state);
      syncUrl();
      render();
    }

    fromSel.addEventListener('change', function () { state.from = fromSel.value; update(); });
    toSel.addEventListener('change', function () { state.to = toSel.value; update(); });
    if (securityBox) {
      securityBox.addEventListener('change', function () { state.security = securityBox.checked; update(); });
    }
    actionBox.addEventListener('change', function () { state.action = actionBox.checked; update(); });
    boxes.forEach(function (b) {
      b.addEventListener('change', function () {
        state.unchecked = boxes.filter(function (box) { return !box.checked; }).map(function (box) { return box.value; });
        update();
      });
    });
    Array.prototype.forEach.call(form.querySelectorAll('button[data-action]'), function (btn) {
      btn.addEventListener('click', function () {
        var action = btn.dataset.action;
        if (action === 'all') {
          state.unchecked = [];
        } else if (action === 'none') {
          state.unchecked = all.slice();
        } else if (action === 'reset') {
          state = { from: '', to: '', security: false, action: false, unchecked: [] };
          clearStored();
        }
        update();
      });
    });

    form.hidden = false;
    if (isFiltered(state)) {
      // A remembered selection shows in the address bar too, so the page can
      // be bookmarked or shared as it is seen.
      syncUrl();
    }
    render();
  }

  // mkdocs-material re-renders the page body on instant navigation; hook into
  // that when present and fall back to the plain DOM event otherwise.
  if (typeof window.document$ !== 'undefined' && window.document$.subscribe) {
    window.document$.subscribe(init);
  } else if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
