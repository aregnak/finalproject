from flask import Flask, jsonify, request, render_template, Response
import queue
import time

app = Flask(__name__)

# Queue to store video frames
frame_queue = queue.Queue(maxsize=10)

# Default command and speed
current_command = "STOP"
current_speed = 0  # Default speed (0-100%)

@app.route('/')
def index():
    """Render the web interface."""
    return render_template('index.html')

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
        if frame_queue.full():
            frame_queue.get()  # Discard the oldest frame if the queue is full
        frame_queue.put(frame)
        return jsonify(status="success")
    except Exception as e:
        print(f"Error in upload_frame: {e}")
        return jsonify(status="error", message=str(e)), 500

def generate_frames():
    """Generator function to stream video frames."""
    while True:
        try:
            if not frame_queue.empty():
                frame = frame_queue.get()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
            else:
                time.sleep(0.1)  # Wait for new frames
        except Exception as e:
            print(f"Error in generate_frames: {e}")
            time.sleep(0.1)

@app.route('/video_feed')
def video_feed():
    """Endpoint to stream video frames."""
    print("Received request at /video_feed")  # Debug statement
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    app.run(host='192.168.1.110', port=4444, debug=True)
