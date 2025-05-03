from flask import Flask, jsonify, request, render_template, Response
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
auto_mode = False  #auto mode is disabled
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

    # Invert image
    inverted = cv2.bitwise_not(gray)

    # Threshold to isolate dark (inverted = bright) lines
    _, binary = cv2.threshold(inverted, 200, 255, cv2.THRESH_BINARY)

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
            if offset < -30:
                current_command = "RIGHT"
            elif offset > 30:
                current_command = "LEFT"
            else:
                current_command = "FORWARD"
        # Absolutely stop if no option
        # A curse and a blessing at the same time
        else:
            current_command = "STOP"
    else:
        current_command = "STOP"

    # Overlay command
    cv2.putText(roi, current_command, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2)

    # Encode output
    img[roi_start:roi_end, :] = roi
    _, jpeg = cv2.imencode('.jpg', img)
    return jpeg.tobytes()



""" def process_image(frame_data):
    #Process the image to detect the line and update the command.
    global current_command

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
    roi = img[height//2:, :]
    
    # Filter for white and detect edges
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    #lower_white = np.array([0, 0, 200])
    #upper_white = np.array([180, 50, 255])
    #mask = cv2.inRange(hsv, lower_white, upper_white)
    blurred = cv2.GaussianBlur(hsv, (9, 9), 0)
    edges = cv2.Canny(blurred, 100, 150)
    
    # Detect lines
    lines = cv2.HoughLinesP(edges, 1, np.pi/180, threshold=35, minLineLength=5, maxLineGap=10)
    
    if lines is not None:

# old line tracking logic
#        x1, y1, x2, y2 = lines[0][0]
#
#        # Draw line on image (optional)
#        cv2.line(img, (x1, y1), (x2, y2), (0, 255, 0), 2)
#        
#        # Simple steering logic
#        center = img.shape[1] // 2
#        line_center = (x1 + x2) // 2
#        
#        if line_center > center + 50:
#            current_command = "RIGHT"
#        elif line_center < center - 50:
#            current_command = "LEFT"
#        else:
#            current_command = "FORWARD"
        all_x = []
        for line in lines:
            x1, y1, x2, y2 = line[0]
            all_x.extend([x1, x2])

            cv2.line(img, (x1, y1), (x2, y2), (0, 255, 0), 2)  # Green 

           # cv2.line(img, (x1, height + y1),
           #          (x2, height + y2),
           #          (0, 255, 0), 1)

        avg_x = np.mean(all_x)
        center = width // 2
        
        if avg_x > center + 35:
            current_command = "LEFT"
        elif avg_x < center - 35: 
            current_command = "RIGHT"
        else:
            current_command = "FORWARD"


        cv2.line(img, (center, height), (center, height//2), (0,0,255), 1)  # Center line
        cv2.putText(img, current_command, (width-100, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0,0,255), 2)
   
   # Return original image with detected line drawn
    _, jpeg = cv2.imencode('.jpg', img)
    return jpeg.tobytes() """

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

if __name__ == '__main__':
    #app.run(host='192.168.18.14', port=4440, debug=True)
    app.run(host='192.168.18.6', port=4440, debug=True)
    #app.run(host='192.168.1.110', port=4440, debug=True)
    #app.run(host='10.210.11.70', port=4440, debug=True)
