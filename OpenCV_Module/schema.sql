CREATE TABLE IF NOT EXISTS eye_monitor_records
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,

    timestamp TEXT NOT NULL,
    eye_use_period TEXT,
    continuous_use_seconds INTEGER DEFAULT 0,

    eye_side TEXT,
    eye_image_path TEXT,
    face_image_path TEXT,

    eye_pos_x REAL,
    eye_pos_y REAL,

    gaze_x REAL,
    gaze_y REAL,

    eye_screen_distance_cm REAL,
    gaze_angle_deg REAL,

    pitch_deg REAL,
    roll_deg REAL,
    yaw_deg REAL
);
