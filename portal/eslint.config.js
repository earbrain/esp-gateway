import js from "@eslint/js";
import tseslint from "typescript-eslint";

const projectConfig = {
  files: ["src/**/*.{ts,tsx}", "vite.config.ts"],
  ignores: ["dist/**", "node_modules/**"],
  languageOptions: {
    parser: tseslint.parser,
    parserOptions: {
      project: "./tsconfig.json",
      tsconfigRootDir: import.meta.dirname,
      ecmaVersion: "latest",
      sourceType: "module",
      ecmaFeatures: {
        jsx: true,
      },
    },
  },
  plugins: {
    "@typescript-eslint": tseslint.plugin,
  },
  rules: {
    indent: ["error", 2, { SwitchCase: 1 }],
    "@typescript-eslint/no-unused-vars": ["warn", { argsIgnorePattern: "^_" }],
  },
};

export default [js.configs.recommended, ...tseslint.configs.recommended, projectConfig];
