'use client';

import React from 'react';
import { Radio, Shield, Plane, Navigation, Anchor, Target, AlertTriangle } from 'lucide-react';

interface NodePosition {
  id: number;
  callsign: string;
  role: string;
  x: number;
  y: number;
}

const NODES: NodePosition[] = [
  { id: 1, callsign: 'HQ_COMMAND', role: 'HQ', x: 80, y: 220 },
  { id: 2, callsign: 'RADAR_NORTH', role: 'RADAR', x: 260, y: 70 },
  { id: 3, callsign: 'UAV_RELAY', role: 'UAV', x: 270, y: 220 },
  { id: 4, callsign: 'ARMOR_GROUP', role: 'ARMOR', x: 470, y: 150 },
  { id: 5, callsign: 'NAVAL_PATROL', role: 'NAVAL', x: 120, y: 370 },
  { id: 6, callsign: 'ARTILLERY_DIR', role: 'ARTILLERY', x: 380, y: 350 },
];

const LINKS = [
  { u: 1, v: 2, snr: '24 dBm', loss: '3%' },
  { u: 1, v: 3, snr: '34 dBm', loss: '1%' },
  { u: 1, v: 5, snr: '26 dBm', loss: '2%' },
  { u: 2, v: 4, snr: '18 dBm', loss: '6%' },
  { u: 2, v: 6, snr: '22 dBm', loss: '4%' },
  { u: 3, v: 4, snr: '30 dBm', loss: '1%' },
  { u: 3, v: 6, snr: '28 dBm', loss: '2%' },
  { u: 4, v: 6, snr: '20 dBm', loss: '5%' },
];

interface MeshNetworkMapProps {
  activePath: number[];
  isUavJammed: boolean;
  selectedNode: number | null;
  onSelectNode: (id: number) => void;
}

export default function MeshNetworkMap({
  activePath,
  isUavJammed,
  selectedNode,
  onSelectNode,
}: MeshNetworkMapProps) {
  // Helper to get coordinates
  const getNode = (id: number) => NODES.find((n) => n.id === id);

  const isLinkActive = (u: number, v: number) => {
    for (let i = 0; i < activePath.length - 1; ++i) {
      if (
        (activePath[i] === u && activePath[i + 1] === v) ||
        (activePath[i] === v && activePath[i + 1] === u)
      ) {
        return true;
      }
    }
    return false;
  };

  const getNodeIcon = (role: string) => {
    switch (role) {
      case 'HQ': return <Shield className="w-4 h-4 text-cyan-400" />;
      case 'RADAR': return <Radio className="w-4 h-4 text-emerald-400" />;
      case 'UAV': return <Plane className="w-4 h-4 text-amber-400" />;
      case 'ARMOR': return <Target className="w-4 h-4 text-rose-400" />;
      case 'NAVAL': return <Anchor className="w-4 h-4 text-blue-400" />;
      default: return <Navigation className="w-4 h-4 text-slate-400" />;
    }
  };

  return (
    <div className="relative w-full bg-[#040c18] border border-cyan-900/50 rounded-xl p-4 shadow-xl flex flex-col items-center">
      <div className="w-full flex items-center justify-between text-xs font-mono text-cyan-400 mb-2">
        <span>TACTICAL MANET TOPOLOGY // RF MESH</span>
        <span className="flex items-center gap-1.5">
          {isUavJammed ? (
            <span className="text-rose-400 flex items-center gap-1 font-bold animate-pulse">
              <AlertTriangle className="w-3.5 h-3.5" /> EW JAMMING ACTIVE (NODE 3)
            </span>
          ) : (
            <span className="text-emerald-400 flex items-center gap-1">
              <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse"></span> ALL RF CHANNELS CLEAR
            </span>
          )}
        </span>
      </div>

      <svg viewBox="0 0 560 440" className="w-full max-w-[560px] h-auto overflow-visible select-none">
        {/* Render Links */}
        {LINKS.map((link) => {
          const n1 = getNode(link.u);
          const n2 = getNode(link.v);
          if (!n1 || !n2) return null;

          const active = isLinkActive(link.u, link.v);
          const jammed = isUavJammed && (link.u === 3 || link.v === 3);

          return (
            <g key={`${link.u}-${link.v}`}>
              <line
                x1={n1.x}
                y1={n1.y}
                x2={n2.x}
                y2={n2.y}
                stroke={jammed ? '#ef4444' : active ? '#06b6d4' : '#1e3a5f'}
                strokeWidth={active ? 3 : 1.5}
                strokeDasharray={jammed ? '4,4' : active ? '6,3' : 'none'}
                className={active ? 'animate-pulse' : ''}
              />
              {/* SNR Badge on midpoint */}
              <text
                x={(n1.x + n2.x) / 2}
                y={(n1.y + n2.y) / 2 - 4}
                fill={jammed ? '#f87171' : active ? '#67e8f9' : '#64748b'}
                fontSize="8"
                fontFamily="monospace"
                textAnchor="middle"
              >
                {jammed ? 'JAMMED' : link.snr}
              </text>
            </g>
          );
        })}

        {/* Render Nodes */}
        {NODES.map((node) => {
          const isSelected = selectedNode === node.id;
          const isInPath = activePath.includes(node.id);
          const isJammed = isUavJammed && node.id === 3;

          let ringColor = isInPath ? '#06b6d4' : '#1e293b';
          if (isJammed) ringColor = '#ef4444';

          return (
            <g
              key={node.id}
              onClick={() => onSelectNode(node.id)}
              className="cursor-pointer group"
              transform={`translate(${node.x}, ${node.y})`}
            >
              {/* Pulse effect if selected or active */}
              {isInPath && (
                <circle
                  r="24"
                  fill="none"
                  stroke={isJammed ? '#ef4444' : '#06b6d4'}
                  strokeWidth="1.5"
                  opacity="0.4"
                  className="animate-ping"
                />
              )}

              {/* Node Circle */}
              <circle
                r="18"
                fill="#081528"
                stroke={isSelected ? '#38bdf8' : ringColor}
                strokeWidth={isSelected ? 3 : 2}
              />

              {/* Node Icon inside */}
              <foreignObject x="-8" y="-8" width="16" height="16">
                <div className="flex items-center justify-center">
                  {getNodeIcon(node.role)}
                </div>
              </foreignObject>

              {/* Node Label */}
              <text
                y="28"
                fill={isSelected ? '#38bdf8' : isInPath ? '#e2e8f0' : '#94a3b8'}
                fontSize="9"
                fontWeight="bold"
                fontFamily="monospace"
                textAnchor="middle"
              >
                {node.callsign}
              </text>
              <text
                y="38"
                fill="#64748b"
                fontSize="7.5"
                fontFamily="monospace"
                textAnchor="middle"
              >
                ID: {node.id} | {node.role}
              </text>
            </g>
          );
        })}
      </svg>

      <div className="w-full flex items-center justify-between text-[11px] font-mono text-slate-400 mt-2 px-2 border-t border-cyan-950 pt-2">
        <span>ACTIVE PATH: [{activePath.join(' → ')}]</span>
        <span className="text-cyan-400">DIJKSTRA MIN-COST ROUTING</span>
      </div>
    </div>
  );
}
