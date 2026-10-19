// 不 import { defineConfig } from 'vite'：构建走的是 `npx --yes vite@6`，
// vite 装在 npx 缓存里，配置文件从项目目录解析不到那个模块。
// defineConfig 只是类型辅助，去掉它行为完全一致 —— 换来的是仓库零 node_modules。
import { inlineSingleFile } from '../vite-single-file.js';

// 交付形态是「一个文件、双击即开、不联网」：构建完把 JS/CSS 全部内联，dist 里只留 HTML。
export default {
  base: './',
  plugins: [inlineSingleFile()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    target: 'es2020',
    cssCodeSplit: false,
    modulePreload: { polyfill: false },
  },
};
