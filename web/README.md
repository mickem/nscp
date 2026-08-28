# NSClient++ web UI

## Testing

Two test suites validate that the UI renders correctly:

- **Unit tests** (`src/**/*.test.ts[x]`, [Vitest](https://vitest.dev/) +
  [React Testing Library](https://testing-library.com/) in jsdom) cover pure
  logic (metric parsing, Redux slices, API response transforms) and component
  rendering (widgets, pages, the login flow, auth gating in the router). The
  REST API is stubbed at the `fetch` level via `src/test/test-utils.tsx`, so no
  backend is needed.
- **Integration tests** (`e2e/*.spec.ts`, [Playwright](https://playwright.dev/))
  build the production bundle, serve it with `vite preview` and drive a real
  Chromium against it with the `/api` routes mocked in the browser
  (`e2e/mock-api.ts`). They validate the rendered UX end to end: the sign-in
  screen and login flow, dashboard widgets and SVG charts, sidebar navigation,
  filtering and deep links.

```sh
npm test           # unit tests (vitest run)
npm run test:watch # unit tests in watch mode
npm run test:e2e   # Playwright integration tests (builds the app first)
```

`npm run test:e2e` needs a Playwright Chromium; run `npx playwright install chromium`
once, or point `PLAYWRIGHT_CHROMIUM_EXECUTABLE` at an existing Chromium binary.

# React + TypeScript + Vite

This template provides a minimal setup to get React working in Vite with HMR and some ESLint rules.

Currently, two official plugins are available:

- [@vitejs/plugin-react](https://github.com/vitejs/vite-plugin-react/blob/main/packages/plugin-react/README.md) uses [Babel](https://babeljs.io/) for Fast Refresh
- [@vitejs/plugin-react-swc](https://github.com/vitejs/vite-plugin-react-swc) uses [SWC](https://swc.rs/) for Fast Refresh

## Expanding the ESLint configuration

If you are developing a production application, we recommend updating the configuration to enable type aware lint rules:

- Configure the top-level `parserOptions` property like this:

```js
export default tseslint.config({
  languageOptions: {
    // other options...
    parserOptions: {
      project: ["./tsconfig.node.json", "./tsconfig.app.json"],
      tsconfigRootDir: import.meta.dirname,
    },
  },
});
```

- Replace `tseslint.configs.recommended` to `tseslint.configs.recommendedTypeChecked` or `tseslint.configs.strictTypeChecked`
- Optionally add `...tseslint.configs.stylisticTypeChecked`
- Install [eslint-plugin-react](https://github.com/jsx-eslint/eslint-plugin-react) and update the config:

```js
// eslint.config.js
import react from "eslint-plugin-react";

export default tseslint.config({
  // Set the react version
  settings: { react: { version: "18.3" } },
  plugins: {
    // Add the react plugin
    react,
  },
  rules: {
    // other rules...
    // Enable its recommended rules
    ...react.configs.recommended.rules,
    ...react.configs["jsx-runtime"].rules,
  },
});
```
