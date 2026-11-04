import { defineConfig } from 'vite';

// Build the game as ONE classic (iife) script plus one css file, so the whole
// thing can be inlined into a single .html with no module loading and no
// separate chunk requests.
export default defineConfig({
  publicDir: false,
  build: {
    outDir: 'dist-standalone',
    emptyOutDir: true,
    cssCodeSplit: false,
    modulePreload: false,
    assetsInlineLimit: 100000000,
    target: 'es2020',
    chunkSizeWarningLimit: 4000,
    rollupOptions: {
      output: {
        format: 'iife',
        inlineDynamicImports: true,
        entryFileNames: 'app.js',
        assetFileNames: 'app.[ext]'
      }
    }
  }
});
