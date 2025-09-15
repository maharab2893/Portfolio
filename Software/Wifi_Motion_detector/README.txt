Wi-Fi Motion Detection Script - Instructions
===========================================

1. Python Installation
----------------------
- Make sure Python 3.8 or higher is installed.
- Download Python from: https://www.python.org/downloads/
- During installation, check "Add Python to PATH".

2. Required Python Package
--------------------------
- This script requires the 'matplotlib' & 'pandas' library.
- Open Command Prompt (Windows) or Terminal (Linux/macOS) and run:

    pip install matplotlib pandas

3. Running the Script
---------------------
- Copy the Python file (wifi_motion_oop.py) to a folder on your PC.
- Open Command Prompt (Windows) or Terminal.
- Navigate to the folder containing the script. Example:

    cd C:\Users\YourName\Desktop\WiFiScript

- Run the script:

    python <FILE NAME>.py

- A live graph will appear showing Wi-Fi ping delays.
- If ping fails, it will show "Ping failed" in the console.

4. Stopping the Script
----------------------
- Press Ctrl + C in the Command Prompt / Terminal to stop the script safely.

5. Customization
----------------
- Change the target device IP by editing the line:

    TARGET_IP = "192.168.0.xxx"

- Change alert threshold (ms) by editing:

    THRESHOLD = 10

6. Notes
--------
- Make sure the target device is connected to the same network.
- This script works on Windows, Linux, and macOS.
