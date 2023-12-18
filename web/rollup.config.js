import { swc } from "rollup-plugin-swc3";
import { dts } from "rollup-plugin-dts";
import { nodeResolve } from "@rollup/plugin-node-resolve";
import commonjs from "@rollup/plugin-commonjs";
import { terser } from "rollup-plugin-terser";
import nodePolyfills from 'rollup-plugin-polyfill-node';
import bakedEnv from 'rollup-plugin-baked-env';
import pkg from './package.json';

const extensions = [".js", ".jsx", ".ts", ".tsx", ".mjs", ".wasm"];
const globals = {
  'lit': 'lit',
  'lit/decorators.js': 'lit/decorators.js',
};

export default [
  {
    input: "./src/lottie-player.ts",
    treeshake: false,
    output: [
      {
        file: './dist/lottie-player.js',
        format: "umd",
        name,
        minifyInternalExports: true,
        inlineDynamicImports: true,
        sourcemap: true,
        globals,
      },
      {
        file: pkg.main,
        name,
        format: "cjs",
        minifyInternalExports: true,
        inlineDynamicImports: true,
        sourcemap: true,
        globals,
      },
      {
        file: pkg.module,
        format: "esm",
        name,
        inlineDynamicImports: true,
        sourcemap: true,
        globals,
      },
    ],
    plugins: [
      bakedEnv({ THORVG_VERSION: process.env.THORVG_VERSION }),
      nodePolyfills(),
      commonjs({
        include: /node_modules/
      }),
      swc({
        include: /\.[mc]?[jt]sx?$/,
        exclude: /node_modules/,
        tsconfig: "tsconfig.json",
        jsc: {
          parser: {
            syntax: "typescript",
            tsx: false,
            decorators: true,
            declaration: true,
            dynamicImport: true,
          },
          target: "es5",
        },
      }),
      nodeResolve(),
      terser({
        compress: true,
        mangle: true,
        output: {
          comments: false,
        },
      }),
    ],
  },
  {
    input: "./src/lottie-player.ts",
    treeshake: false,
    output: [
      {
        file: './dist/lottie-player.d.ts',
        format: "esm",
      }
    ],
    plugins: [
      dts(),
    ],
  }
];
