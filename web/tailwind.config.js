/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./app/**/*.{js,ts,jsx,tsx,mdx}",
    "./pages/**/*.{js,ts,jsx,tsx,mdx}",
    "./components/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  theme: {
    extend: {
      colors: {
        tactical: {
          bg: '#050b14',
          card: '#0a1628',
          border: '#1e3a5f',
          cyan: '#06b6d4',
          blue: '#3b82f6',
          amber: '#f59e0b',
          red: '#ef4444',
          green: '#10b981',
        }
      }
    },
  },
  plugins: [],
}
