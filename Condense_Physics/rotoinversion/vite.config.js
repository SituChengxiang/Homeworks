import { inlineSingleFile } from '../design/vite-single-file.js';

// three 走仓库里那份 vendor/three.module.js：交付物不许从网络加载任何东西，
// 也就不必往 node_modules 里再装一份 three。
const vendor = new URL('../vendor/three.module.js', import.meta.url).pathname;

// 不 import defineConfig：构建走 `npx --yes vite@6`，vite 在 npx 缓存里，项目目录解析不到。
export default {
  base: './',
  resolve: { alias: { three: vendor } },
  plugins: [inlineSingleFile()],
  build: { outDir: 'dist', emptyOutDir: true, target: 'es2020', cssCodeSplit: false, modulePreload: { polyfill: false } },
};
