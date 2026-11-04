import { defineConfig } from 'vite';

export default defineConfig({
  server: {
    host: '0.0.0.0',
    port: 5174,
    strictPort: true,
    allowedHosts: true,
    cors: true
  },
  preview: { host: '0.0.0.0', allowedHosts: true },
  build: { chunkSizeWarningLimit: 1200 }
});
