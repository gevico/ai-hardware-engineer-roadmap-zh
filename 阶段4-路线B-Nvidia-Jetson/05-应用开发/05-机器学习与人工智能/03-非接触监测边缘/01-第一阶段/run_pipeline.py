"""
Phase 1 pipeline: camera calibration + real-time object detection with depth.
Uses camera01 RGB and depth from Free-Viewpoint RGB-D Video Dataset.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import cv2
import numpy as np

# Add phase1 so imports work when run from repo root or from phase1/
_phase1_dir = Path(__file__).resolve().parent
if str(_phase1_dir) not in sys.path:
    sys.path.insert(0, str(_phase1_dir))

from calibration import load_paras_txt, get_camera, get_intrinsics
from depth_utils import gray_to_depth, depth_in_roi
from detection import CompositeDetector, Detection


def parse_args():
    parser = argparse.ArgumentParser(
        description="Phase 1: calibration + object detection with depth (camera01)."
    )
    parser.add_argument(
        "--dataset-dir",
        type=Path,
        default=_phase1_dir.parent / "Free-Viewpoint-RGB-D-Video-Dataset-main",
        help="Path to Free-Viewpoint-RGB-D-Video-Dataset-main (contains camera01-*.mp4 and Camera Parameters)",
    )
    parser.add_argument(
        "--camera-index",
        type=int,
        default=0,
        help="Camera index in paras.txt (0 = camera01)",
    )
    parser.add_argument(
        "--no-person",
        action="store_true",
        help="Disable person (HOG) detection; only face",
    )
    parser.add_argument(
        "--no-display",
        action="store_true",
        help="Run without opening display (e.g. headless); still process and print stats",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Optional: write output video path (e.g. phase1_out.mp4)",
    )
    parser.add_argument(
        "--max-frames",
        type=int,
        default=0,
        help="Max frames to process (0 = all)",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    dataset_dir = Path(args.dataset_dir)
    paras_path = dataset_dir / "Camera Parameters" / "paras.txt"
    rgb_path = dataset_dir / "camera01-rgb.mp4"
    depth_path = dataset_dir / "camera01-depth.mp4"

    if not paras_path.exists():
        print(f"Error: calibration not found at {paras_path}")
        sys.exit(1)
    if not rgb_path.exists():
        print(f"Error: RGB video not found at {rgb_path}")
        sys.exit(1)
    if not depth_path.exists():
        print(f"Error: depth video not found at {depth_path}")
        sys.exit(1)

    # Load calibration (camera01 = index 0)
    cameras = load_paras_txt(paras_path)
    camera = get_camera(cameras, args.camera_index)
    K, width, height = get_intrinsics(camera)
    print(f"Loaded camera{args.camera_index + 1:02d}: {width}x{height}")
    print(f"K:\n{K}")

    # Open videos
    cap_rgb = cv2.VideoCapture(str(rgb_path))
    cap_depth = cv2.VideoCapture(str(depth_path))
    if not cap_rgb.isOpened():
        print(f"Error: could not open {rgb_path}")
        sys.exit(1)
    if not cap_depth.isOpened():
        print(f"Error: could not open {depth_path}")
        sys.exit(1)

    fps = cap_rgb.get(cv2.CAP_PROP_FPS) or 30.0
    total_frames = int(cap_rgb.get(cv2.CAP_PROP_FRAME_COUNT))
    print(f"RGB/Depth: {total_frames} frames @ {fps:.1f} fps")

    # Detector
    detector = CompositeDetector(detect_face=True, detect_person=not args.no_person)

    # Optional writer
    out_writer = None
    if args.out:
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        out_writer = cv2.VideoWriter(str(args.out), fourcc, fps, (width * 2, height))

    frame_idx = 0
    try:
        while True:
            ret_rgb, frame_rgb = cap_rgb.read()
            ret_dep, frame_depth_gray = cap_depth.read()
            if not ret_rgb or not ret_dep:
                break
            if args.max_frames and frame_idx >= args.max_frames:
                break

            # Convert depth to metric (meters); ensure 2D grayscale
            if frame_depth_gray.ndim == 3:
                frame_depth_gray = cv2.cvtColor(frame_depth_gray, cv2.COLOR_BGR2GRAY)
            depth_map = gray_to_depth(frame_depth_gray, in_meters=True)

            # Detect
            detections: list[Detection] = detector.detect(frame_rgb)

            # Depth per detection
            vis = frame_rgb.copy()
            for bbox, label, conf in detections:
                x1, y1, x2, y2 = bbox
                d = depth_in_roi(depth_map, bbox, valid_min=0.3, valid_max=8.0)
                depth_str = f"{d:.2f}m" if d is not None else "N/A"
                color = (0, 255, 0) if label == "face" else (255, 165, 0)
                cv2.rectangle(vis, (x1, y1), (x2, y2), color, 2)
                cv2.putText(
                    vis, f"{label} {depth_str}",
                    (x1, y1 - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2,
                )

            # Depth overlay (colormap) for visualization
            depth_vis = depth_map.copy()
            depth_vis = np.clip(depth_vis, 0.3, 6.0)
            depth_vis = (depth_vis - 0.3) / (6.0 - 0.3) * 255
            depth_vis = np.uint8(depth_vis)
            depth_vis = cv2.applyColorMap(depth_vis, cv2.COLORMAP_INFERNO)

            combined = np.hstack([vis, depth_vis])
            if out_writer:
                out_writer.write(combined)

            if not args.no_display:
                cv2.imshow("Phase1: RGB + Depth", combined)
                key = cv2.waitKey(1) & 0xFF
                if key == ord("q"):
                    break

            frame_idx += 1
            if frame_idx % 100 == 0:
                print(f"Frame {frame_idx}/{total_frames}  detections={len(detections)}")

    finally:
        cap_rgb.release()
        cap_depth.release()
        if out_writer:
            out_writer.release()
        if not args.no_display:
            cv2.destroyAllWindows()

    print(f"Done. Processed {frame_idx} frames.")


if __name__ == "__main__":
    main()
