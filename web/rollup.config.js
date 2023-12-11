import { swc } from 'rollup-plugin-swc3';
import resolve from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';

const extensions = [".js", ".jsx", ".ts", ".tsx", ".mjs", ".wasm"];
const globals = {
  'lit': 'lit',
  'lit/decorators.js': 'lit/decorators.js',
};

export default {
  input: './src/lottie-player.ts',
  treeshake: false,
  output: [
    {
      file: './dist/lottie-player.esm.js',
      format: 'esm',
      globals,
    },
    {
      file: './dist/lottie-player.cjs.js',
      format: 'cjs',
      globals,
    },
    {
      file: './dist/lottie-player.js',
      format: 'umd',
      name: 'lottie-player',
      globals,
    },
  ],
  plugins: [
    resolve(),
    commonjs({
      include: [
        './dist/thorvg-wasm.js',
        './dist/thorvg-wasm.wasm'
      ],
      requireReturnsDefault: 'auto',
    }),
    swc({
      include: /\.[mc]?[jt]sx?$/,
      exclude: /node_modules/,
      tsconfig: 'tsconfig.json',
      extensions,
    }),
  ],
}
