module.exports = {
  env: {
    browser: true,
    es2021: true,
  },
  extends: ["standard-with-typescript", "plugin:wc/best-practice"],
  overrides: [
    {
      env: {
        node: true,
      },
      files: [".eslintrc.{js,cjs}"],
      parserOptions: {
        sourceType: "script",
      },
    },
  ],
  parserOptions: {
    ecmaVersion: "latest",
    sourceType: "module",
  },
  rules: {
    semi: [2, "always"],
    "@typescript-eslint/semi": "off",
  },
  settings: {
    wc: {
      elementBaseClasses: ["LitElement"],
    },
  },
};
