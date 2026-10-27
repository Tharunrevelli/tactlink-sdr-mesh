import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'TACTLINK-SDR | Tactical Mesh Protocol & Telemetry Visualizer',
  description: 'Military SDR Data Link Protocol with Selective Repeat ARQ, Dijkstra Mesh Routing & CRC-32 Framing',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="antialiased bg-[#030812] text-slate-100 min-h-screen">
        {children}
      </body>
    </html>
  );
}
