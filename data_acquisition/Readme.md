# Key Components
    - dashboard   -> Connect to this over WiFi to visualize data
    - data_queues -> This is where RTOS queues are configured
    - wheel_speed -> Gathers raw sensor data from hall effects sensors and pushes it to a queue

# How to Run
 - Run the bootstrap sheel script which corresponds with your OS. It is expected you have ESP-IDF v5.5.1 installed.
 - For Windows:
 - For Mac:
 - For Linux:
 - If you face issues with this script, check if the path within the script corresponds with the ESP-IDF location on your system
 - You are now ready to build the project