import json
import asyncio
from datetime import datetime
from pathlib import Path
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pydantic import BaseModel
from typing import Optional

app = FastAPI(title="Arduino Dashboard")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- State model ---

class DeviceState(BaseModel):
    light: bool = False
    fan: bool = False
    brightness: int = 255
    timer_index: int = 0
    timer_label: str = "Off"
    invert: bool = False
    uptime_sec: int = 0
    wifi_rssi: Optional[int] = None
    last_update: Optional[str] = None

STATE_FILE = Path(__file__).parent / "state.json"

# Current state in memory
current_state = DeviceState()

# Load persisted state if exists
if STATE_FILE.exists():
    try:
        data = json.loads(STATE_FILE.read_text())
        current_state = DeviceState(**data)
    except Exception as e:
        print(f"Error loading state.json: {e}")

# --- WebSocket manager ---

class ConnectionManager:
    def __init__(self):
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket):
        self.active.remove(ws)

    async def broadcast(self, data: dict):
        dead = []
        for ws in self.active:
            try:
                await ws.send_json(data)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.active.remove(ws)

manager = ConnectionManager()

# --- API endpoints ---

@app.post("/api/state")
async def update_state(state: DeviceState):
    global current_state
    state.last_update = datetime.now().isoformat()
    current_state = state
    
    # Persist to disk
    try:
        STATE_FILE.write_text(current_state.model_dump_json(indent=2))
    except Exception as e:
        print(f"Error saving state.json: {e}")
        
    await manager.broadcast(state.model_dump())
    return {"status": "ok"}

@app.get("/api/state")
async def get_state():
    return current_state.model_dump()

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    # Send current state on connect
    try:
        await ws.send_json(current_state.model_dump())
        while True:
            # Keep alive / receive pings
            await ws.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(ws)
    except Exception:
        manager.disconnect(ws)

# --- Serve React frontend ---

frontend_dist = Path(__file__).parent / "frontend" / "dist"

@app.get("/")
async def serve_root():
    return FileResponse(frontend_dist / "index.html")

# Mount static assets (must be after the root route)
if frontend_dist.exists():
    app.mount("/", StaticFiles(directory=str(frontend_dist), html=True), name="frontend")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=4444)
