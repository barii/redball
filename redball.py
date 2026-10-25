import socket
import cv2
from ultralytics import YOLO

ESP32 = socket.gethostbyname("redball.local")  # or put the ESP32-CAM's IP here
CMD_PORT = 4210
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def drive(cmd, speed=None):
    msg = cmd if speed is None else f"{cmd} {speed:.2f}"
    sock.sendto(msg.encode(), (ESP32, CMD_PORT))
    print(msg)
    return msg

model = YOLO("yolo11n.pt")
CUP = 41  # COCO class id for "cup"
cap = cv2.VideoCapture(f"http://{ESP32}:81/stream")
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
    cv2.putText(frame, status, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
    cv2.imshow("redball", frame)
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

cap.release()
cv2.destroyAllWindows()
