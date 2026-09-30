"""
Phase 1 camera calibration for Free-Viewpoint RGB-D Video Dataset (camera01).
Loads intrinsics and extrinsics from Camera Parameters/paras.txt.
"""
from pathlib import Path
import numpy as np


def load_paras_txt(paras_path: Path) -> list[dict]:
    """
    Parse paras.txt (5 lines per camera).
    Returns list of dicts: resolution (w, h), K_matrix (fx, fy, cx, cy),
    R_matrix (3x3), world_position (3,).
    """
    path = Path(paras_path)
    text = path.read_text()
    cameras = []
    lines = text.strip().splitlines()
    i = 0
    while i < len(lines):
        if not lines[i].strip().startswith("camera_id"):
            i += 1
            continue
        # camera_id N
        cam_id = int(lines[i].split()[1])
        i += 1
        if i >= len(lines):
            break
        # resolution W H
        res = lines[i].split()
        w, h = int(res[1]), int(res[2])
        i += 1
        if i >= len(lines):
            break
        # K_matrix fx fy cx cy
        k_vals = [float(x) for x in lines[i].split()[1:5]]
        fx, fy, cx, cy = k_vals
        i += 1
        if i >= len(lines):
            break
        # R_matrix 9 values row-major
        r_vals = [float(x) for x in lines[i].split()[1:10]]
        R = np.array(r_vals, dtype=np.float64).reshape(3, 3)
        i += 1
        if i >= len(lines):
            break
        # world_position tx ty tz
        t_vals = [float(x) for x in lines[i].split()[1:4]]
        t = np.array(t_vals, dtype=np.float64)
        i += 1

        K = np.array([
            [fx, 0, cx],
            [0, fy, cy],
            [0, 0, 1],
        ], dtype=np.float64)
        cameras.append({
            "camera_id": cam_id,
            "width": w,
            "height": h,
            "K": K,
            "R": R,
            "t": t,
        })
    return cameras


def get_camera(cameras: list[dict], camera_index: int = 0) -> dict:
    """Get camera dict for given index (0 = camera01)."""
    if camera_index < 0 or camera_index >= len(cameras):
        raise IndexError(f"camera_index must be 0..{len(cameras) - 1}")
    return cameras[camera_index]


def get_intrinsics(camera: dict) -> tuple[np.ndarray, int, int]:
    """Return (K 3x3, width, height)."""
    return camera["K"], camera["width"], camera["height"]


def project_world_to_image(K: np.ndarray, R: np.ndarray, t: np.ndarray, point_3d: np.ndarray) -> np.ndarray:
    """
    Project world point Xw to image: Xp = K @ (R @ (Xw - t)).
    point_3d: (3,) or (N, 3). Returns (2,) or (N, 2) pixel coords.
    """
    p = np.asarray(point_3d, dtype=np.float64)
    if p.ndim == 1:
        p = p.reshape(1, 3)
    # Xc = R @ (Xw - t)
    Xc = (R @ (p - t).T).T  # (N, 3)
    # Xp = K @ Xc
    Xp = (K @ Xc.T).T  # (N, 3)
    uv = Xp[:, :2] / (Xp[:, 2:3] + 1e-10)
    return uv if uv.shape[0] > 1 else uv.ravel()
