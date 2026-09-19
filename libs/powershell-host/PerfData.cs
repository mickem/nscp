// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using PB.Common;
using Line = PB.Commands.QueryResponseMessage.Types.Response.Types.Line;

namespace NSCP.PowerShell
{
    /// <summary>
    /// Nagios performance data ("'label'=value[UOM];warn;crit;min;max") to and
    /// from the protobuf representation.
    ///
    /// A check answered by a script goes out as "message|perfdata" and is split
    /// and parsed by the native module, with the same parser the external-script
    /// path uses. These two are for the other direction: what a script hands to
    /// <c>$nscp.Core.SimpleSubmit</c>, and what it gets back from
    /// <c>$nscp.Core.SimpleQuery</c>.
    /// </summary>
    internal static class PerfData
    {
        /// <summary>Parse a performance data string onto <paramref name="line"/>.</summary>
        internal static void Parse(string perf, Line line)
        {
            if (string.IsNullOrWhiteSpace(perf) || line == null) return;
            foreach (var item in Split(perf))
            {
                var equals = item.LastIndexOf('=');
                if (equals <= 0) continue;
                var alias = item.Substring(0, equals).Trim().Trim('\'');
                if (alias.Length == 0) continue;
                var fields = item.Substring(equals + 1).Split(';');
                var value = new PerformanceData.Types.FloatValue();
                value.Value = Number(fields[0], out var unit);
                value.Unit = unit;
                if (fields.Length > 1) value.Warning = Optional(fields[1]);
                if (fields.Length > 2) value.Critical = Optional(fields[2]);
                if (fields.Length > 3) value.Minimum = Optional(fields[3]);
                if (fields.Length > 4) value.Maximum = Optional(fields[4]);
                line.Perf.Add(new PerformanceData { Alias = alias, FloatValue = value });
            }
        }

        /// <summary>Render <paramref name="line"/>'s performance data back to a string.</summary>
        internal static string Format(Line line)
        {
            if (line == null || line.Perf.Count == 0) return string.Empty;
            var text = new StringBuilder();
            foreach (var perf in line.Perf)
            {
                if (text.Length > 0) text.Append(' ');
                text.Append('\'').Append(perf.Alias).Append("'=");
                if (perf.StringValue != null)
                {
                    text.Append(perf.StringValue.Value);
                    continue;
                }
                var value = perf.FloatValue;
                if (value == null) continue;
                text.Append(Text(value.Value)).Append(value.Unit ?? string.Empty);
                // Trailing empty fields are dropped, so ";;;0;100" stays
                // readable while ";80;90" does not grow tails it does not need.
                var fields = new[]
                {
                    value.Warning != null ? Text(value.Warning.Value) : string.Empty,
                    value.Critical != null ? Text(value.Critical.Value) : string.Empty,
                    value.Minimum != null ? Text(value.Minimum.Value) : string.Empty,
                    value.Maximum != null ? Text(value.Maximum.Value) : string.Empty,
                };
                var last = fields.Length - 1;
                while (last >= 0 && fields[last].Length == 0) last--;
                for (var i = 0; i <= last; i++) text.Append(';').Append(fields[i]);
            }
            return text.ToString();
        }

        /// <summary>
        /// Split on the spaces between items, keeping the ones inside a quoted
        /// label ("'disk usage'=50%") together.
        /// </summary>
        private static IEnumerable<string> Split(string perf)
        {
            var start = 0;
            var quoted = false;
            for (var i = 0; i < perf.Length; i++)
            {
                if (perf[i] == '\'') quoted = !quoted;
                if (quoted || perf[i] != ' ') continue;
                if (i > start) yield return perf.Substring(start, i - start);
                start = i + 1;
            }
            if (start < perf.Length) yield return perf.Substring(start);
        }

        /// <summary>The leading number of a field, with whatever follows it as the unit.</summary>
        private static double Number(string field, out string unit)
        {
            unit = string.Empty;
            if (field == null) return 0;
            var text = field.Trim();
            var end = 0;
            while (end < text.Length && (char.IsDigit(text[end]) || text[end] == '.' || text[end] == ',' || (end == 0 && (text[end] == '-' || text[end] == '+')))) end++;
            unit = text.Substring(end).Trim();
            return Value(text.Substring(0, end));
        }

        private static OptionalFloat Optional(string field)
        {
            if (string.IsNullOrWhiteSpace(field)) return null;
            // Nagios range syntax ("4:5", "@0:90", "~:10") carries no single
            // number; take the lower bound, as the native parser does.
            var text = field.Trim().TrimStart('@');
            var colon = text.IndexOf(':');
            if (colon >= 0) text = text.Substring(0, colon);
            if (text == "~" || text.Length == 0) return null;
            return new OptionalFloat { Value = Number(text, out _) };
        }

        private static double Value(string text)
        {
            if (string.IsNullOrEmpty(text)) return 0;
            return double.TryParse(text.Replace(',', '.'), NumberStyles.Float, CultureInfo.InvariantCulture, out var value) ? value : 0;
        }

        private static string Text(double value)
        {
            return value.ToString("0.######", CultureInfo.InvariantCulture);
        }
    }
}
