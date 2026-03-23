import { useState, useEffect, useRef, useCallback } from 'react'
import './App.css'

function useWebSocket(url) {
  const [state, setState] = useState(null)
  const [connected, setConnected] = useState(false)
  const wsRef = useRef(null)
  const reconnectTimer = useRef(null)

  const connect = useCallback(() => {
    try {
      const ws = new WebSocket(url)
      wsRef.current = ws

      ws.onopen = () => setConnected(true)
      ws.onclose = () => {
        setConnected(false)
        reconnectTimer.current = setTimeout(connect, 3000)
      }
      ws.onerror = () => ws.close()
      ws.onmessage = (e) => {
        try {
          setState(JSON.parse(e.data))
        } catch {}
      }
    } catch {
      reconnectTimer.current = setTimeout(connect, 3000)
    }
  }, [url])

  useEffect(() => {
    // Fetch initial state via HTTP to reflect status immediately
    fetch('/api/state')
      .then(res => res.json())
      .then(data => {
        if (data && data.last_update) {
          setState(data)
        }
      })
      .catch(console.error)

    connect()
    return () => {
      clearTimeout(reconnectTimer.current)
      wsRef.current?.close()
    }
  }, [connect])

  return { state, connected }
}

function formatUptime(seconds) {
  if (!seconds) return '0s'
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  const s = seconds % 60
  if (h > 0) return `${h}h ${m}m ${s}s`
  if (m > 0) return `${m}m ${s}s`
  return `${s}s`
}

function formatTime(iso) {
  if (!iso) return 'Never'
  const d = new Date(iso)
  return d.toLocaleTimeString()
}

function StatusCard({ label, icon, value, isOn, colorClass, detail }) {
  return (
    <div className={`card ${colorClass}`}>
      <div className="card-header">
        <span className="card-label">{label}</span>
        <span className="card-icon">{icon}</span>
      </div>
      <div className={`card-value ${isOn !== undefined ? (isOn ? 'on' : 'off') : ''}`}>
        {value}
      </div>
      {detail && <div className="card-detail">{detail}</div>}
    </div>
  )
}

function BrightnessCard({ brightness }) {
  const pct = Math.round((brightness / 255) * 100)
  return (
    <div className="card blue">
      <div className="card-header">
        <span className="card-label">Brightness</span>
        <span className="card-icon">☀️</span>
      </div>
      <div className="card-value">{pct}%</div>
      <div className="progress-bar-container">
        <div className="progress-bar-bg">
          <div className="progress-bar-fill" style={{ width: `${pct}%` }} />
        </div>
      </div>
    </div>
  )
}

export default function App() {
  const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  const wsUrl = `${wsProtocol}//${window.location.host}/ws`
  const { state, connected } = useWebSocket(wsUrl)

  const hasData = state && state.last_update

  return (
    <div className="app">
      <header className="header">
        <h1>⚡ Arduino Dashboard</h1>
        <p className="subtitle">ESP8266 Real-Time Monitor</p>
        <div className={`connection-badge ${connected ? 'connected' : 'disconnected'}`}>
          <span className="dot" />
          {connected ? 'WebSocket Connected' : 'Reconnecting...'}
        </div>
      </header>

      {!hasData ? (
        <div className="waiting">
          <div className="loader" />
          <p>Waiting for Arduino data…</p>
          <p className="hint">Make sure the ESP8266 is powered on and connected to WiFi</p>
        </div>
      ) : (
        <>
          <div className="dashboard-grid">
            <StatusCard
              label="Light"
              icon="💡"
              value={state.light ? 'ON' : 'OFF'}
              isOn={state.light}
              colorClass={state.light ? 'green' : 'red'}
            />
            <StatusCard
              label="Fan"
              icon="🌀"
              value={state.fan ? 'ON' : 'OFF'}
              isOn={state.fan}
              colorClass={state.fan ? 'green' : 'red'}
            />
            <BrightnessCard brightness={state.brightness} />
            <StatusCard
              label="Screen Timer"
              icon="⏱️"
              value={state.timer_label}
              colorClass="orange"
              detail={state.timer_index === 0 ? 'Always on' : 'Auto sleep active'}
            />
            <StatusCard
              label="Invert Display"
              icon="🔲"
              value={state.invert ? 'ON' : 'OFF'}
              isOn={state.invert}
              colorClass="purple"
            />
            <StatusCard
              label="WiFi Signal"
              icon="📶"
              value={state.wifi_rssi ? `${state.wifi_rssi} dBm` : 'N/A'}
              colorClass="cyan"
              detail={state.wifi_rssi ? (
                state.wifi_rssi > -50 ? 'Excellent' :
                state.wifi_rssi > -70 ? 'Good' :
                state.wifi_rssi > -80 ? 'Fair' : 'Weak'
              ) : undefined}
            />
          </div>

          <footer className="footer">
            <p className="last-update">Last update: {formatTime(state.last_update)}</p>
            <p className="uptime">Device uptime: {formatUptime(state.uptime_sec)}</p>
          </footer>
        </>
      )}
    </div>
  )
}
