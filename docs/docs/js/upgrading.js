/*
 * Filter for the "Upgrading" page (docs/docs/setup/upgrading.md).
 *
 * The page is assembled by docs/hooks/upgrading.py, which wraps every version
 * in <div class="upgrade-version" data-version="..."> and every note in
 * <div class="upgrade-entry" data-modules="A B" data-security="1">. This script
 * enables the (otherwise hidden) filter form and hides the notes that do not
 * match the version range, the ticked modules or the security-only switch.
 *
 * The selection is stored in localStorage (so a reader who comes back for the
 * next upgrade finds their modules ticked) and mirrored into the query string
 * (?from=0.16.4&to=0.18.1&security=1&modules=NRPEServer,CheckSystem) so a
 * filtered view can be bookmarked or shared. Query-string parameters win over
 * the stored selection.
 */
(function () {
  'use strict';

  var STORAGE_KEY = 'nscp.upgrading.filter';

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
    if (!params.has('from') && !params.has('to') && !params.has('security') && !params.has('modules')) {
      return null;
    }
    var checked = params.has('modules')
      ? params.get('modules').split(',').filter(Boolean)
      : all.slice();
    return {
      from: params.get('from') || '',
      to: params.get('to') || '',
      security: params.get('security') === '1',
      unchecked: all.filter(function (m) { return checked.indexOf(m) < 0; })
    };
  }

  function toQuery(state, all) {
    var params = new URLSearchParams();
    if (state.from) { params.set('from', state.from); }
    if (state.to) { params.set('to', state.to); }
    if (state.security) { params.set('security', '1'); }
    if (state.unchecked.length) {
      params.set('modules', all.filter(function (m) { return state.unchecked.indexOf(m) < 0; }).join(','));
    }
    var q = params.toString();
    return window.location.pathname + (q ? '?' + q : '') + window.location.hash;
  }

  function init() {
    var form = document.getElementById('upgrade-filter');
    if (!form || form.dataset.ready) { return; }
    form.dataset.ready = '1';

    var fromSel = document.getElementById('upgrade-from');
    var toSel = document.getElementById('upgrade-to');
    var securityBox = document.getElementById('upgrade-security');
    var summary = document.getElementById('upgrade-modules-summary');
    var status = document.getElementById('upgrade-status');
    var boxes = Array.prototype.slice.call(form.querySelectorAll('input[name="module"]'));
    var all = boxes.map(function (b) { return b.value; });
    var entries = Array.prototype.slice.call(document.querySelectorAll('.upgrade-entry'));
    var versions = Array.prototype.slice.call(document.querySelectorAll('.upgrade-version'));

    var defaults = { from: '', to: '', security: false, unchecked: [] };
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
      security: !!state.security,
      unchecked: (state.unchecked || []).filter(function (m) { return all.indexOf(m) >= 0; })
    };

    function render() {
      fromSel.value = state.from;
      toSel.value = state.to;
      securityBox.checked = state.security;
      boxes.forEach(function (b) { b.checked = state.unchecked.indexOf(b.value) < 0; });

      var checked = all.filter(function (m) { return state.unchecked.indexOf(m) < 0; });
      var shown = 0;
      entries.forEach(function (entry) {
        var version = entry.closest('.upgrade-version').dataset.version;
        var mods = (entry.dataset.modules || '').split(/\s+/).filter(Boolean);
        var visible = (!state.from || compareVersions(version, state.from) > 0) &&
          (!state.to || compareVersions(version, state.to) <= 0) &&
          (!state.security || entry.dataset.security === '1') &&
          mods.some(function (m) { return checked.indexOf(m) >= 0; });
        entry.hidden = !visible;
        if (visible) { shown++; }
      });
      versions.forEach(function (section) {
        section.hidden = !section.querySelector('.upgrade-entry:not([hidden])');
      });

      summary.textContent = checked.length === all.length ? 'all'
        : checked.length === 0 ? 'none' : checked.length + ' of ' + all.length;
      var filtered = state.from || state.to || state.security || state.unchecked.length;
      status.textContent = filtered
        ? 'Showing ' + shown + ' of ' + entries.length + ' entries.' + (shown ? '' : ' Nothing matches this selection.')
        : 'Showing all ' + entries.length + ' entries.';
      form.classList.toggle('upgrade-filter--active', !!filtered);
    }

    function update() {
      store(state);
      try {
        window.history.replaceState(null, '', toQuery(state, all));
      } catch (e) { /* file:// or a sandboxed view: the filter still applies */ }
      render();
    }

    fromSel.addEventListener('change', function () { state.from = fromSel.value; update(); });
    toSel.addEventListener('change', function () { state.to = toSel.value; update(); });
    securityBox.addEventListener('change', function () { state.security = securityBox.checked; update(); });
    boxes.forEach(function (b) {
      b.addEventListener('change', function () {
        state.unchecked = all.filter(function (m) {
          var box = boxes[all.indexOf(m)];
          return !box.checked;
        });
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
          state = { from: '', to: '', security: false, unchecked: [] };
          clearStored();
        }
        update();
      });
    });

    form.hidden = false;
    if (state.from || state.to || state.security || state.unchecked.length) {
      // A remembered selection shows in the address bar too, so the page can
      // be bookmarked or shared as it is seen.
      try { window.history.replaceState(null, '', toQuery(state, all)); } catch (e) { /* ignore */ }
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
