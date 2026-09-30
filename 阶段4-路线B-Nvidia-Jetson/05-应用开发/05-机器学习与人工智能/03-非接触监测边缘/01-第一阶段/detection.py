"""
Real-time object detection for Phase 1 pipeline.
Uses OpenCV built-in face (Haar) and person (HOG) detectors; no extra model files.
"""
from __future__ import annotations

import cv2
import numpy as np
from pathlib import Path

# (x1, y1, x2, y2), label, confidence
Detection = tuple[tuple[int, int, int, int], str, float]


def _find_opencv_data() -> Path | None:
    """Try to locate OpenCV data dir for haarcascades."""
    for base in [Path(cv2.__file__).parent, Path(".")]:
        d = base / "data" / "haarcascade_frontalface_default.xml"
        if d.exists():
            return d.parent
        d = base / "share" / "opencv4" / "haarcascades" / "haarcascade_frontalface_default.xml"
        if d.exists():
            return d.parent
    return None


class FaceDetector:
    """OpenCV Haar cascade face detector (built-in)."""

    def __init__(self, scale_factor: float = 1.1, min_neighbors: int = 5, min_size: tuple[int, int] = (30, 30)):
        self.scale_factor = scale_factor
        self.min_neighbors = min_neighbors
        self.min_size = min_size
        data_dir = _find_opencv_data()
        if data_dir is None:
            raise FileNotFoundError(
                "OpenCV haarcascade not found. Install opencv-python or opencv-contrib-python."
            )
        path = data_dir / "haarcascade_frontalface_default.xml"
        self._cascade = cv2.CascadeClassifier(str(path))
        if self._cascade.empty():
            raise RuntimeError(f"Failed to load cascade from {path}")

    def detect(self, frame: np.ndarray) -> list[Detection]:
        """Return list of (bbox, label, confidence). Bbox = (x1, y1, x2, y2)."""
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) if frame.ndim == 3 else frame
        rects = self._cascade.detectMultiScale(
            gray,
            scaleFactor=self.scale_factor,
            minNeighbors=self.min_neighbors,
            minSize=self.min_size,
        )
        out: list[Detection] = []
        for (x, y, w, h) in rects:
            bbox = (int(x), int(y), int(x + w), int(y + h))
            out.append((bbox, "face", 1.0))
        return out


class PersonDetector:
    """OpenCV HOG person detector (built-in)."""

    def __init__(self, hit_threshold: float = 0.0, win_stride: tuple[int, int] = (8, 8)):
        self._hog = cv2.HOGDescriptor()
        self._hog.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())
        self.hit_threshold = hit_threshold
        self.win_stride = win_stride

    def detect(self, frame: np.ndarray) -> list[Detection]:
        """Return list of (bbox, label, confidence)."""
        rects, weights = self._hog.detectMultiScale(
            frame,
            hitThreshold=self.hit_threshold,
            winStride=self.win_stride,
        )
        out: list[Detection] = []
        for i, (x, y, w, h) in enumerate(rects):
            conf = float(weights[i].ravel()[0]) if weights is not None and len(weights) > i else 0.0
            bbox = (int(x), int(y), int(x + w), int(y + h))
            out.append((bbox, "person", max(0.0, min(1.0, conf))))
        return out


class CompositeDetector:
    """Run face and optionally person detection; optionally suppress person boxes that overlap faces."""

    def __init__(self, detect_face: bool = True, detect_person: bool = True, min_face_confidence: float = 0.0):
        self.detect_face = detect_face
        self.detect_person = detect_person
        self.min_face_confidence = min_face_confidence
        self._face = FaceDetector() if detect_face else None
        self._person = PersonDetector() if detect_person else None

    def detect(self, frame: np.ndarray) -> list[Detection]:
        results: list[Detection] = []
        if self._face:
            results.extend(self._face.detect(frame))
        if self._person:
            persons = self._person.detect(frame)
            if self._face and results:
                # Drop person boxes that heavily overlap a face (avoid duplicate face region)
                face_boxes = [r[0] for r in results if r[1] == "face"]
                for bbox, label, conf in persons:
                    if not _overlaps_any(bbox, face_boxes, iou_threshold=0.3):
                        results.append((bbox, label, conf))
            else:
                results.extend(persons)
        return results


def _overlaps_any(bbox: tuple[int, int, int, int], boxes: list[tuple[int, int, int, int]], iou_threshold: float) -> bool:
    for b in boxes:
        if _iou(bbox, b) >= iou_threshold:
            return True
    return False


def _iou(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> float:
    x1 = max(a[0], b[0])
    y1 = max(a[1], b[1])
    x2 = min(a[2], b[2])
    y2 = min(a[3], b[3])
    if x2 <= x1 or y2 <= y1:
        return 0.0
    inter = (x2 - x1) * (y2 - y1)
    area_a = (a[2] - a[0]) * (a[3] - a[1])
    area_b = (b[2] - b[0]) * (b[3] - b[1])
    union = area_a + area_b - inter
    return inter / union if union > 0 else 0.0
