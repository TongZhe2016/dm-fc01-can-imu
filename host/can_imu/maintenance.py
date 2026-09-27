"""Shared maintenance lock for standalone and parent-workspace use."""
import os
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]

def hardware_lock():
    override = os.environ.get("CAN_IMU_HARDWARE_LOCK")
    if override:
        path = Path(override)
    else:
        workspace = next((p for p in ROOT.parents if (p / "real_drone/aerial_arm_driver").is_dir()), None)
        path = workspace / ".omx/state/hardware-can.lock" if workspace else Path(os.environ.get("XDG_STATE_HOME", str(Path.home()/".local/state"))) / "dm-fc01-can-imu/hardware.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    return path
