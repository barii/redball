import cv2
from ultralytics import YOLO

def drive(cmd, speed=None):
    # placeholder: replace with real motor control
    print(cmd if speed is None else f"{cmd} {speed:.2f}")
    return cmd if speed is None else f"{cmd} {speed:.2f}"

model = YOLO("yolo11n.pt")
CUP = 44  # COCO class id for "cup"
CAMERA = 0  # 0 is the iPhone Continuity Camera when it's connected
cap = cv2.VideoCapture(CAMERA)
KP, DEADBAND, MIN_CONF = 0.005, 30, 0.4
CLOSE_FRAC = 0.13  # stop when the mug fills this fraction of the frame

while True:
    ok, frame = cap.read()
    if not ok:
        continue
    h, w = frame.shape[:2]
    boxes = model(frame, classes=[CUP], conf=MIN_CONF, verbose=False)[0].boxes

    if not len(boxes):
        status = drive("search")
    else:
        x1, y1, x2, y2 = max(boxes.xyxy.tolist(), key=lambda b: (b[2] - b[0]) * (b[3] - b[1]))
        frac = (x2 - x1) * (y2 - y1) / (w * h)
        err = (x1 + x2) / 2 - w / 2
        if frac > CLOSE_FRAC:      status = drive("stop")
        elif abs(err) < DEADBAND:  status = drive("forward")
        elif err < 0:              status = drive("left",  min(-err * KP, 1.0))
        else:                      status = drive("right", min(err * KP, 1.0))
        cv2.rectangle(frame, (int(x1), int(y1)), (int(x2), int(y2)), (0, 255, 0), 4)
        status += f"  ({frac:.0%} of frame)"

    cv2.line(frame, (w // 2, 0), (w // 2, h), (255, 255, 255), 1)
    cv2.putText(frame, status, (30, 70), cv2.FONT_HERSHEY_SIMPLEX, 2, (0, 255, 0), 4)
    cv2.imshow("redball", frame)
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

cap.release()
cv2.destroyAllWindows()
