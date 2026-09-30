"""
Depth conversion and ROI depth for Free-Viewpoint RGB-D Video Dataset.
Grayscale depth frames (0–255) -> metric depth (meters).
"""
import numpy as np

# Dataset constants (from dataset README)
FB = 32504.0
MIN_DEPTH = 40.0   # cm
MAX_DEPTH = 150.0  # cm
MAX_DISP = FB / MIN_DEPTH
MIN_DISP = FB / MAX_DEPTH


def gray_to_depth(gray: np.ndarray, in_meters: bool = True) -> np.ndarray:
    """
    Convert grayscale depth frame (0–255) to metric depth.
    gray: uint8 or float in [0, 255], shape (H, W).
    Returns float array; units are meters if in_meters=True, else cm.
    """
    depth = np.asarray(gray, dtype=np.float32)
    if depth.max() <= 1.0:
        depth = depth * 255.0
    disp = depth / 255.0 * (MAX_DISP - MIN_DISP) + MIN_DISP
    np.maximum(disp, 1e-6, out=disp)
    out = FB / disp
    if in_meters:
        out = out / 100.0  # cm -> m
    return out


def depth_in_roi(
    depth_map: np.ndarray,
    bbox: tuple[int, int, int, int],
    valid_min: float = 0.1,
    valid_max: float = 10.0,
    use_median: bool = True,
) -> float | None:
    """
    Compute representative depth inside a bounding box.
    bbox: (x1, y1, x2, y2) in pixel coordinates.
    depth_map: float (H, W) or (H, W, C); same units as valid_min/valid_max (default meters).
    valid_min, valid_max: ignore depth outside this range.
    use_median: if True use median, else mean.
    Returns depth in same units as depth_map, or None if no valid pixels.
    """
    if depth_map.ndim == 3:
        depth_map = depth_map[:, :, 0] if depth_map.shape[2] >= 1 else depth_map.mean(axis=2)
    x1, y1, x2, y2 = bbox
    h, w = depth_map.shape
    x1 = max(0, min(x1, w - 1))
    x2 = max(0, min(x2, w))
    y1 = max(0, min(y1, h - 1))
    y2 = max(0, min(y2, h))
    roi = depth_map[y1:y2, x1:x2]
    mask = (roi >= valid_min) & (roi <= valid_max)
    vals = roi[mask]
    if vals.size == 0:
        return None
    return float(np.median(vals) if use_median else np.mean(vals))
