from flask import Flask, jsonify, request, render_template, Response, send_file, abort
import queue
import time
import cv2
import numpy as np

app = Flask(__name__)

# Queues to store raw and processed video frames
processed_frame_queue = queue.Queue(maxsize=10)

# Default command and speed
current_command = "STOP"
current_speed = 0  # Default speed (0-100%)
auto_mode = False  # Auto mode is disabled
last_command = "STOP"  # In case it stops detecting lines
lost_line_time = None  # Wait before starting recovery sequence
recovery_time = None   # Time under recovery mode
headlights_state = "OFF" # Headlights off

@app.route('/')
def index():
    """Render the web interface."""
    return render_template('index.html', command=current_command, speed=current_speed, auto_mode=auto_mode)

@app.route('/control', methods=['GET'])
def control():
    """Endpoint to send control commands to the ESP32."""
    global current_command
    return jsonify(command=current_command)

@app.route('/set_command', methods=['POST'])
def set_command():
    """Endpoint to update the control command."""
    global current_command
    data = request.json
    if data and 'command' in data:
        new_command = data['command'].upper()
        if new_command in ["FORWARD", "REVERSE", "STOP", "LEFT", "RIGHT"]:
            current_command = new_command
            return jsonify(status="success", command=current_command)
        else:
            return jsonify(status="error", message="Invalid command"), 400
    else:
        return jsonify(status="error", message="No command provided"), 400

@app.route('/set_speed', methods=['POST'])
def set_speed():
    """Endpoint to set the motor speed."""
    global current_speed
    data = request.json
    if data and 'speed' in data:
        new_speed = int(data['speed'])
        if 0 <= new_speed <= 100:
            current_speed = new_speed
            return jsonify(status="success", speed=current_speed)
        else:
            return jsonify(status="error", message="Invalid speed value"), 400
    else:
        return jsonify(status="error", message="No speed provided"), 400

@app.route('/speed', methods=['GET'])
def speed():
    """Endpoint to get the current motor speed."""
    global current_speed
    return jsonify(speed=current_speed)

@app.route('/upload_frame', methods=['POST'])
def upload_frame():
    """Endpoint for the ESP32 to upload video frames."""
    try:
        print("Received request at /upload_frame")
        frame = request.data  # Get the raw frame data
        print(f"Received frame of size: {len(frame)} bytes")

        # Process the frame to detect the line (if auto mode is enabled)
        processed_frame = process_image(frame) if auto_mode else frame

        if processed_frame_queue.full():
            processed_frame_queue.get()  # Discard the oldest frame if the queue is full

        processed_frame_queue.put(processed_frame)
        return jsonify(status="success")
    except Exception as e:
        print(f"Error in upload_frame: {e}")
        return jsonify(status="error", message=str(e)), 500

@app.route('/toggle_auto_mode', methods=['POST'])
def toggle_auto_mode():
    """Endpoint to toggle auto mode on/off."""
    global auto_mode
    data = request.json
    if data and 'auto_mode' in data:
        auto_mode = data['auto_mode']
        return jsonify(status="success", auto_mode=auto_mode)
    else:
        return jsonify(status="error", message="No auto_mode provided"), 400

@app.route('/set_headlights', methods=['POST'])
def set_headlights():
    global headlights_state
    data = request.json
    if data and 'headlights_on' in data:
        cmd = data['headlights_on'].upper()
        if cmd in ["ON", "OFF"]:
            headlights_state = cmd
            return jsonify(status="success", hdcommand=headlights_state)
    return jsonify(status="error", message="Invalid or missing command"), 400

@app.route('/get_headlights', methods=['GET'])
def get_headlights():
    return jsonify(hdcommand=headlights_state)


def process_image(frame_data):
    global current_command
    global lost_line_time
    global recovery_time
    global last_command

    try:
        nparr = np.frombuffer(frame_data, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        if img is None:
            current_command = "STOP"
            return frame_data
    except:
        current_command = "STOP"
        return frame_data

    height, width = img.shape[:2]

    # Region of interest set to the top 40% of the screen
    roi_start = height // 2
    roi_end = int(height * 0.9)
    roi = img[roi_start:roi_end, :]

    # Convert to grayscale
    gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)

    # The following works best for dark linles on a light background.
    # For white lines, remove `inverted` and pass `gray` argument to binary threshold below.
    # Invert image
    inverted = cv2.bitwise_not(gray)

    # Threshold to isolate dark (inverted = bright) lines
    # Change threshold depending on climate
    _, binary = cv2.threshold(inverted, 180, 255, cv2.THRESH_BINARY)

    # Mask center region
    mask = np.zeros_like(binary)
    margin = int(width * 0.3)
    cv2.rectangle(mask, (margin, 0), (width - margin, roi.shape[0]), 255, -1)
    binary = cv2.bitwise_and(binary, mask)

    # reduce noise around edges
    kernel = np.ones((5, 5), np.uint8)
    binary = cv2.erode(binary, kernel, iterations=1)
    binary = cv2.dilate(binary, kernel, iterations=2)

    # Find contours
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    if contours:
        recovery_time = None
        lost_line_time = None
        # Use the largest contour (presumably the line)
        largest = max(contours, key=cv2.contourArea)
        M = cv2.moments(largest)

        if M["m00"] != 0:
            cx = int(M["m10"] / M["m00"])  # Centroid X
            center = width // 2

            # Draw detected line and center line
            cv2.drawContours(roi, [largest], -1, (0, 255, 0), 2)
            cv2.circle(roi, (cx, roi.shape[0] // 2), 5, (255, 0, 0), -1)
            cv2.line(roi, (center, 0), (center, roi.shape[0]), (0, 0, 255), 2)

            # Turn threshold
            offset = cx - center
            if offset < -35:
                current_command = "RIGHT"
            elif offset > 35:
                current_command = "LEFT"
            else:
                current_command = "FORWARD"
            
            last_command = current_command
        # Absolutely stop if no option
        # A curse and a blessing at the same time
        else:
            current_command = "STOP"
    else:
        if lost_line_time is None:
            lost_line_time = time.time()

        # Wait 2 seconds before triggering recovery
        if time.time() - lost_line_time > 1.0:
            current_command = "REVERSE"

            if recovery_time is None:
                recovery_time = time.time()

            # Stop after 2 seconds of recovery
            if time.time() - recovery_time > 2.0:
                current_command = "STOP"
        else:
            current_command = "STOP"

    # Overlay command
    cv2.putText(roi, current_command, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2)

    # Encode output
    img[roi_start:roi_end, :] = roi
    _, jpeg = cv2.imencode('.jpg', img)
    return jpeg.tobytes()


def generate_processed_frames():
    """Generator function to stream processed video frames."""
    while True:
        try:
            if not processed_frame_queue.empty():
                frame = processed_frame_queue.get()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
            else:
                time.sleep(0.1)  # Wait for new frames
        except Exception as e:
            print(f"Error in generate_processed_frames: {e}")
            time.sleep(0.1)


@app.route('/processed_video_feed')
def processed_video_feed():
    """Endpoint to stream processed video frames."""
    return Response(generate_processed_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/view_code/<filename>')
def view_code(filename):
    """Serve a specific project file as plain text."""
    allowed_files = {
        "server": "app.py",
        "website": "./templates/index.html",
        "esp32": "../ESP32/ESP32.ino",
        "avr": "../avrcode/main.asm"
    }

    if filename not in allowed_files:
        return abort(404)

    try:
        return send_file(allowed_files[filename], mimetype='text/plain')
    except Exception as e:
        return f"Error reading file: {e}", 500


if __name__ == '__main__':
    #app.run(host='192.168.18.14', port=4440, debug=True)
    app.run(host='192.168.18.6', port=4440, debug=True)
    #app.run(host='192.168.1.110', port=4440, debug=True)
    #app.run(host='10.210.11.70', port=4440, debug=True)
