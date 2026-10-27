'use client';

import React from 'react';
import { CheckCircle2, XCircle, ArrowRight, ShieldAlert, Cpu, Hash } from 'lucide-react';

interface EventData {
  step: number;
  seq_num: number;
  priority: string;
  src: number;
  dst: number;
  path: number[];
  route_cost: number;
  status: string;
  bytes: number;
  uav_jammed: boolean;
}

interface ProtocolInspectorProps {
  currentEvent: EventData | null;
  onToggleJamming: () => void;
  isJammingActive: boolean;
}

export default function ProtocolInspector({
  currentEvent,
  onToggleJamming,
  isJammingActive,
}: ProtocolInspectorProps) {
  if (!currentEvent) {
    return <div className="text-xs font-mono text-slate-500">Awaiting telemetry...</div>;
  }

  const isDelivered = currentEvent.status === 'DELIVERED';
  const isCrcError = currentEvent.status === 'CRC_ERROR_REJECTED';
  const isRetransmit = currentEvent.status === 'RF_DROP_RETRANSMIT';

  return (
    <div className="flex flex-col gap-4 bg-[#061224] border border-cyan-900/60 rounded-xl p-4 shadow-xl">
      {/* Header with Jamming Injection Toggle */}
      <div className="flex items-center justify-between border-b border-cyan-900/40 pb-2">
        <div className="flex items-center gap-2">
          <Cpu className="w-4 h-4 text-cyan-400" />
          <h2 className="text-xs font-bold font-mono text-cyan-300">
            FRAME WIRE DISSECTOR // MIL-STD-188-220
          </h2>
        </div>
        <button
          onClick={onToggleJamming}
          className={`px-2.5 py-1 text-[10px] font-mono font-bold rounded flex items-center gap-1.5 transition ${
            isJammingActive
              ? 'bg-rose-950 text-rose-300 border border-rose-700 hover:bg-rose-900'
              : 'bg-cyan-950 text-cyan-300 border border-cyan-800 hover:bg-cyan-900'
          }`}
        >
          <ShieldAlert className="w-3.5 h-3.5" />
          {isJammingActive ? 'CLEAR EW JAMMING' : 'INJECT EW JAMMING'}
        </button>
      </div>

      {/* Binary Packet Fields Visualizer */}
      <div className="flex flex-wrap gap-1.5 font-mono text-[10px]">
        <div className="bg-slate-900 border border-slate-700 p-2 rounded flex-1 min-w-[70px]">
          <div className="text-slate-500 text-[8px]">PREAMBLE</div>
          <div className="text-cyan-400 font-bold">0xAA55</div>
        </div>
        <div className="bg-slate-900 border border-slate-700 p-2 rounded flex-1 min-w-[60px]">
          <div className="text-slate-500 text-[8px]">VER</div>
          <div className="text-slate-300">0x01</div>
        </div>
        <div className="bg-slate-900 border border-slate-700 p-2 rounded flex-1 min-w-[90px]">
          <div className="text-slate-500 text-[8px]">PRIORITY QoS</div>
          <div
            className={`font-bold ${
              currentEvent.priority === 'EMERGENCY'
                ? 'text-rose-400'
                : currentEvent.priority === 'TACTICAL'
                ? 'text-amber-400'
                : 'text-cyan-400'
            }`}
          >
            {currentEvent.priority}
          </div>
        </div>
        <div className="bg-slate-900 border border-slate-700 p-2 rounded flex-1 min-w-[80px]">
          <div className="text-slate-500 text-[8px]">SRC → DST</div>
          <div className="text-slate-300 font-bold">
            N{currentEvent.src} → N{currentEvent.dst}
          </div>
        </div>
        <div className="bg-slate-900 border border-slate-700 p-2 rounded flex-1 min-w-[70px]">
          <div className="text-slate-500 text-[8px]">SEQ NUM</div>
          <div className="text-emerald-400 font-bold">#{currentEvent.seq_num}</div>
        </div>
        <div className="bg-slate-900 border border-slate-700 p-2 rounded flex-1 min-w-[80px]">
          <div className="text-slate-500 text-[8px]">FRAME SIZE</div>
          <div className="text-slate-300">{currentEvent.bytes} bytes</div>
        </div>
      </div>

      {/* Frame Status Alert */}
      <div
        className={`p-3 rounded-lg border flex items-center justify-between text-xs font-mono ${
          isDelivered
            ? 'bg-emerald-950/40 border-emerald-800 text-emerald-300'
            : isCrcError
            ? 'bg-rose-950/60 border-rose-800 text-rose-300'
            : 'bg-amber-950/40 border-amber-800 text-amber-300'
        }`}
      >
        <div className="flex items-center gap-2">
          {isDelivered && <CheckCircle2 className="w-4 h-4 text-emerald-400" />}
          {isCrcError && <XCircle className="w-4 h-4 text-rose-400" />}
          {isRetransmit && <Hash className="w-4 h-4 text-amber-400" />}
          <div>
            <div className="font-bold">STATUS: {currentEvent.status}</div>
            <div className="text-[10px] opacity-80">
              {isDelivered
                ? 'CRC-32 Validated // Selective ACK Dispatched // In-Order Delivery'
                : isCrcError
                ? 'Bit-Flip Detected via IEEE 802.3 Polynomial // Frame Discarded'
                : 'RF Loss Simulated // Selective Repeat ARQ Triggered'}
            </div>
          </div>
        </div>
        <div className="text-right text-[10px]">
          <div>ROUTE COST</div>
          <div className="font-bold text-cyan-300">{currentEvent.route_cost.toFixed(2)}</div>
        </div>
      </div>

      {/* ARQ & Mesh Routing Path Summary */}
      <div className="bg-slate-950 p-3 rounded-lg border border-cyan-950 text-xs font-mono">
        <div className="text-slate-400 text-[10px] mb-1">DYNAMIC ROUTING PATH &amp; HOPS</div>
        <div className="flex items-center gap-1.5 text-cyan-300 font-bold flex-wrap">
          {currentEvent.path.map((nodeId, idx) => (
            <React.Fragment key={idx}>
              <span className="bg-cyan-950 px-2 py-0.5 rounded border border-cyan-800">
                Node {nodeId}
              </span>
              {idx + 1 < currentEvent.path.length && (
                <ArrowRight className="w-3.5 h-3.5 text-slate-500" />
              )}
            </React.Fragment>
          ))}
        </div>
      </div>
    </div>
  );
}
