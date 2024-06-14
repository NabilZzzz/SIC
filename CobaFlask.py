from flask import Flask, request, jsonify

app = Flask(__name__)

dummy_data = [
    {
        "temperature": 25,
        "humidity": 80,
        "timestamp": "2022-11-25 22:10:00"
    },
    {
        "temperature": 26,
        "humidity": 85,
        "timestamp": "2022-11-25 22:10:05"
    },
    {
        "temperature": 27,
        "humidity": 90,
        "timestamp": "2022-11-25 22:10:10"
    }
]

@app.route('/sensor/data', methods=['POST', 'GET'])
def sensor_data():
    try:
        if request.method == 'POST':
            data = request.get_json()
            if not data:
                raise ValueError("No JSON data received")
            dummy_data.append(data)
            print(data)
            return jsonify({'message': 'Data received'})
        if request.method == 'GET':
            return jsonify(dummy_data)
    except Exception as e:
        return jsonify({"error": str(e)})

if __name__ == '__main__':
    app.run(port=5000, debug=True)
