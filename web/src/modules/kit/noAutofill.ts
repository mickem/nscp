// Settings fields hold the agent's credentials, not the reader's: no
// browser or password manager should offer to fill or save them. Browsers
// honour autocomplete; the managers each read their own marker.
export const NO_AUTOFILL = {
  autoComplete: "off",
  "data-1p-ignore": "true", // 1Password
  "data-lpignore": "true", // LastPass
  "data-bwignore": "true", // Bitwarden
  "data-form-type": "other", // Dashlane
} as const;

// A password field: "new-password" is what keeps browsers from filling in a
// saved one ("off" is ignored on password fields).
export const NO_PASSWORD_AUTOFILL = { ...NO_AUTOFILL, autoComplete: "new-password" } as const;
