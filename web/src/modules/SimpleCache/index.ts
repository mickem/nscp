// The result cache (modules/SimpleCache): results submitted on a channel,
// kept by a key and read back with check_cache.
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/cache";
const TABLE: KeyTable = {
  [PATH]: {
    channel: { builtIn: fixed("CACHE") },
    "primary index": { builtIn: fixed("${alias-or-command}") },
  },
};

const SimpleCache: ModulePage = {
  id: "SimpleCache",
  Panel: simplePage({
    moduleId: "SimpleCache",
    title: "Result cache",
    subheader: "Keeps the latest result submitted on a channel, for check_cache to read back.",
    path: PATH,
    table: TABLE,
    section: "What is cached",
    fields: [
      {
        key: "channel",
        label: "Channel",
        kind: "text",
        help: "Results submitted here are cached: point a scheduler or a real-time check at it.",
      },
      {
        key: "primary index",
        label: "Cache key",
        kind: "text",
        help: "What a result is kept by, and what check_cache asks for: ${alias-or-command}, ${command}, ${alias}, ${host}, ${channel}.",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
};

export default SimpleCache;
