'use client';

import React, { useState, useEffect } from 'react';
import MeshNetworkMap from '../components/MeshNetworkMap';
import ProtocolInspector from '../components/ProtocolInspector';
import { Radio, Play, Pause, RotateCcw, Activity, ShieldCheck, Zap, Server, Network } from 'lucide-react';

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

interface TelemetryRoot {
  events: EventData[];
}

export default function Home() {
  const [telemetry, setTelemetry] = useState<TelemetryRoot | null>(null);
  const [currentStepIdx, setCurrentStepIdx] = useState(0);
  const [isPlaying, setIsPlaying] = useState(true);
  const [selectedNode, setSelectedNode] = useState<number | null>(null);
  const [isJammingActive, setIsJammingActive] = useState(false);
  const [playbackSpeed, setPlaybackSpeed] = useState<number>(1);

  useEffect(() => {
    fetch('/tactlink_telemetry.json')
      .then((res) => res.json())
      .then((data: TelemetryRoot) => {
        setTelemetry(data);
      })
      .catch((err) => console.error('Failed to load tactlink telemetry:', err));
  }, []);

  useEffect(() => {
    if (!isPlaying || !telemetry) return;
    const interval = setInterval(() => {
      setCurrentStepIdx((prev) => {
        const next = prev + 1;
        return next >= telemetry.events.length ? 0 : next;
      });
    }, 1200 / playbackSpeed);

    return () => clearInterval(interval);
  }, [isPlaying, telemetry, playbackSpeed]);

  const currentEvent: EventData | null =
    telemetry && telemetry.events[currentStepIdx] ? telemetry.events[currentStepIdx] : null;

  // Auto-sync jamming state from recorded step, or allow manual override
  useEffect(() => {
    if (currentEvent) {
      setIsJammingActive(currentEvent.uav_jammed);
    }
  }, [currentStepIdx]);

  const activePath = currentEvent
    ? isJammingActive
      ? [1, 2, 4] // Jammed failover path
      : [1, 3, 4] // Optimal nominal path via UAV
    : [1, 3, 4];

  return (
    <main className="min-h-screen bg-[#020713] text-slate-100 flex flex-col p-4 md:p-6 lg:p-8 font-mono">
      {/* Header */}
      <header className="flex flex-col md:flex-row md:items-center justify-between border-b border-cyan-900/60 pb-4 mb-6 gap-4">
        <div className="flex items-center gap-3">
          <div className="p-2 rounded bg-cyan-950 border border-cyan-500/40 text-cyan-400">
            <Radio className="w-6 h-6" />
          </div>
          <div>
            <h1 className="text-xl md:text-2xl font-bold tracking-wider text-cyan-400">
              TACTLINK-SDR // TACTICAL DATA LINK PROTOCOL
            </h1>
            <p className="text-xs text-cyan-600">
              RESILIENT AD-HOC MESH ROUTING &amp; SELECTIVE REPEAT ARQ // MIL-STD-188-220 SPEC [C++14]
            </p>
          </div>
        </div>

        <div className="flex items-center gap-3 text-xs">
          <div className="flex items-center gap-2 bg-cyan-950/80 px-3 py-1.5 rounded border border-cyan-800">
            <span className="w-2 h-2 rounded-full bg-cyan-400 animate-pulse"></span>
            <span className="text-cyan-300">SDR PHY LINK UP</span>
          </div>
          <div className="bg-slate-900 px-3 py-1.5 rounded border border-slate-700 text-slate-300">
            BEL DEFENCE ELECTRONICS
          </div>
        </div>
      </header>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 flex-1">
        {/* Left Column: Network Map & Playback (7 cols) */}
        <div className="lg:col-span-7 flex flex-col gap-4">
          <MeshNetworkMap
            activePath={activePath}
            isUavJammed={isJammingActive}
            selectedNode={selectedNode}
            onSelectNode={setSelectedNode}
          />

          {/* Timeline & Scrubber */}
          <div className="bg-[#040e1e] border border-cyan-900/60 rounded-xl p-3 shadow-lg">
            <div className="flex items-center justify-between text-xs text-cyan-400 mb-2">
              <div className="flex items-center gap-2">
                <button
                  onClick={() => setIsPlaying(!isPlaying)}
                  className="p-1.5 rounded bg-cyan-600 hover:bg-cyan-500 text-black font-bold transition flex items-center gap-1"
                >
                  {isPlaying ? <Pause className="w-3.5 h-3.5" /> : <Play className="w-3.5 h-3.5" />}
                  <span>{isPlaying ? 'PAUSE' : 'RESUME'}</span>
                </button>
                <button
                  onClick={() => {
                    setCurrentStepIdx(0);
                    setIsPlaying(true);
                  }}
                  className="p-1.5 rounded bg-cyan-950 hover:bg-cyan-900 border border-cyan-800 text-cyan-300 transition"
                  title="Rewind"
                >
                  <RotateCcw className="w-3.5 h-3.5" />
                </button>
                <div className="flex items-center gap-1 ml-2">
                  {[1, 2, 5].map((spd) => (
                    <button
                      key={spd}
                      onClick={() => setPlaybackSpeed(spd)}
                      className={`px-2 py-0.5 text-[10px] rounded border ${
                        playbackSpeed === spd
                          ? 'bg-cyan-500 text-black border-cyan-400 font-bold'
                          : 'bg-cyan-950 text-cyan-400 border-cyan-900'
                      }`}
                    >
                      {spd}x
                    </button>
                  ))}
                </div>
              </div>

              <span>
                STEP: {currentStepIdx + 1} / {telemetry ? telemetry.events.length : 0} | SEQ #{' '}
                {currentEvent ? currentEvent.seq_num : 0}
              </span>
            </div>

            <input
              type="range"
              min={0}
              max={telemetry ? telemetry.events.length - 1 : 0}
              value={currentStepIdx}
              onChange={(e) => {
                setCurrentStepIdx(Number(e.target.value));
                setIsPlaying(false);
              }}
              className="w-full accent-cyan-500 cursor-pointer h-1.5 bg-cyan-950 rounded-lg"
            />
          </div>
        </div>

        {/* Right Column: Metrics, Inspector, Table (5 cols) */}
        <div className="lg:col-span-5 flex flex-col gap-4">
          {/* Metrics HUD */}
          <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
            <div className="bg-cyan-950/30 border border-cyan-900/60 p-2.5 rounded-lg">
              <div className="text-[10px] text-cyan-500 uppercase">Delivery Rate</div>
              <div className="text-xl font-bold text-emerald-400 mt-0.5">100%</div>
            </div>
            <div className="bg-cyan-950/30 border border-cyan-900/60 p-2.5 rounded-lg">
              <div className="text-[10px] text-cyan-500 uppercase">CRC Blocks</div>
              <div className="text-xl font-bold text-rose-400 mt-0.5">2 Bad</div>
            </div>
            <div className="bg-cyan-950/30 border border-cyan-900/60 p-2.5 rounded-lg">
              <div className="text-[10px] text-cyan-500 uppercase">ARQ Retries</div>
              <div className="text-xl font-bold text-amber-400 mt-0.5">5 Pkts</div>
            </div>
            <div className="bg-cyan-950/30 border border-cyan-900/60 p-2.5 rounded-lg">
              <div className="text-[10px] text-cyan-500 uppercase">Routing Engine</div>
              <div className="text-xl font-bold text-cyan-300 mt-0.5">
                &lt;1 <span className="text-xs">ms</span>
              </div>
            </div>
          </div>

          {/* Frame Inspector */}
          <ProtocolInspector
            currentEvent={currentEvent}
            onToggleJamming={() => setIsJammingActive(!isJammingActive)}
            isJammingActive={isJammingActive}
          />

          {/* Tactical Event Stream Table */}
          <div className="bg-[#051122] border border-cyan-900/60 rounded-xl p-4 flex flex-col flex-1 overflow-hidden">
            <h3 className="text-xs font-bold text-cyan-400 mb-2 flex items-center gap-1.5">
              <Network className="w-3.5 h-3.5" /> LIVE SDR PACKET TELEMETRY
            </h3>
            <div className="overflow-x-auto overflow-y-auto max-h-48 scrollbar-thin">
              <table className="w-full text-left text-xs">
                <thead>
                  <tr className="border-b border-cyan-900/60 text-[10px] text-cyan-500">
                    <th className="pb-1.5">STEP</th>
                    <th className="pb-1.5">PRIORITY</th>
                    <th className="pb-1.5">HOPS</th>
                    <th className="pb-1.5">COST</th>
                    <th className="pb-1.5 text-right">STATUS</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-cyan-950">
                  {telemetry &&
                    telemetry.events.slice(0, currentStepIdx + 1).reverse().map((ev) => (
                      <tr key={ev.step} className="hover:bg-cyan-950/30 transition">
                        <td className="py-1.5 text-slate-400">#{ev.step}</td>
                        <td className="py-1.5">
                          <span
                            className={`px-1.5 py-0.5 text-[9px] rounded font-bold ${
                              ev.priority === 'EMERGENCY'
                                ? 'bg-rose-950 text-rose-400'
                                : ev.priority === 'TACTICAL'
                                ? 'bg-amber-950 text-amber-400'
                                : 'bg-cyan-950 text-cyan-400'
                            }`}
                          >
                            {ev.priority}
                          </span>
                        </td>
                        <td className="py-1.5 text-slate-300">[{ev.path.join('→')}]</td>
                        <td className="py-1.5 text-slate-400">{ev.route_cost.toFixed(1)}</td>
                        <td className="py-1.5 text-right">
                          <span
                            className={`text-[10px] ${
                              ev.status === 'DELIVERED'
                                ? 'text-emerald-400'
                                : ev.status === 'CRC_ERROR_REJECTED'
                                ? 'text-rose-400'
                                : 'text-amber-400'
                            }`}
                          >
                            {ev.status}
                          </span>
                        </td>
                      </tr>
                    ))}
                </tbody>
              </table>
            </div>
          </div>

          {/* Badges */}
          <div className="bg-cyan-950/20 border border-cyan-900/40 rounded-lg p-2.5 text-[11px] text-cyan-500 flex flex-wrap gap-2">
            <span className="bg-cyan-950 px-2 py-0.5 rounded border border-cyan-800">
              Selective Repeat ARQ
            </span>
            <span className="bg-cyan-950 px-2 py-0.5 rounded border border-cyan-800">
              Dijkstra Shortest Path
            </span>
            <span className="bg-cyan-950 px-2 py-0.5 rounded border border-cyan-800">
              CRC-32 IEEE 802.3
            </span>
            <span className="bg-cyan-950 px-2 py-0.5 rounded border border-cyan-800">
              Strict Multilevel QoS
            </span>
          </div>
        </div>
      </div>
    </main>
  );
}
