import React, { useEffect, useMemo, useRef, useState } from "react";
import { initializeApp } from "firebase/app";
import {
  getDatabase,
  ref,
  onValue,
  query as rtdbQuery,
  limitToLast,
  orderByChild,
  get,
} from "firebase/database";

import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
  Area,
  AreaChart,
  BarChart,
  Bar,
  PieChart,
  Pie,
  Cell,
  ScatterChart,
  Scatter,
  ZAxis,
} from "recharts";

// =============================================
// FIREBASE CONFIGURATION - CHANGE THESE IF NEEDED
// =============================================
const firebaseConfig = {
  apiKey: "AIzaSyCRZ7WkCXFl2QGLWDlJAHgWVkiARecHQ04",
  authDomain: "smart-study-space-b59cb.firebaseapp.com",
  databaseURL:
    "https://smart-study-space-b59cb-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "smart-study-space-b59cb",
  storageBucket: "smart-study-space-b59cb.firebasestorage.app",
  messagingSenderId: "955323192192",
  appId: "1:955323192192:web:28c0a5505d75d2c6bee765",
};

const app = initializeApp(firebaseConfig);
const rtdb = getDatabase(app);

// =============================================
// YOUR RTDB PATHS (must match ESP32)
// =============================================
const LIVE_PATH = "USMLibrary/Desk01"; // ESP32 live node
const LOG_PATH = "USMLibrary/Desk01Logs"; // ESP32 historical logs (push)

// ---------------------------------------------
// Fake data generator (kept for demo/testing)
// ---------------------------------------------
function makeFakeReading(t) {
  const seatStates = ["AVAILABLE", "RESERVED", "OCCUPIED"];
  const seatStatus = seatStates[Math.floor(Math.random() * seatStates.length)];
  const ldrDO = Math.random() > 0.7 ? 1 : 0; // 1=low light

  const fsrValue =
    seatStatus === "OCCUPIED" ? Math.floor(2800 + Math.random() * 1400) : 0;
  const distance_cm =
    seatStatus === "RESERVED"
      ? Math.floor(10 + Math.random() * 25)
      : seatStatus === "OCCUPIED"
      ? Math.floor(25 + Math.random() * 30)
      : Math.floor(60 + Math.random() * 80);

  const ldrAO = ldrDO === 1 ? Math.floor(100 + Math.random() * 800) : Math.floor(2000 + Math.random() * 1500);

  return {
    seatStatus,
    lightStatus: ldrDO === 1 ? "LOW LIGHT" : "BRIGHT",
    distance_cm,
    fsrValue,
    ldrAO,
    ldrDO,
    updatedAt: t,
  };
}

export default function App() {
  // Tabs
  const [activeTab, setActiveTab] = useState("live"); // 'live' | 'history'

  // Mode toggle
  const [dataMode, setDataMode] = useState("firebase"); // 'fake' | 'firebase'

  // Data
  const [liveData, setLiveData] = useState(null);
  const [historicalData, setHistoricalData] = useState([]);

  // UI state
  const [loading, setLoading] = useState(true);
  const [systemOnline, setSystemOnline] = useState(false);

  // history settings
  const [timeRange, setTimeRange] = useState(60); // minutes

  // timers/unsubs
  const fakeTimerRef = useRef(null);
  const liveUnsubRef = useRef(null);

  const modeLabel = useMemo(() => (dataMode === "fake" ? "Fake Data" : "Firebase (RTDB)"), [dataMode]);

  // ---------------------------------------------
  // LIVE MODE RUNNER: Fake OR Firebase RTDB
  // ---------------------------------------------
  useEffect(() => {
    // cleanup
    if (fakeTimerRef.current) {
      clearInterval(fakeTimerRef.current);
      fakeTimerRef.current = null;
    }
    if (liveUnsubRef.current) {
      try {
        liveUnsubRef.current();
      } catch {}
      liveUnsubRef.current = null;
    }

    setLoading(true);
    setSystemOnline(false);

    // Fake mode
    if (dataMode === "fake") {
      const now = Date.now();
      const first = makeFakeReading(now);
      setLiveData(first);
      setSystemOnline(true);
      setLoading(false);

      fakeTimerRef.current = setInterval(() => {
        const r = makeFakeReading(Date.now());
        setLiveData(r);
        setSystemOnline(true);
      }, 2000);

      return;
    }

    // Firebase RTDB mode
    const liveRef = ref(rtdb, LIVE_PATH);

    const unsub = onValue(
      liveRef,
      (snapshot) => {
        const data = snapshot.val();

        if (data) {
          const updatedAt = Number(data.updatedAt ?? 0);
          const diff = Date.now() - updatedAt;
          //const updatedAt = normalizeTs(data.updatedAt);
          //const diff = Date.now() - updatedAt;
          setSystemOnline(updatedAt > 0 && diff < 15000);


          setSystemOnline(diff < 15000); // online if updated within 15s

          setLiveData({
            ...data,
            updatedAt,
            server_received_at: new Date(),
          });
        } else {
          setLiveData(null);
          setSystemOnline(false);
        }
        setLoading(false);
      },
      (error) => {
        console.error("RTDB live onValue error:", error);
        setLiveData(null);
        setSystemOnline(false);
        setLoading(false);
      }
    );

    // store cleanup function
    liveUnsubRef.current = () => unsub();

    return () => {
      try {
        unsub();
      } catch {}
    };
  }, [dataMode]);

  // ---------------------------------------------
  // HISTORY FETCH (RTDB logs): last N minutes
  // ---------------------------------------------
  useEffect(() => {
    if (dataMode !== "firebase") return;
    if (activeTab !== "history") return;

    const fetchHistory = async () => {
      setLoading(true);

      try {
        const now = Date.now();
        const start = now - timeRange * 60_000;

        // fetch recent logs (limitToLast) then filter by time range client-side
        // NOTE: RTDB cannot do "where timestamp >=" without index rules.
        // We keep it simple for CPC357: read last 500 logs.
        const logsQ = rtdbQuery(ref(rtdb, LOG_PATH), limitToLast(500));
        const snap = await get(logsQ);

        const raw = snap.val() || {};

        const rows = Object.entries(raw)
          .map(([id, v]) => ({ id, ...v }))
          .map((r) => ({
            ...r,
            updatedAt: Number(r.updatedAt ?? 0),
            timeLabel: new Date(Number(r.updatedAt ?? 0)).toLocaleTimeString(),
            // normalize numeric fields
            fsrValue: Number(r.fsrValue ?? 0),
            distance_cm: Number(r.distance_cm ?? 0),
            ldrAO: Number(r.ldrAO ?? 0),
            ldrDO: Number(r.ldrDO ?? 0),
          }))
          .filter((r) => r.updatedAt >= start)
          .sort((a, b) => a.updatedAt - b.updatedAt);

        setHistoricalData(rows);
      } catch (e) {
        console.error("History fetch error:", e);
        setHistoricalData([]);
      } finally {
        setLoading(false);
      }
    };

    fetchHistory();
  }, [activeTab, timeRange, dataMode]);

  return (
    <div className="min-h-screen bg-gray-50">
      {/* Header */}
      <header className="bg-blue-600 text-white shadow-lg">
        <div className="container mx-auto px-4 py-6">
          <div className="flex flex-col md:flex-row md:justify-between md:items-center gap-4">
            <div>
              <h1 className="text-3xl font-bold">🪑 USM Library Monitor</h1>
              <p className="text-blue-100 mt-1">Smart Study Space - Real-time Seat Availability</p>
            </div>

            <div className="flex items-center gap-3">
              {/* Mode Toggle */}
              <button
                onClick={() => setDataMode((m) => (m === "fake" ? "firebase" : "fake"))}
                className="bg-white/20 hover:bg-white/30 transition px-4 py-2 rounded-lg font-semibold"
                title="Switch data source"
              >
                Mode: <span className="underline">{modeLabel}</span>
              </button>

              {/* Online badge */}
              <div className="bg-white text-blue-600 px-4 py-2 rounded-lg shadow">
                <div className="flex items-center gap-2">
                  <div className={`w-3 h-3 rounded-full ${systemOnline ? "bg-green-500" : "bg-red-500"}`}></div>
                  <span className="font-semibold">{systemOnline ? "System Online" : "Offline"}</span>
                </div>
              </div>
            </div>
          </div>

          <p className="text-blue-100 text-sm mt-3">
            Current data source: <b>{modeLabel}</b>
          </p>
        </div>
      </header>

      {/* Tab Navigation */}
      <nav className="bg-blue-100 border-b border-blue-200">
        <div className="container mx-auto px-4">
          <div className="flex gap-2 py-3">
            <button
              onClick={() => setActiveTab("live")}
              className={`px-6 py-2 rounded-t-lg font-semibold transition-colors ${
                activeTab === "live" ? "bg-blue-600 text-white" : "bg-white text-blue-600 hover:bg-blue-50"
              }`}
            >
              📊 Live View
            </button>

            <button
              onClick={() => setActiveTab("history")}
              className={`px-6 py-2 rounded-t-lg font-semibold transition-colors ${
                activeTab === "history" ? "bg-blue-600 text-white" : "bg-white text-blue-600 hover:bg-blue-50"
              }`}
            >
              📈 History & Analytics
            </button>
          </div>
        </div>
      </nav>

      {/* Main Content */}
      <main className="container mx-auto px-4 py-8">
        {loading && (
          <div className="text-center py-20">
            <div className="inline-block animate-spin rounded-full h-16 w-16 border-4 border-blue-600 border-t-transparent"></div>
            <p className="mt-4 text-gray-600">Loading data... ({modeLabel})</p>
          </div>
        )}

        {!loading && activeTab === "live" && <LiveView data={liveData} />}

        {!loading && activeTab === "history" && (
          <HistoryView data={dataMode === "fake" ? [] : historicalData} timeRange={timeRange} setTimeRange={setTimeRange} />
        )}
      </main>

      <footer className="bg-gray-800 text-white mt-20 py-6">
        <div className="container mx-auto px-4 text-center">
          <p>CPC357 IoT Architecture Project - USM</p>
          <p className="text-sm text-gray-400 mt-1">Smart Study Space Monitoring System • Mode: {modeLabel}</p>
        </div>
      </footer>
    </div>
  );
}

function LiveView({ data }) {
  if (!data) {
    return (
      <div className="text-center py-20">
        <p className="text-gray-600 text-lg">No live data available</p>
        <p className="text-sm text-gray-500 mt-2">Waiting for ESP32 readings...</p>
      </div>
    );
  }

  const seatStatus = String(data.seatStatus || "UNKNOWN");
  const lightStatus = String(data.lightStatus || "UNKNOWN");

  const isOccupied = seatStatus === "OCCUPIED";
  const isReserved = seatStatus === "RESERVED";
  const isAvailable = seatStatus === "AVAILABLE";

  const headerColor = isOccupied ? "bg-red-600" : isReserved ? "bg-gray-600" : "bg-green-600";

  const updatedAtMs = Number(data.updatedAt ?? 0);
  const timeSinceUpdate = updatedAtMs ? Math.floor((Date.now() - updatedAtMs) / 1000) : 0;

  return (
    <div className="grid md:grid-cols-2 gap-6">
      {/* Seat Status Card */}
      <div className="bg-white rounded-xl shadow-lg overflow-hidden">
        <div className={`px-6 py-4 ${headerColor} text-white`}>
          <h2 className="text-2xl font-bold">
            {isOccupied
              ? "🔴 SEAT: OCCUPIED"
              : isReserved
              ? "⚪ SEAT: RESERVED"
              : "🟢 SEAT: AVAILABLE"}
          </h2>
        </div>

        <div className="p-6 space-y-4">
          <div>
            <p className="text-sm text-gray-600">Firebase Path</p>
            <p className="text-lg font-semibold">/{LIVE_PATH}</p>
          </div>

          <div className="border-t pt-4">
            <p className="font-semibold mb-3">Live Sensor Values:</p>

            <div className="space-y-2">
              <Row label="👤 Pressure (FSR)" value={`${Number(data.fsrValue ?? 0)}`} />
              <Row label="📏 Distance" value={`${Number(data.distance_cm ?? 0).toFixed(1)} cm`} />
              <Row label="💡 LDR AO" value={`${Number(data.ldrAO ?? 0)}`} />
              <Row label="💡 LDR DO" value={`${Number(data.ldrDO ?? 0)}`} />
            </div>
          </div>

          <div className="border-t pt-4">
            <p className="font-semibold mb-2">System Status:</p>
            <div className="flex justify-between items-center">
              <span className="text-sm">Seat</span>
              <span className={`px-4 py-1 rounded font-semibold text-white ${headerColor}`}>{seatStatus}</span>
            </div>
            <div className="flex justify-between items-center mt-2">
              <span className="text-sm">Light</span>
              <span className={`px-4 py-1 rounded font-semibold text-white ${lightStatus === "LOW LIGHT" ? "bg-yellow-500" : "bg-blue-500"}`}>{lightStatus}</span>
            </div>
          </div>

          <div className="border-t pt-4 text-sm text-gray-600">
            <p>
              Last updated: <span className="font-semibold">{timeSinceUpdate} seconds ago</span>
            </p>
            <p className="mt-1">{updatedAtMs ? new Date(updatedAtMs).toLocaleString() : "-"}</p>
          </div>
        </div>
      </div>

      {/* Lighting Info Card */}
      <div className="bg-white rounded-xl shadow-lg overflow-hidden">
        <div className={`px-6 py-4 ${lightStatus === "LOW LIGHT" ? "bg-yellow-600" : "bg-blue-600"} text-white`}>
          <h2 className="text-2xl font-bold">💡 LIGHT: {lightStatus}</h2>
        </div>

        <div className="p-6 space-y-4">
          <div>
            <p className="text-sm text-gray-600 mb-2">Analog Light Value (LDR AO)</p>
            <div className="bg-gray-50 border-2 border-gray-200 rounded-lg p-6 text-center">
              <p className="text-5xl font-bold text-gray-800">{Number(data.ldrAO ?? 0)}</p>
              <p className="text-xl text-gray-600 mt-2">ADC</p>
            </div>
          </div>

          <div className="bg-gray-50 rounded-lg p-4">
            <p className="text-xs text-gray-600 mb-2">Interpretation</p>
            <div className="space-y-1 text-sm">
              <p><span className="text-yellow-600 font-semibold">●</span> LOW LIGHT: DO = 1</p>
              <p><span className="text-blue-600 font-semibold">●</span> BRIGHT: DO = 0</p>
            </div>
            <p className="text-xs text-gray-500 italic mt-2">(Based on your module default setting)</p>
          </div>
        </div>
      </div>

      {/* System Health */}
      <div className="md:col-span-2 bg-green-50 border-2 border-green-500 rounded-xl p-6">
        <div className="flex items-center gap-4">
          <div className="w-4 h-4 bg-green-500 rounded-full animate-pulse"></div>
          <div>
            <p className="text-lg font-bold text-green-800">System Status: Online & Transmitting</p>
            <p className="text-sm text-gray-600">Realtime Database → Live node updates every ~2 seconds</p>
          </div>
        </div>
      </div>
    </div>
  );
}

function Row({ label, value }) {
  return (
    <div className="flex justify-between items-center">
      <span className="text-sm">{label}:</span>
      <span className="px-3 py-1 rounded font-semibold bg-gray-100 text-gray-800">{value}</span>
    </div>
  );
}

function HistoryView({ data, timeRange, setTimeRange }) {
  const timeRangeOptions = [
    { value: 10, label: "Last 10 min" },
    { value: 60, label: "Last 1 hour" },
    { value: 360, label: "Last 6 hours" },
    { value: 1440, label: "Last 24 hours" },
  ];

  const chartData = (data || []).map((r) => ({
    time: r.timeLabel,
    fsrValue: Number(r.fsrValue ?? 0),
    distance_cm: Number(r.distance_cm ?? 0),
    ldrAO: Number(r.ldrAO ?? 0),
    ldrDO: Number(r.ldrDO ?? 0),
    seatStatus: String(r.seatStatus || "UNKNOWN"),
    seatNumeric:
      String(r.seatStatus || "").toUpperCase() === "OCCUPIED"
        ? 2
        : String(r.seatStatus || "").toUpperCase() === "RESERVED"
        ? 1
        : 0,
  }));

  const busyCount = (data || []).filter((d) => String(d.seatStatus).toUpperCase() === "OCCUPIED").length;
  const reservedCount = (data || []).filter((d) => String(d.seatStatus).toUpperCase() === "RESERVED").length;
  const availableCount = Math.max(0, (data || []).length - busyCount - reservedCount);

  const seatPie = [
    { name: "Occupied", value: busyCount },
    { name: "Reserved", value: reservedCount },
    { name: "Available", value: availableCount },
  ];
  const pieColors = ["#ef4444", "#6b7280", "#22c55e"];

  // Scatter: Pressure vs Distance, dot size by LDR AO
  const scatterAll = (data || []).map((d) => ({
    pressure: Number(d.fsrValue ?? 0),
    distance: Number(d.distance_cm ?? 0),
    ldrAO: Number(d.ldrAO ?? 0),
    status: String(d.seatStatus || "UNKNOWN"),
  }));

  const scatterOcc = scatterAll.filter((x) => x.status.toUpperCase() === "OCCUPIED");
  const scatterRes = scatterAll.filter((x) => x.status.toUpperCase() === "RESERVED");
  const scatterAvail = scatterAll.filter((x) => x.status.toUpperCase() === "AVAILABLE");

  return (
    <div className="space-y-6">
      {/* Time Range Selector */}
      <div className="bg-white rounded-xl shadow p-4">
        <div className="flex items-center justify-between gap-3 flex-wrap">
          <div>
            <p className="font-semibold">Select Time Range:</p>
            <p className="text-sm text-gray-500">Realtime DB logs (last 500 points fetched)</p>
          </div>
          <p className="text-sm text-gray-600">
            Showing <b>{(data || []).length}</b> points
          </p>
        </div>

        <div className="flex gap-3 flex-wrap mt-3">
          {timeRangeOptions.map((option) => (
            <button
              key={option.value}
              onClick={() => setTimeRange(option.value)}
              className={`px-6 py-2 rounded-lg font-semibold transition-colors ${
                timeRange === option.value
                  ? "bg-blue-600 text-white"
                  : "bg-gray-100 text-blue-600 hover:bg-blue-50"
              }`}
            >
              {option.label}
            </button>
          ))}
        </div>
      </div>

      {/* Quick Summary */}
      <div className="grid md:grid-cols-3 gap-4">
        <div className="bg-white rounded-xl shadow p-5">
          <p className="text-sm text-gray-500">Occupied Count</p>
          <p className="text-3xl font-bold mt-1">{busyCount}</p>
        </div>
        <div className="bg-white rounded-xl shadow p-5">
          <p className="text-sm text-gray-500">Reserved Count</p>
          <p className="text-3xl font-bold mt-1">{reservedCount}</p>
        </div>
        <div className="bg-white rounded-xl shadow p-5">
          <p className="text-sm text-gray-500">Available Count</p>
          <p className="text-3xl font-bold mt-1">{availableCount}</p>
        </div>
      </div>

      {/* Seat Status Timeline */}
      <div className="bg-white rounded-xl shadow-lg p-6">
        <h3 className="text-xl font-bold mb-4">Seat Status Over Time</h3>
        <ResponsiveContainer width="100%" height={300}>
          <AreaChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis dataKey="time" />
            <YAxis domain={[0, 2]} ticks={[0, 1, 2]} />
            <Tooltip />
            <Legend />
            <Area type="stepAfter" dataKey="seatNumeric" stroke="#2563eb" fill="#93c5fd" name="0=Avail,1=Res,2=Occ" />
          </AreaChart>
        </ResponsiveContainer>
      </div>

      {/* FSR */}
      <div className="bg-white rounded-xl shadow-lg p-6">
        <h3 className="text-xl font-bold mb-4">FSR Pressure Over Time</h3>
        <ResponsiveContainer width="100%" height={300}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis dataKey="time" />
            <YAxis />
            <Tooltip />
            <Legend />
            <Line type="monotone" dataKey="fsrValue" stroke="#ef4444" strokeWidth={2} dot={false} name="FSR" />
          </LineChart>
        </ResponsiveContainer>
      </div>

      {/* Distance */}
      <div className="bg-white rounded-xl shadow-lg p-6">
        <h3 className="text-xl font-bold mb-4">Ultrasonic Distance (cm) Over Time</h3>
        <ResponsiveContainer width="100%" height={300}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis dataKey="time" />
            <YAxis />
            <Tooltip />
            <Legend />
            <Line type="monotone" dataKey="distance_cm" stroke="#22c55e" strokeWidth={2} dot={false} name="Distance cm" />
          </LineChart>
        </ResponsiveContainer>
      </div>

      {/* Donut Pie */}
      <div className="bg-white rounded-xl shadow-lg p-6">
        <h3 className="text-xl font-bold mb-4">Seat Status Distribution</h3>
        <ResponsiveContainer width="100%" height={260}>
          <PieChart>
            <Pie data={seatPie} dataKey="value" nameKey="name" cx="50%" cy="50%" innerRadius={60} outerRadius={95} paddingAngle={2}>
              {seatPie.map((_, idx) => (
                <Cell key={`cell-${idx}`} fill={pieColors[idx]} />
              ))}
            </Pie>
            <Tooltip />
            <Legend />
          </PieChart>
        </ResponsiveContainer>
      </div>

      {/* Scatter */}
      <div className="bg-white rounded-xl shadow-lg p-6">
        <h3 className="text-xl font-bold mb-2">Pressure vs Distance (Scatter)</h3>
        <p className="text-sm text-gray-600 mb-3">Dot size = LDR AO</p>
        <ResponsiveContainer width="100%" height={320}>
          <ScatterChart>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis type="number" dataKey="pressure" name="FSR" />
            <YAxis type="number" dataKey="distance" name="Distance" />
            <ZAxis type="number" dataKey="ldrAO" range={[60, 220]} name="LDR AO" />
            <Tooltip />
            <Legend />
            <Scatter name="Occupied" data={scatterOcc} fill="#ef4444" />
            <Scatter name="Reserved" data={scatterRes} fill="#6b7280" />
            <Scatter name="Available" data={scatterAvail} fill="#22c55e" />
          </ScatterChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
}
