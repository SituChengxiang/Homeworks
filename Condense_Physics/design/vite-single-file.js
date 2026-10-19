// 把 Vite 产物压回一个文件：交付形态是「双击即开、不联网」，
// 所以 JS / CSS 全部内联，dist 里只留一个 HTML。
// 自己写这 30 行，是为了不依赖第三方插件、也让产物结构一眼能核对。
export function inlineSingleFile() {
  return {
    name: 'inline-single-file',
    enforce: 'post',
    apply: 'build',
    generateBundle(_opts, bundle) {
      const htmlName = Object.keys(bundle).find((n) => n.endsWith('.html'));
      if (!htmlName) throw new Error('产物里没有 HTML：检查 vite.config 的 root / build.rollupOptions.input');
      let html = String(bundle[htmlName].source);

      for (const [name, item] of Object.entries(bundle)) {
        const base = name.split('/').pop();
        if (item.type === 'chunk' && item.isEntry) {
          const code = item.code.replace(/<\/script/gi, '<\\/script');
          const tag = new RegExp(`<script[^>]*src="[^"]*${esc(base)}"[^>]*></script>`);
          if (!tag.test(html)) throw new Error(`HTML 里找不到入口脚本 ${base}，没法内联`);
          html = html.replace(tag, () => `<script type="module">\n${code}\n</script>`);
          delete bundle[name];                    // 内联完就要从产物里摘掉，否则单文件不成立
        } else if (item.type === 'asset' && name.endsWith('.css')) {
          const css = String(item.source).replace(/<\/style/gi, '<\\/style');
          html = html.replace(new RegExp(`<link[^>]*href="[^"]*${esc(base)}"[^>]*>`),
            () => `<style>\n${css}\n</style>`);
          delete bundle[name];
        }
      }

      const left = Object.keys(bundle).filter((n) => !n.endsWith('.html'));
      if (left.length) throw new Error(`还有没被内联的产物：${left.join(', ')}`);
      if (/<script[^>]*src=|<link[^>]*href=/.test(html)) throw new Error('HTML 里仍有外部引用，单文件不成立');
      bundle[htmlName].source = html;
    },
  };
}

const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
