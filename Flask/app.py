from flask import Flask, jsonify, request, render_template, Response
import queue
import time
import cv2
import numpy as np

app = Flask(__name__)

# Queues to store raw and processed video frames
raw_frame_queue = queue.Queue(maxsize=10)
processed_frame_queue = queue.Queue(maxsize=10)

# Default command and speed
current_command = "STOP"
current_speed = 0  # Default speed (0-100%)

# Auto mode state
auto_mode = False  # Default: auto mode is disabled

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

        if raw_frame_queue.full():
            raw_frame_queue.get()  # Discard the oldest frame if the queue is full
        if processed_frame_queue.full():
            processed_frame_queue.get()  # Discard the oldest frame if the queue is full

        raw_frame_queue.put(frame)
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

def process_image(frame_data):
    """Process the image to detect the line and update the command."""
    global current_command

    # Convert the frame data to a NumPy array
    nparr = np.frombuffer(frame_data, np.uint8)
    image = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

    # Convert to grayscale
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

    # Apply Gaussian blur
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)

    # Apply binary thresholding
    #_, binary = cv2.threshold(blurred, 127, 255, cv2.THRESH_BINARY_INV)

    binary = cv2.adaptiveThreshold(
            blurred, 255,
            cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
            cv2.THRESH_BINARY_INV, 11, 2)


    # Detect edges
    edges = cv2.Canny(binary, 50, 150)

    # Detect lines using Hough Transform
    # lines = cv2.HoughLinesP(edges, 1, np.pi/180, threshold=50, minLineLength=5, maxLineGap=10)

    # New function allowing for better angle filtering
    valid_lines = []
    for line in lines:
        x1, y1, x2, y2 = line[0]
        angle = np.arctan2(y2 - y1, x2 - x1) * 180 / np.pi
        if abs(angle) < 15: 
            valid_lines.append(line)

    if lines is not None:
        # Calculate error (deviation from center)
        if lines is not None:
            avg_x1 = np.mean([line[0][0] for line in lines])
            avg_x2 = np.mean([line[0][2] for line in lines])
            line_center = (avg_x1 + avg_x2) // 2
        #line = lines[0][0]  # Take the first detected line
            line_center = (line[0] + line[2]) // 2
            image_center = image.shape[1] // 2
            error = line_center - image_center

        # Determine the command based on the error
        if error > 50: 
            current_command = "RIGHT"
        elif error < -50:  
            current_command = "LEFT"
        else:
            current_command = "FORWARD"

        # Draw the detected line on the image
        for line in lines:
            x1, y1, x2, y2 = line[0]
            cv2.line(image, (x1, y1), (x2, y2), (0, 255, 0), 2)
    else:
        current_command = "STOP"  # No line detected

    # Encode the modified image as JPEG
    _, jpeg = cv2.imencode('.jpg', image)
    return jpeg.tobytes()

def generate_raw_frames():
    """Generator function to stream raw video frames."""
    while True:
        try:
            if not raw_frame_queue.empty():
                frame = raw_frame_queue.get()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
            else:
                time.sleep(0.1)  # Wait for new frames
        except Exception as e:
            print(f"Error in generate_raw_frames: {e}")
            time.sleep(0.1)

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

@app.route('/raw_video_feed')
def raw_video_feed():
    """Endpoint to stream raw video frames."""
    return Response(generate_raw_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/processed_video_feed')
def processed_video_feed():
    """Endpoint to stream processed video frames."""
    return Response(generate_processed_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    app.run(host='192.168.18.14', port=4440, debug=True)
   #app.run(host='192.168.1.110', port=4440, debug=True)
